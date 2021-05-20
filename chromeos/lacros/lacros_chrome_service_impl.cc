// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/lacros/lacros_chrome_service_impl.h"

#include <atomic>
#include <utility>

#include "base/bind_post_task.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/logging.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "build/chromeos_buildflags.h"
#include "chromeos/crosapi/cpp/crosapi_constants.h"
#include "chromeos/crosapi/mojom/app_service.mojom.h"
#include "chromeos/crosapi/mojom/automation.mojom.h"
#include "chromeos/crosapi/mojom/cert_database.mojom.h"
#include "chromeos/crosapi/mojom/clipboard.mojom.h"
#include "chromeos/crosapi/mojom/clipboard_history.mojom.h"
#include "chromeos/crosapi/mojom/content_protection.mojom.h"
#include "chromeos/crosapi/mojom/crosapi.mojom.h"
#include "chromeos/crosapi/mojom/download_controller.mojom.h"
#include "chromeos/crosapi/mojom/drive_integration_service.mojom.h"
#include "chromeos/crosapi/mojom/feedback.mojom.h"
#include "chromeos/crosapi/mojom/file_manager.mojom.h"
#include "chromeos/crosapi/mojom/holding_space_service.mojom.h"
#include "chromeos/crosapi/mojom/keystore_service.mojom.h"
#include "chromeos/crosapi/mojom/local_printer.mojom.h"
#include "chromeos/crosapi/mojom/message_center.mojom.h"
#include "chromeos/crosapi/mojom/metrics_reporting.mojom.h"
#include "chromeos/crosapi/mojom/prefs.mojom.h"
#include "chromeos/crosapi/mojom/screen_manager.mojom.h"
#include "chromeos/crosapi/mojom/select_file.mojom.h"
#include "chromeos/crosapi/mojom/system_display.mojom.h"
#include "chromeos/crosapi/mojom/task_manager.mojom.h"
#include "chromeos/crosapi/mojom/test_controller.mojom.h"
#include "chromeos/crosapi/mojom/url_handler.mojom.h"
#include "chromeos/lacros/lacros_chrome_service_delegate.h"
#include "chromeos/lacros/lacros_chrome_service_impl_never_blocking_state.h"
#include "chromeos/lacros/system_idle_cache.h"
#include "chromeos/services/machine_learning/public/mojom/machine_learning_service.mojom.h"
#include "chromeos/startup/startup.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "mojo/public/cpp/system/invitation.h"
#include "services/device/public/mojom/hid.mojom.h"
#include "url/gurl.h"

namespace chromeos {
namespace {

using Crosapi = crosapi::mojom::Crosapi;

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
  absl::optional<std::string> content = ReadStartupData();
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

LacrosChromeServiceImpl::InterfaceEntryBase::InterfaceEntryBase() = default;
LacrosChromeServiceImpl::InterfaceEntryBase::~InterfaceEntryBase() = default;

template <typename CrosapiInterface,
          void (Crosapi::*bind_func)(mojo::PendingReceiver<CrosapiInterface>),
          uint32_t MethodMinVersion>
class LacrosChromeServiceImpl::InterfaceEntry
    : public LacrosChromeServiceImpl::InterfaceEntryBase {
 public:
  InterfaceEntry() : InterfaceEntryBase() {}
  InterfaceEntry(const InterfaceEntry&) = delete;
  InterfaceEntry& operator=(const InterfaceEntry&) = delete;
  ~InterfaceEntry() override = default;
  void* GetInternal() override { return &remote_; }
  void MaybeBind(uint32_t crosapi_version,
                 LacrosChromeServiceImpl* impl) override {
    available_ = crosapi_version >= MethodMinVersion;
    if (available_) {
      impl->InitializeAndBindRemote<CrosapiInterface, bind_func>(&remote_);
    }
  }

 private:
  mojo::Remote<CrosapiInterface> remote_;
};

// static
LacrosChromeServiceImpl* LacrosChromeServiceImpl::Get() {
  // If this returns null and causes failure in a unit test, consider using
  // ScopedLacrosServiceTestHelper in the test to instantiate
  // LacrosChromeServiceImpl.
  return g_instance;
}

LacrosChromeServiceImpl::LacrosChromeServiceImpl(
    std::unique_ptr<LacrosChromeServiceDelegate> delegate)
    : delegate_(std::move(delegate)),
      sequenced_state_(nullptr, base::OnTaskRunnerDeleter(nullptr)),
      observer_list_(
          base::MakeRefCounted<base::ObserverListThreadSafe<Observer>>()) {
  if (disable_crosapi_for_testing_) {
    // Tests don't call BrowserService::InitDeprecated(), so provide
    // BrowserInitParams with default values.
    init_params_ = crosapi::mojom::BrowserInitParams::New();

    // To simplify testing, instantiate under Fallback mode.
    system_idle_cache_ = std::make_unique<SystemIdleCache>();

  } else {
    // Read the startup data from the inherited FD.
    init_params_ = ReadStartupBrowserInitParams();
    DCHECK(init_params_);
    if (init_params_->idle_info) {
      // Presence of initial |idle_info| indicates that ash-chrome can stream
      // idle info updates, so instantiate under Streaming mode, using
      // |idle_info| as initial cached values.
      system_idle_cache_ =
          std::make_unique<SystemIdleCache>(*init_params_->idle_info);

      // After construction finishes, start caching.
      base::SequencedTaskRunnerHandle::Get()->PostTask(
          FROM_HERE,
          base::BindOnce(&LacrosChromeServiceImpl::StartSystemIdleCache,
                         weak_factory_.GetWeakPtr()));
    } else {
      // Ash-chrome cannot stream, so instantiate under fallback mode.
      system_idle_cache_ = std::make_unique<SystemIdleCache>();
    }

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

  sequenced_state_ = std::unique_ptr<LacrosChromeServiceImplNeverBlockingState,
                                     base::OnTaskRunnerDeleter>(
      new LacrosChromeServiceImplNeverBlockingState(affine_sequence,
                                                    weak_factory_.GetWeakPtr()),
      base::OnTaskRunnerDeleter(never_blocking_sequence_));
  weak_sequenced_state_ = sequenced_state_->GetWeakPtr();

  never_blocking_sequence_->PostTask(
      FROM_HERE,
      base::BindOnce(&LacrosChromeServiceImplNeverBlockingState::BindCrosapi,
                     weak_sequenced_state_));

  ConstructRemote<crosapi::mojom::AppPublisher, &Crosapi::BindAppPublisher,
                  Crosapi::MethodMinVersions::kBindAppPublisherMinVersion>();
  ConstructRemote<
      chromeos::machine_learning::mojom::MachineLearningService,
      &crosapi::mojom::Crosapi::BindMachineLearningService,
      Crosapi::MethodMinVersions::kBindMachineLearningServiceMinVersion>();
  ConstructRemote<
      crosapi::mojom::AutomationFactory, &Crosapi::BindAutomationFactory,
      Crosapi::MethodMinVersions::kBindAutomationFactoryMinVersion>();
  ConstructRemote<crosapi::mojom::CertDatabase, &Crosapi::BindCertDatabase,
                  Crosapi::MethodMinVersions::kBindCertDatabaseMinVersion>();
  ConstructRemote<crosapi::mojom::Clipboard, &Crosapi::BindClipboard,
                  Crosapi::MethodMinVersions::kBindClipboardMinVersion>();
  ConstructRemote<
      crosapi::mojom::ClipboardHistory, &Crosapi::BindClipboardHistory,
      Crosapi::MethodMinVersions::kBindClipboardHistoryMinVersion>();
  ConstructRemote<
      crosapi::mojom::ContentProtection, &Crosapi::BindContentProtection,
      Crosapi::MethodMinVersions::kBindContentProtectionMinVersion>();
  ConstructRemote<
      crosapi::mojom::DeviceAttributes, &Crosapi::BindDeviceAttributes,
      Crosapi::MethodMinVersions::kBindDeviceAttributesMinVersion>();
  ConstructRemote<
      crosapi::mojom::DownloadController, &Crosapi::BindDownloadController,
      Crosapi::MethodMinVersions::kBindDownloadControllerMinVersion>();
  ConstructRemote<crosapi::mojom::Feedback,
                  &crosapi::mojom::Crosapi::BindFeedback,
                  Crosapi::MethodMinVersions::kBindFeedbackMinVersion>();
  ConstructRemote<crosapi::mojom::FileManager,
                  &crosapi::mojom::Crosapi::BindFileManager,
                  Crosapi::MethodMinVersions::kBindFileManagerMinVersion>();
  ConstructRemote<device::mojom::HidManager,
                  &crosapi::mojom::Crosapi::BindHidManager,
                  Crosapi::MethodMinVersions::kBindHidManagerMinVersion>();
  ConstructRemote<
      crosapi::mojom::HoldingSpaceService,
      &crosapi::mojom::Crosapi::BindHoldingSpaceService,
      Crosapi::MethodMinVersions::kBindHoldingSpaceServiceMinVersion>();
  ConstructRemote<crosapi::mojom::IdleService,
                  &crosapi::mojom::Crosapi::BindIdleService,
                  Crosapi::MethodMinVersions::kBindIdleServiceMinVersion>();
  ConstructRemote<crosapi::mojom::KeystoreService,
                  &crosapi::mojom::Crosapi::BindKeystoreService,
                  Crosapi::MethodMinVersions::kBindKeystoreServiceMinVersion>();
  ConstructRemote<crosapi::mojom::LocalPrinter,
                  &crosapi::mojom::Crosapi::BindLocalPrinter,
                  Crosapi::MethodMinVersions::kBindLocalPrinterMinVersion>();
  ConstructRemote<crosapi::mojom::MessageCenter,
                  &crosapi::mojom::Crosapi::BindMessageCenter,
                  Crosapi::MethodMinVersions::kBindMessageCenterMinVersion>();
  ConstructRemote<crosapi::mojom::Prefs, &crosapi::mojom::Crosapi::BindPrefs,
                  Crosapi::MethodMinVersions::kBindPrefsMinVersion>();
  ConstructRemote<crosapi::mojom::SelectFile,
                  &crosapi::mojom::Crosapi::BindSelectFile,
                  Crosapi::MethodMinVersions::kBindSelectFileMinVersion>();
  ConstructRemote<crosapi::mojom::SystemDisplay, &Crosapi::BindSystemDisplay,
                  Crosapi::MethodMinVersions::kBindSystemDisplayMinVersion>();
  ConstructRemote<crosapi::mojom::TaskManager,
                  &crosapi::mojom::Crosapi::BindTaskManager,
                  Crosapi::MethodMinVersions::kBindTaskManagerMinVersion>();
  ConstructRemote<crosapi::mojom::UrlHandler,
                  &crosapi::mojom::Crosapi::BindUrlHandler,
                  Crosapi::MethodMinVersions::kBindUrlHandlerMinVersion>();
  ConstructRemote<
      crosapi::mojom::DriveIntegrationService,
      &crosapi::mojom::Crosapi::BindDriveIntegrationService,
      Crosapi::MethodMinVersions::kBindDriveIntegrationServiceMinVersion>();

#if !BUILDFLAG(IS_CHROMEOS_DEVICE)
  // The test controller is not available on production devices as tests only
  // run on Linux.
  ConstructRemote<crosapi::mojom::TestController,
                  &crosapi::mojom::Crosapi::BindTestController,
                  Crosapi::MethodMinVersions::kBindTestControllerMinVersion>();
#endif

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
        FROM_HERE, base::BindOnce(&LacrosChromeServiceImplNeverBlockingState::
                                      BindBrowserServiceReceiver,
                                  weak_sequenced_state_, std::move(receiver)));
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
        base::BindOnce(
            &LacrosChromeServiceImplNeverBlockingState::FusePipeCrosapi,
            weak_sequenced_state_,
            mojo::PendingRemote<crosapi::mojom::Crosapi>(
                invitation.ExtractMessagePipe(0), /*version=*/0)));

    // In this case, ash-chrome should be new enough, so init params should be
    // passed from the startup outband file descriptor.
  }

  delegate_->OnInitialized(*init_params_);
  did_bind_receiver_ = true;

  if (CrosapiVersion()) {
    for (auto& entry : interfaces_) {
      entry.second->MaybeBind(*CrosapiVersion(), this);
    }
  }

  if (IsOnBrowserStartupAvailable()) {
    never_blocking_sequence_->PostTask(
        FROM_HERE,
        base::BindOnce(
            &LacrosChromeServiceImplNeverBlockingState::OnBrowserStartup,
            weak_sequenced_state_, ToMojo(delegate_->GetChromeVersion())));
  }
}

bool LacrosChromeServiceImpl::IsAccountManagerAvailable() const {
  absl::optional<uint32_t> version = CrosapiVersion();
  return version &&
         version.value() >=
             Crosapi::MethodMinVersions::kBindAccountManagerMinVersion;
}

bool LacrosChromeServiceImpl::IsMediaSessionAudioFocusAvailable() const {
  absl::optional<uint32_t> version = CrosapiVersion();
  return version &&
         version.value() >=
             Crosapi::MethodMinVersions::kBindMediaSessionAudioFocusMinVersion;
}

bool LacrosChromeServiceImpl::IsMediaSessionAudioFocusDebugAvailable() const {
  absl::optional<uint32_t> version = CrosapiVersion();
  return version && version.value() >=
                        Crosapi::MethodMinVersions::
                            kBindMediaSessionAudioFocusDebugMinVersion;
}

bool LacrosChromeServiceImpl::IsMediaSessionControllerAvailable() const {
  absl::optional<uint32_t> version = CrosapiVersion();
  return version &&
         version.value() >=
             Crosapi::MethodMinVersions::kBindMediaSessionControllerMinVersion;
}

bool LacrosChromeServiceImpl::IsMetricsReportingAvailable() const {
  absl::optional<uint32_t> version = CrosapiVersion();
  return version &&
         version.value() >=
             Crosapi::MethodMinVersions::kBindMetricsReportingMinVersion;
}

bool LacrosChromeServiceImpl::IsScreenManagerAvailable() const {
  absl::optional<uint32_t> version = CrosapiVersion();
  return version &&
         version.value() >=
             Crosapi::MethodMinVersions::kBindScreenManagerMinVersion;
}

bool LacrosChromeServiceImpl::IsSensorHalClientAvailable() const {
  absl::optional<uint32_t> version = CrosapiVersion();
  return version &&
         version.value() >=
             Crosapi::MethodMinVersions::kBindSensorHalClientMinVersion;
}

void LacrosChromeServiceImpl::BindAccountManagerReceiver(
    mojo::PendingReceiver<crosapi::mojom::AccountManager> pending_receiver) {
  DCHECK(IsAccountManagerAvailable());
  BindPendingReceiverOrRemote<
      mojo::PendingReceiver<crosapi::mojom::AccountManager>,
      &crosapi::mojom::Crosapi::BindAccountManager>(
      std::move(pending_receiver));
}

void LacrosChromeServiceImpl::BindAudioFocusManager(
    mojo::PendingReceiver<media_session::mojom::AudioFocusManager> remote) {
  DCHECK(IsMediaSessionAudioFocusAvailable());

  BindPendingReceiverOrRemote<
      mojo::PendingReceiver<media_session::mojom::AudioFocusManager>,
      &crosapi::mojom::Crosapi::BindMediaSessionAudioFocus>(std::move(remote));
}

void LacrosChromeServiceImpl::BindAudioFocusManagerDebug(
    mojo::PendingReceiver<media_session::mojom::AudioFocusManagerDebug>
        remote) {
  DCHECK(IsMediaSessionAudioFocusAvailable());

  BindPendingReceiverOrRemote<
      mojo::PendingReceiver<media_session::mojom::AudioFocusManagerDebug>,
      &crosapi::mojom::Crosapi::BindMediaSessionAudioFocusDebug>(
      std::move(remote));
}

void LacrosChromeServiceImpl::BindMachineLearningService(
    mojo::PendingReceiver<
        chromeos::machine_learning::mojom::MachineLearningService> receiver) {
  DCHECK(
      IsAvailable<chromeos::machine_learning::mojom::MachineLearningService>());

  BindPendingReceiverOrRemote<
      mojo::PendingReceiver<
          chromeos::machine_learning::mojom::MachineLearningService>,
      &crosapi::mojom::Crosapi::BindMachineLearningService>(
      std::move(receiver));
}

void LacrosChromeServiceImpl::BindMediaControllerManager(
    mojo::PendingReceiver<media_session::mojom::MediaControllerManager>
        remote) {
  DCHECK(IsMediaSessionAudioFocusAvailable());

  BindPendingReceiverOrRemote<
      mojo::PendingReceiver<media_session::mojom::MediaControllerManager>,
      &crosapi::mojom::Crosapi::BindMediaSessionController>(std::move(remote));
}

void LacrosChromeServiceImpl::BindMetricsReporting(
    mojo::PendingReceiver<crosapi::mojom::MetricsReporting> receiver) {
  DCHECK(IsMetricsReportingAvailable());
  BindPendingReceiverOrRemote<
      mojo::PendingReceiver<crosapi::mojom::MetricsReporting>,
      &crosapi::mojom::Crosapi::BindMetricsReporting>(std::move(receiver));
}

void LacrosChromeServiceImpl::BindScreenManagerReceiver(
    mojo::PendingReceiver<crosapi::mojom::ScreenManager> pending_receiver) {
  DCHECK(IsScreenManagerAvailable());
  BindPendingReceiverOrRemote<
      mojo::PendingReceiver<crosapi::mojom::ScreenManager>,
      &crosapi::mojom::Crosapi::BindScreenManager>(std::move(pending_receiver));
}

void LacrosChromeServiceImpl::BindSensorHalClient(
    mojo::PendingRemote<chromeos::sensors::mojom::SensorHalClient> remote) {
  DCHECK(IsSensorHalClientAvailable());
  BindPendingReceiverOrRemote<
      mojo::PendingRemote<chromeos::sensors::mojom::SensorHalClient>,
      &crosapi::mojom::Crosapi::BindSensorHalClient>(std::move(remote));
}

bool LacrosChromeServiceImpl::IsOnBrowserStartupAvailable() const {
  absl::optional<uint32_t> version = CrosapiVersion();
  return version && version.value() >=
                        Crosapi::MethodMinVersions::kOnBrowserStartupMinVersion;
}

void LacrosChromeServiceImpl::BindVideoCaptureDeviceFactory(
    mojo::PendingReceiver<crosapi::mojom::VideoCaptureDeviceFactory>
        pending_receiver) {
  DCHECK(IsVideoCaptureDeviceFactoryAvailable());
  BindPendingReceiverOrRemote<
      mojo::PendingReceiver<crosapi::mojom::VideoCaptureDeviceFactory>,
      &crosapi::mojom::Crosapi::BindVideoCaptureDeviceFactory>(
      std::move(pending_receiver));
}

bool LacrosChromeServiceImpl::IsVideoCaptureDeviceFactoryAvailable() const {
  absl::optional<uint32_t> version = CrosapiVersion();
  return version && version.value() >=
                        Crosapi::MethodMinVersions::
                            kBindVideoCaptureDeviceFactoryMinVersion;
}

int LacrosChromeServiceImpl::GetInterfaceVersion(
    base::Token interface_uuid) const {
  if (disable_crosapi_for_testing_)
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

void LacrosChromeServiceImpl::NewWindowAffineSequence(bool incognito) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(affine_sequence_checker_);
  delegate_->NewWindow(incognito);
}

void LacrosChromeServiceImpl::NewTabAffineSequence() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(affine_sequence_checker_);
  delegate_->NewTab();
}

void LacrosChromeServiceImpl::RestoreTabAffineSequence() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(affine_sequence_checker_);
  delegate_->RestoreTab();
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

absl::optional<uint32_t> LacrosChromeServiceImpl::CrosapiVersion() const {
  if (disable_crosapi_for_testing_)
    return absl::nullopt;
  DCHECK(did_bind_receiver_);
  return init_params_->crosapi_version;
}

void LacrosChromeServiceImpl::StartSystemIdleCache() {
  system_idle_cache_->Start();
}

template <typename PendingReceiverOrRemote,
          void (Crosapi::*bind_func)(PendingReceiverOrRemote)>
void LacrosChromeServiceImpl::BindPendingReceiverOrRemote(
    PendingReceiverOrRemote pending_receiver_or_remote) {
  never_blocking_sequence_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &LacrosChromeServiceImplNeverBlockingState::
              BindCrosapiFeatureReceiver<PendingReceiverOrRemote, bind_func>,
          weak_sequenced_state_, std::move(pending_receiver_or_remote)));
}

template <typename CrosapiInterface,
          void (Crosapi::*bind_func)(mojo::PendingReceiver<CrosapiInterface>)>
void LacrosChromeServiceImpl::InitializeAndBindRemote(
    mojo::Remote<CrosapiInterface>* remote) {
  mojo::PendingReceiver<CrosapiInterface> pending_receiver =
      remote->BindNewPipeAndPassReceiver();
  BindPendingReceiverOrRemote<mojo::PendingReceiver<CrosapiInterface>,
                              bind_func>(std::move(pending_receiver));
}

template <typename CrosapiInterface,
          void (Crosapi::*bind_func)(mojo::PendingReceiver<CrosapiInterface>),
          uint32_t MethodMinVersion>
void LacrosChromeServiceImpl::ConstructRemote() {
  DCHECK(!base::Contains(interfaces_, CrosapiInterface::Uuid_));
  interfaces_.emplace(CrosapiInterface::Uuid_,
                      std::make_unique<LacrosChromeServiceImpl::InterfaceEntry<
                          CrosapiInterface, bind_func, MethodMinVersion>>());
}

void LacrosChromeServiceImpl::AddObserver(Observer* obs) {
  observer_list_->AddObserver(obs);
}

void LacrosChromeServiceImpl::RemoveObserver(Observer* obs) {
  observer_list_->RemoveObserver(obs);
}

void LacrosChromeServiceImpl::UpdateDeviceAccountPolicyAffineSequence(
    const std::vector<uint8_t>& policy_fetch_response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(affine_sequence_checker_);
  observer_list_->Notify(FROM_HERE, &Observer::NotifyPolicyUpdate,
                         policy_fetch_response);
}

// static
bool LacrosChromeServiceImpl::disable_crosapi_for_testing_ = false;

}  // namespace chromeos
