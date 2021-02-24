// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/lacros/lacros_chrome_service_impl.h"

#include <atomic>
#include <utility>

#include "base/bind_post_task.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "build/chromeos_buildflags.h"
#include "chromeos/crosapi/cpp/crosapi_constants.h"
#include "chromeos/crosapi/mojom/crosapi.mojom.h"
#include "chromeos/lacros/lacros_chrome_service_delegate.h"
#include "chromeos/startup/startup.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "mojo/public/cpp/system/invitation.h"
#include "url/gurl.h"

namespace chromeos {
namespace {

using Crosapi = crosapi::mojom::Crosapi;

// Tests will set this to |true| which will make all crosapi functionality
// unavailable.
bool g_disable_all_crosapi_for_tests = false;

// We use a std::atomic here rather than a base::NoDestructor because we want to
// allow instances of LacrosChromeServiceImpl to be destroyed to facilitate
// testing.
std::atomic<LacrosChromeServiceImpl*> g_instance = {nullptr};

crosapi::mojom::BrowserInfoPtr ToMojo(const std::string& browser_version) {
  auto info = crosapi::mojom::BrowserInfo::New();
  info->browser_version = browser_version;
  return info;
}

// Reads and parses the startup data to BrowserInitParams.
// If data is missing, or failed to parse, returns a null StructPtr.
crosapi::mojom::BrowserInitParamsPtr ReadStartupBrowserInitParams() {
  base::Optional<std::string> content = ReadStartupData();
  if (!content)
    return {};

  crosapi::mojom::BrowserInitParamsPtr result;
  if (!crosapi::mojom::BrowserInitParams::Deserialize(
          content->data(), content->size(), &result)) {
    LOG(ERROR) << "Failed to parse startup data";
    return {};
  }

  return result;
}

}  // namespace

// This class that holds all state that is affine to a single, never-blocking
// sequence. The sequence must be never-blocking to avoid deadlocks, see
// https://crbug.com/1103765.
class LacrosChromeServiceNeverBlockingState
    : public crosapi::mojom::BrowserService {
 public:
  LacrosChromeServiceNeverBlockingState(
      scoped_refptr<base::SequencedTaskRunner> owner_sequence,
      base::WeakPtr<LacrosChromeServiceImpl> owner,
      crosapi::mojom::BrowserInitParamsPtr* init_params)
      : owner_sequence_(owner_sequence),
        owner_(owner),
        init_params_(init_params) {
    DETACH_FROM_SEQUENCE(sequence_checker_);
  }
  ~LacrosChromeServiceNeverBlockingState() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  }

  // crosapi::mojom::BrowserService:
  void InitDeprecated(crosapi::mojom::BrowserInitParamsPtr params) override {
    if (init_params_)
      *init_params_ = std::move(params);
    initialized_.Signal();
  }

  void RequestCrosapiReceiver(
      RequestCrosapiReceiverCallback callback) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    // TODO(hidehiko): Remove non-error logging from here.
    LOG(WARNING) << "CrosapiReceiver requested.";
    std::move(callback).Run(std::move(pending_crosapi_receiver_));
  }

  void NewWindow(NewWindowCallback callback) override {
    owner_sequence_->PostTaskAndReply(
        FROM_HERE,
        base::BindOnce(&LacrosChromeServiceImpl::NewWindowAffineSequence,
                       owner_),
        std::move(callback));
  }

  void GetFeedbackData(GetFeedbackDataCallback callback) override {
    owner_sequence_->PostTask(
        FROM_HERE,
        base::BindOnce(
            &LacrosChromeServiceImpl::GetFeedbackDataAffineSequence, owner_,
            base::BindPostTask(base::SequencedTaskRunnerHandle::Get(),
                               std::move(callback))));
  }

  void GetHistograms(GetHistogramsCallback callback) override {
    owner_sequence_->PostTask(
        FROM_HERE,
        base::BindOnce(
            &LacrosChromeServiceImpl::GetHistogramsAffineSequence, owner_,
            base::BindPostTask(base::SequencedTaskRunnerHandle::Get(),
                               std::move(callback))));
  }

  void GetActiveTabUrl(GetActiveTabUrlCallback callback) override {
    owner_sequence_->PostTask(
        FROM_HERE,
        base::BindOnce(
            &LacrosChromeServiceImpl::GetActiveTabUrlAffineSequence, owner_,
            base::BindPostTask(base::SequencedTaskRunnerHandle::Get(),
                               std::move(callback))));
  }

  // Unlike most of other methods of this class, this is called on the
  // affined thread. Specifically, it is intended to be called before starting
  // the message pumping of the affined thread to pass the initialization
  // parameter from ash-chrome needed for the procedure running before the
  // message pumping.
  void WaitForInit() { initialized_.Wait(); }

  // Crosapi is the interface that lacros-chrome uses to message
  // ash-chrome. This method binds the remote, which allows queuing of message
  // to ash-chrome. The messages will not go through until
  // RequestCrosapiReceiver() is invoked.
  void BindCrosapi() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    pending_crosapi_receiver_ = crosapi_.BindNewPipeAndPassReceiver();
  }

  // BrowserService is the interface that ash-chrome uses to message
  // lacros-chrome. This handles and routes all incoming messages from
  // ash-chrome.
  void BindBrowserServiceReceiver(
      mojo::PendingReceiver<crosapi::mojom::BrowserService> receiver) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    receiver_.Bind(std::move(receiver));
  }

  void FusePipeCrosapi(
      mojo::PendingRemote<crosapi::mojom::Crosapi> pending_remote) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    mojo::FusePipes(std::move(pending_crosapi_receiver_),
                    std::move(pending_remote));
    crosapi_->BindBrowserServiceHost(
        browser_service_host_.BindNewPipeAndPassReceiver());
    browser_service_host_->AddBrowserService(
        receiver_.BindNewPipeAndPassRemote());
  }

  // These methods pass the receiver end of a mojo message pipe to ash-chrome.
  // This effectively allows ash-chrome to receive messages sent on these
  // message pipes.
  void BindMessageCenterReceiver(
      mojo::PendingReceiver<crosapi::mojom::MessageCenter> pending_receiver) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    crosapi_->BindMessageCenter(std::move(pending_receiver));
  }

  void BindSelectFileReceiver(
      mojo::PendingReceiver<crosapi::mojom::SelectFile> pending_receiver) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    crosapi_->BindSelectFile(std::move(pending_receiver));
  }

  void BindHidManagerReceiver(
      mojo::PendingReceiver<device::mojom::HidManager> pending_receiver) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    crosapi_->BindHidManager(std::move(pending_receiver));
  }

  void BindScreenManagerReceiver(
      mojo::PendingReceiver<crosapi::mojom::ScreenManager> pending_receiver) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    crosapi_->BindScreenManager(std::move(pending_receiver));
  }

  void BindKeystoreServiceReceiver(
      mojo::PendingReceiver<crosapi::mojom::KeystoreService> pending_receiver) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    crosapi_->BindKeystoreService(std::move(pending_receiver));
  }

  void BindFeedbackReceiver(
      mojo::PendingReceiver<crosapi::mojom::Feedback> pending_receiver) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    crosapi_->BindFeedback(std::move(pending_receiver));
  }

  void BindCertDbReceiver(
      mojo::PendingReceiver<crosapi::mojom::CertDatabase> pending_receiver) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    crosapi_->BindCertDatabase(std::move(pending_receiver));
  }

  void BindDeviceAttributesReceiver(
      mojo::PendingReceiver<crosapi::mojom::DeviceAttributes>
          pending_receiver) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    crosapi_->BindDeviceAttributes(std::move(pending_receiver));
  }

  void OnBrowserStartup(crosapi::mojom::BrowserInfoPtr browser_info) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    crosapi_->OnBrowserStartup(std::move(browser_info));
  }

  void BindAccountManagerReceiver(
      mojo::PendingReceiver<crosapi::mojom::AccountManager> pending_receiver) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DVLOG(1) << "Binding AccountManager";
    crosapi_->BindAccountManager(std::move(pending_receiver));
  }

  void BindFileManagerReceiver(
      mojo::PendingReceiver<crosapi::mojom::FileManager> pending_receiver) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    crosapi_->BindFileManager(std::move(pending_receiver));
  }

  void BindClipboardReceiver(
      mojo::PendingReceiver<crosapi::mojom::Clipboard> pending_receiver) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    crosapi_->BindClipboard(std::move(pending_receiver));
  }

  void BindMediaSessionAudioFocusReceiver(
      mojo::PendingReceiver<media_session::mojom::AudioFocusManager>
          pending_receiver) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    crosapi_->BindMediaSessionAudioFocus(std::move(pending_receiver));
  }

  void BindMediaSessionAudioFocusDebugReceiver(
      mojo::PendingReceiver<media_session::mojom::AudioFocusManagerDebug>
          pending_receiver) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    crosapi_->BindMediaSessionAudioFocusDebug(std::move(pending_receiver));
  }

  void BindMediaSessionControllerReceiver(
      mojo::PendingReceiver<media_session::mojom::MediaControllerManager>
          pending_receiver) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    crosapi_->BindMediaSessionController(std::move(pending_receiver));
  }

  void BindMetricsReportingReceiver(
      mojo::PendingReceiver<crosapi::mojom::MetricsReporting> receiver) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    crosapi_->BindMetricsReporting(std::move(receiver));
  }

  void BindSensorHalClientRemote(
      mojo::PendingRemote<chromeos::sensors::mojom::SensorHalClient>
          pending_remote) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    crosapi_->BindSensorHalClient(std::move(pending_remote));
  }

  void BindPrefsReceiver(
      mojo::PendingReceiver<crosapi::mojom::Prefs> pending_receiver) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    crosapi_->BindPrefs(std::move(pending_receiver));
  }

  void BindTestControllerReceiver(
      mojo::PendingReceiver<crosapi::mojom::TestController> pending_receiver) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    crosapi_->BindTestController(std::move(pending_receiver));
  }

  void BindUrlHandlerReceiver(
      mojo::PendingReceiver<crosapi::mojom::UrlHandler> pending_receiver) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    crosapi_->BindUrlHandler(std::move(pending_receiver));
  }

  base::WeakPtr<LacrosChromeServiceNeverBlockingState> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  // Receives and routes messages from ash-chrome.
  mojo::Receiver<crosapi::mojom::BrowserService> receiver_{this};

  // This remote allows lacros-chrome to send messages to ash-chrome.
  mojo::Remote<crosapi::mojom::Crosapi> crosapi_;

  mojo::Remote<crosapi::mojom::BrowserServiceHost> browser_service_host_;

  // This class holds onto the receiver for Crosapi until ash-chrome
  // is ready to bind it.
  mojo::PendingReceiver<crosapi::mojom::Crosapi> pending_crosapi_receiver_;

  // This allows LacrosChromeServiceNeverBlockingState to route IPC messages
  // back to the affine thread on LacrosChromeServiceImpl. |owner_| is affine to
  // |owner_sequence_|.
  scoped_refptr<base::SequencedTaskRunner> owner_sequence_;
  base::WeakPtr<LacrosChromeServiceImpl> owner_;

  // Owned by LacrosChromeServiceImpl.
  crosapi::mojom::BrowserInitParamsPtr* const init_params_;

  // Lock to wait for InitDeprecated() invocation.
  // Because the parameters are needed before starting the affined thread's
  // message pumping, it is necessary to use sync primitive here, instead.
  base::WaitableEvent initialized_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<LacrosChromeServiceNeverBlockingState> weak_factory_{
      this};
};

// static
LacrosChromeServiceImpl* LacrosChromeServiceImpl::Get() {
  return g_instance;
}

LacrosChromeServiceImpl::LacrosChromeServiceImpl(
    std::unique_ptr<LacrosChromeServiceDelegate> delegate)
    : delegate_(std::move(delegate)),
      sequenced_state_(nullptr, base::OnTaskRunnerDeleter(nullptr)) {
  if (g_disable_all_crosapi_for_tests) {
    // Tests don't call BrowserService::InitDeprecated(), so provide
    // BrowserInitParams with default values.
    init_params_ = crosapi::mojom::BrowserInitParams::New();
  } else {
    // Try to read the startup data. If ash-chrome is too old, the data
    // may not available, then fallback to the older approach.
    init_params_ = ReadStartupBrowserInitParams();

    // Short term workaround: if --crosapi-mojo-platform-channel-handle is
    // available, close --mojo-platform-channel-handle, and remove it
    // from command line. It is for backward compatibility support by
    // ash-chrome.
    // TODO(crbug.com/1180712): Remove this, when ash-chrome stops to support
    // legacy invitation flow.
    auto* command_line = base::CommandLine::ForCurrentProcess();
    if (command_line->HasSwitch(crosapi::kCrosapiMojoPlatformChannelHandle) &&
        command_line->HasSwitch(mojo::PlatformChannel::kHandleSwitch)) {
      std::ignore = mojo::PlatformChannel::RecoverPassedEndpointFromCommandLine(
          *command_line);
      command_line->RemoveSwitch(mojo::PlatformChannel::kHandleSwitch);
    }
  }

  // The sequence on which this object was constructed, and thus affine to.
  scoped_refptr<base::SequencedTaskRunner> affine_sequence =
      base::SequencedTaskRunnerHandle::Get();

  never_blocking_sequence_ = base::ThreadPool::CreateSequencedTaskRunner(
      {base::TaskPriority::USER_BLOCKING,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN});

  sequenced_state_ = std::unique_ptr<LacrosChromeServiceNeverBlockingState,
                                     base::OnTaskRunnerDeleter>(
      new LacrosChromeServiceNeverBlockingState(
          affine_sequence, weak_factory_.GetWeakPtr(),
          init_params_.is_null() ? &init_params_ : nullptr),
      base::OnTaskRunnerDeleter(never_blocking_sequence_));
  weak_sequenced_state_ = sequenced_state_->GetWeakPtr();

  never_blocking_sequence_->PostTask(
      FROM_HERE,
      base::BindOnce(&LacrosChromeServiceNeverBlockingState::BindCrosapi,
                     weak_sequenced_state_));

  DCHECK(!g_instance);
  g_instance = this;
}

LacrosChromeServiceImpl::~LacrosChromeServiceImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(affine_sequence_checker_);
  DCHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

void LacrosChromeServiceImpl::BindReceiver(
    mojo::PendingReceiver<crosapi::mojom::BrowserService> receiver) {
  if (receiver.is_valid()) {
    // This is legacy invitation flow.
    // TODO(crbug.com/1180712): Remove this after all base ash-chrome is new
    // enough supporting new invitation flow.
    never_blocking_sequence_->PostTask(
        FROM_HERE,
        base::BindOnce(
            &LacrosChromeServiceNeverBlockingState::BindBrowserServiceReceiver,
            weak_sequenced_state_, std::move(receiver)));

    // If ash-chrome is too old, BrowserInitParams may not be passed from
    // a memory backed file directly. Then, try to wait for InitDeprecated()
    // invocation for backward compatibility.
    if (!init_params_)
      sequenced_state_->WaitForInit();
  } else {
    // Accept Crosapi invitation here. Mojo IPC support should be initialized
    // at this stage.
    auto* command_line = base::CommandLine::ForCurrentProcess();

    // In unittests/browser_tests cases, the mojo pipe may not be set up.
    // Just ignore the case.
    if (!command_line->HasSwitch(crosapi::kCrosapiMojoPlatformChannelHandle))
      return;

    mojo::PlatformChannelEndpoint endpoint =
        mojo::PlatformChannel::RecoverPassedEndpointFromString(
            command_line->GetSwitchValueASCII(
                crosapi::kCrosapiMojoPlatformChannelHandle));
    auto invitation = mojo::IncomingInvitation::Accept(std::move(endpoint));
    never_blocking_sequence_->PostTask(
        FROM_HERE,
        base::BindOnce(&LacrosChromeServiceNeverBlockingState::FusePipeCrosapi,
                       weak_sequenced_state_,
                       mojo::PendingRemote<crosapi::mojom::Crosapi>(
                           invitation.ExtractMessagePipe(0), /*version=*/0)));

    // In this case, ash-chrome should be new enough, so init params should be
    // passed from the startup outband file descriptor.
  }

  // In any case, |init_params_| should be initialized to a valid instance
  // at this point.
  DCHECK(init_params_);

  delegate_->OnInitialized(*init_params_);
  did_bind_receiver_ = true;

  if (IsCertDbAvailable()) {
    never_blocking_sequence_->PostTask(
        FROM_HERE,
        base::BindOnce(
            &LacrosChromeServiceNeverBlockingState::BindCertDbReceiver,
            weak_sequenced_state_,
            cert_database_remote_.BindNewPipeAndPassReceiver()));
  }

  if (IsClipboardAvailable()) {
    mojo::PendingReceiver<crosapi::mojom::Clipboard> pending_receiver =
        clipboard_remote_.BindNewPipeAndPassReceiver();
    never_blocking_sequence_->PostTask(
        FROM_HERE,
        base::BindOnce(
            &LacrosChromeServiceNeverBlockingState::BindClipboardReceiver,
            weak_sequenced_state_, std::move(pending_receiver)));
  }

  if (IsDeviceAttributesAvailable()) {
    never_blocking_sequence_->PostTask(
        FROM_HERE,
        base::BindOnce(&LacrosChromeServiceNeverBlockingState::
                           BindDeviceAttributesReceiver,
                       weak_sequenced_state_,
                       device_attributes_remote_.BindNewPipeAndPassReceiver()));
  }

  if (IsFeedbackAvailable()) {
    never_blocking_sequence_->PostTask(
        FROM_HERE,
        base::BindOnce(
            &LacrosChromeServiceNeverBlockingState::BindFeedbackReceiver,
            weak_sequenced_state_,
            feedback_remote_.BindNewPipeAndPassReceiver()));
  }

  if (IsFileManagerAvailable()) {
    mojo::PendingReceiver<crosapi::mojom::FileManager> pending_receiver =
        file_manager_remote_.BindNewPipeAndPassReceiver();
    never_blocking_sequence_->PostTask(
        FROM_HERE,
        base::BindOnce(
            &LacrosChromeServiceNeverBlockingState::BindFileManagerReceiver,
            weak_sequenced_state_, std::move(pending_receiver)));
  }

  if (IsHidManagerAvailable()) {
    mojo::PendingReceiver<device::mojom::HidManager>
        hid_manager_pending_receiver =
            hid_manager_remote_.BindNewPipeAndPassReceiver();
    never_blocking_sequence_->PostTask(
        FROM_HERE,
        base::BindOnce(
            &LacrosChromeServiceNeverBlockingState::BindHidManagerReceiver,
            weak_sequenced_state_, std::move(hid_manager_pending_receiver)));
  }

  if (IsKeystoreServiceAvailable()) {
    mojo::PendingReceiver<crosapi::mojom::KeystoreService>
        keystore_service_pending_receiver =
            keystore_service_remote_.BindNewPipeAndPassReceiver();
    never_blocking_sequence_->PostTask(
        FROM_HERE,
        base::BindOnce(
            &LacrosChromeServiceNeverBlockingState::BindKeystoreServiceReceiver,
            weak_sequenced_state_,
            std::move(keystore_service_pending_receiver)));
  }

  // Bind the remote for MessageCenter on the current thread, and then pass the
  // receiver to the never_blocking_sequence_.
  if (IsMessageCenterAvailable()) {
    never_blocking_sequence_->PostTask(
        FROM_HERE,
        base::BindOnce(
            &LacrosChromeServiceNeverBlockingState::BindMessageCenterReceiver,
            weak_sequenced_state_,
            message_center_remote_.BindNewPipeAndPassReceiver()));
  }

  if (IsOnBrowserStartupAvailable()) {
    never_blocking_sequence_->PostTask(
        FROM_HERE,
        base::BindOnce(&LacrosChromeServiceNeverBlockingState::OnBrowserStartup,
                       weak_sequenced_state_,
                       ToMojo(delegate_->GetChromeVersion())));
  }

  if (IsPrefsAvailable()) {
    mojo::PendingReceiver<crosapi::mojom::Prefs> pending_receiver =
        prefs_remote_.BindNewPipeAndPassReceiver();
    never_blocking_sequence_->PostTask(
        FROM_HERE,
        base::BindOnce(
            &LacrosChromeServiceNeverBlockingState::BindPrefsReceiver,
            weak_sequenced_state_, std::move(pending_receiver)));
  }

  // Bind the remote for SelectFile on the current thread, and then pass the
  // receiver to the never_blocking_sequence_.
  if (IsSelectFileAvailable()) {
    mojo::PendingReceiver<crosapi::mojom::SelectFile>
        select_file_pending_receiver =
            select_file_remote_.BindNewPipeAndPassReceiver();
    never_blocking_sequence_->PostTask(
        FROM_HERE,
        base::BindOnce(
            &LacrosChromeServiceNeverBlockingState::BindSelectFileReceiver,
            weak_sequenced_state_, std::move(select_file_pending_receiver)));
  }

  if (IsTestControllerAvailable()) {
    mojo::PendingReceiver<crosapi::mojom::TestController> pending_receiver =
        test_controller_remote_.BindNewPipeAndPassReceiver();
    never_blocking_sequence_->PostTask(
        FROM_HERE,
        base::BindOnce(
            &LacrosChromeServiceNeverBlockingState::BindTestControllerReceiver,
            weak_sequenced_state_, std::move(pending_receiver)));
  }

  if (IsUrlHandlerAvailable()) {
    mojo::PendingReceiver<crosapi::mojom::UrlHandler> pending_receiver =
        url_handler_remote_.BindNewPipeAndPassReceiver();
    never_blocking_sequence_->PostTask(
        FROM_HERE,
        base::BindOnce(
            &LacrosChromeServiceNeverBlockingState::BindUrlHandlerReceiver,
            weak_sequenced_state_, std::move(pending_receiver)));
  }
}

// static
void LacrosChromeServiceImpl::DisableCrosapiForTests() {
  g_disable_all_crosapi_for_tests = true;
}

bool LacrosChromeServiceImpl::IsAccountManagerAvailable() const {
  base::Optional<uint32_t> version = CrosapiVersion();
  return version &&
         version.value() >=
             Crosapi::MethodMinVersions::kBindAccountManagerMinVersion;
}

bool LacrosChromeServiceImpl::IsCertDbAvailable() const {
  base::Optional<uint32_t> version = CrosapiVersion();
  return version && version.value() >=
                        Crosapi::MethodMinVersions::kBindCertDatabaseMinVersion;
}

bool LacrosChromeServiceImpl::IsClipboardAvailable() const {
  base::Optional<uint32_t> version = CrosapiVersion();
  return version && version.value() >=
                        Crosapi::MethodMinVersions::kBindClipboardMinVersion;
}

bool LacrosChromeServiceImpl::IsDeviceAttributesAvailable() const {
  base::Optional<uint32_t> version = CrosapiVersion();
  return version &&
         version.value() >=
             Crosapi::MethodMinVersions::kBindDeviceAttributesMinVersion;
}

bool LacrosChromeServiceImpl::IsFeedbackAvailable() const {
  base::Optional<uint32_t> version = CrosapiVersion();
  return version &&
         version.value() >= Crosapi::MethodMinVersions::kBindFeedbackMinVersion;
}

bool LacrosChromeServiceImpl::IsFileManagerAvailable() const {
  base::Optional<uint32_t> version = CrosapiVersion();
  return version && version.value() >=
                        Crosapi::MethodMinVersions::kBindFileManagerMinVersion;
}

bool LacrosChromeServiceImpl::IsHidManagerAvailable() const {
  base::Optional<uint32_t> version = CrosapiVersion();
  return version && version.value() >=
                        Crosapi::MethodMinVersions::kBindHidManagerMinVersion;
}

bool LacrosChromeServiceImpl::IsKeystoreServiceAvailable() const {
  base::Optional<uint32_t> version = CrosapiVersion();
  return version &&
         version.value() >=
             Crosapi::MethodMinVersions::kBindKeystoreServiceMinVersion;
}

bool LacrosChromeServiceImpl::IsMediaSessionAudioFocusAvailable() const {
  base::Optional<uint32_t> version = CrosapiVersion();
  return version &&
         version.value() >=
             Crosapi::MethodMinVersions::kBindMediaSessionAudioFocusMinVersion;
}

bool LacrosChromeServiceImpl::IsMediaSessionAudioFocusDebugAvailable() const {
  base::Optional<uint32_t> version = CrosapiVersion();
  return version && version.value() >=
                        Crosapi::MethodMinVersions::
                            kBindMediaSessionAudioFocusDebugMinVersion;
}

bool LacrosChromeServiceImpl::IsMediaSessionControllerAvailable() const {
  base::Optional<uint32_t> version = CrosapiVersion();
  return version &&
         version.value() >=
             Crosapi::MethodMinVersions::kBindMediaSessionControllerMinVersion;
}

bool LacrosChromeServiceImpl::IsMessageCenterAvailable() const {
  base::Optional<uint32_t> version = CrosapiVersion();
  return version &&
         version.value() >=
             Crosapi::MethodMinVersions::kBindMessageCenterMinVersion;
}

bool LacrosChromeServiceImpl::IsMetricsReportingAvailable() const {
  base::Optional<uint32_t> version = CrosapiVersion();
  return version &&
         version.value() >=
             Crosapi::MethodMinVersions::kBindMetricsReportingMinVersion;
}

bool LacrosChromeServiceImpl::IsPrefsAvailable() const {
  base::Optional<uint32_t> version = CrosapiVersion();
  return version &&
         version.value() >= Crosapi::MethodMinVersions::kBindPrefsMinVersion;
}

bool LacrosChromeServiceImpl::IsScreenManagerAvailable() const {
  base::Optional<uint32_t> version = CrosapiVersion();
  return version &&
         version.value() >=
             Crosapi::MethodMinVersions::kBindScreenManagerMinVersion;
}

bool LacrosChromeServiceImpl::IsSelectFileAvailable() const {
  base::Optional<uint32_t> version = CrosapiVersion();
  return version && version.value() >=
                        Crosapi::MethodMinVersions::kBindSelectFileMinVersion;
}

bool LacrosChromeServiceImpl::IsSensorHalClientAvailable() const {
  base::Optional<uint32_t> version = CrosapiVersion();
  return version &&
         version.value() >=
             Crosapi::MethodMinVersions::kBindSensorHalClientMinVersion;
}

bool LacrosChromeServiceImpl::IsTestControllerAvailable() const {
#if BUILDFLAG(IS_CHROMEOS_DEVICE)
  // The test controller is not available on production devices as tests only
  // run on Linux.
  return false;
#else
  base::Optional<uint32_t> version = CrosapiVersion();
  return version &&
         version.value() >=
             Crosapi::MethodMinVersions::kBindTestControllerMinVersion;
#endif
}

bool LacrosChromeServiceImpl::IsUrlHandlerAvailable() const {
  base::Optional<uint32_t> version = CrosapiVersion();
  return version && version.value() >=
                        Crosapi::MethodMinVersions::kBindUrlHandlerMinVersion;
}

void LacrosChromeServiceImpl::BindAccountManagerReceiver(
    mojo::PendingReceiver<crosapi::mojom::AccountManager> pending_receiver) {
  DCHECK(IsAccountManagerAvailable());
  never_blocking_sequence_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &LacrosChromeServiceNeverBlockingState::BindAccountManagerReceiver,
          weak_sequenced_state_, std::move(pending_receiver)));
}

void LacrosChromeServiceImpl::BindAudioFocusManager(
    mojo::PendingReceiver<media_session::mojom::AudioFocusManager> remote) {
  DCHECK(IsMediaSessionAudioFocusAvailable());

  never_blocking_sequence_->PostTask(
      FROM_HERE, base::BindOnce(&LacrosChromeServiceNeverBlockingState::
                                    BindMediaSessionAudioFocusReceiver,
                                weak_sequenced_state_, std::move(remote)));
}

void LacrosChromeServiceImpl::BindAudioFocusManagerDebug(
    mojo::PendingReceiver<media_session::mojom::AudioFocusManagerDebug>
        remote) {
  DCHECK(IsMediaSessionAudioFocusAvailable());

  never_blocking_sequence_->PostTask(
      FROM_HERE, base::BindOnce(&LacrosChromeServiceNeverBlockingState::
                                    BindMediaSessionAudioFocusDebugReceiver,
                                weak_sequenced_state_, std::move(remote)));
}

void LacrosChromeServiceImpl::BindMediaControllerManager(
    mojo::PendingReceiver<media_session::mojom::MediaControllerManager>
        remote) {
  DCHECK(IsMediaSessionAudioFocusAvailable());

  never_blocking_sequence_->PostTask(
      FROM_HERE, base::BindOnce(&LacrosChromeServiceNeverBlockingState::
                                    BindMediaSessionControllerReceiver,
                                weak_sequenced_state_, std::move(remote)));
}

void LacrosChromeServiceImpl::BindMetricsReporting(
    mojo::PendingReceiver<crosapi::mojom::MetricsReporting> receiver) {
  DCHECK(IsMetricsReportingAvailable());
  never_blocking_sequence_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &LacrosChromeServiceNeverBlockingState::BindMetricsReportingReceiver,
          weak_sequenced_state_, std::move(receiver)));
}

void LacrosChromeServiceImpl::BindScreenManagerReceiver(
    mojo::PendingReceiver<crosapi::mojom::ScreenManager> pending_receiver) {
  DCHECK(IsScreenManagerAvailable());
  never_blocking_sequence_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &LacrosChromeServiceNeverBlockingState::BindScreenManagerReceiver,
          weak_sequenced_state_, std::move(pending_receiver)));
}

void LacrosChromeServiceImpl::BindSensorHalClient(
    mojo::PendingRemote<chromeos::sensors::mojom::SensorHalClient> remote) {
  DCHECK(IsSensorHalClientAvailable());

  never_blocking_sequence_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &LacrosChromeServiceNeverBlockingState::BindSensorHalClientRemote,
          weak_sequenced_state_, std::move(remote)));
}

bool LacrosChromeServiceImpl::IsOnBrowserStartupAvailable() const {
  base::Optional<uint32_t> version = CrosapiVersion();
  return version && version.value() >=
                        Crosapi::MethodMinVersions::kOnBrowserStartupMinVersion;
}

int LacrosChromeServiceImpl::GetInterfaceVersion(
    base::Token interface_uuid) const {
  if (g_disable_all_crosapi_for_tests)
    return -1;
  if (!init_params_->interface_versions)
    return -1;
  const base::flat_map<base::Token, uint32_t>& versions =
      init_params_->interface_versions.value();
  auto it = versions.find(interface_uuid);
  if (it == versions.end())
    return -1;
  return it->second;
}

void LacrosChromeServiceImpl::SetInitParamsForTests(
    crosapi::mojom::BrowserInitParamsPtr init_params) {
  init_params_ = std::move(init_params);
}

void LacrosChromeServiceImpl::NewWindowAffineSequence() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(affine_sequence_checker_);
  delegate_->NewWindow();
}

void LacrosChromeServiceImpl::GetFeedbackDataAffineSequence(
    GetFeedbackDataCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(affine_sequence_checker_);
  delegate_->GetFeedbackData(std::move(callback));
}

void LacrosChromeServiceImpl::GetHistogramsAffineSequence(
    GetHistogramsCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(affine_sequence_checker_);
  delegate_->GetHistograms(std::move(callback));
}

void LacrosChromeServiceImpl::GetActiveTabUrlAffineSequence(
    GetActiveTabUrlCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(affine_sequence_checker_);
  std::move(callback).Run(delegate_->GetActiveTabUrl());
}

base::Optional<uint32_t> LacrosChromeServiceImpl::CrosapiVersion() const {
  if (g_disable_all_crosapi_for_tests)
    return base::nullopt;
  DCHECK(did_bind_receiver_);
  return init_params_->crosapi_version;
}

}  // namespace chromeos
