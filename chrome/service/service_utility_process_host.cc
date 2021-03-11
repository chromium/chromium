// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/service/service_utility_process_host.h"

#include <stdint.h>

#include <limits>
#include <utility>
#include <vector>

#include "base/base_switches.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/process/launch.h"
#include "base/process/process_handle.h"
#include "base/rand_util.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/task_runner_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/win/win_util.h"
#include "build/build_config.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_utility_printing_messages.h"
#include "chrome/services/printing/public/mojom/pdf_to_emf_converter.mojom.h"
#include "content/public/common/child_process_host.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/font_cache_dispatcher_win.h"
#include "content/public/common/result_codes.h"
#include "content/public/common/sandbox_init.h"
#include "content/public/common/sandboxed_process_launcher_delegate.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/platform/named_platform_channel.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "mojo/public/cpp/platform/platform_channel_endpoint.h"
#include "mojo/public/cpp/system/invitation.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "printing/emf_win.h"
#include "sandbox/policy/sandbox_type.h"
#include "sandbox/policy/switches.h"
#include "sandbox/win/src/sandbox_policy.h"
#include "sandbox/win/src/sandbox_types.h"
#include "ui/base/ui_base_switches.h"

namespace {

using content::ChildProcessHost;

enum ServiceUtilityProcessHostEvent {
  SERVICE_UTILITY_STARTED,
  SERVICE_UTILITY_DISCONNECTED,
  SERVICE_UTILITY_METAFILE_REQUEST,
  SERVICE_UTILITY_METAFILE_SUCCEEDED,
  SERVICE_UTILITY_METAFILE_FAILED,
  SERVICE_UTILITY_CAPS_REQUEST,
  SERVICE_UTILITY_CAPS_SUCCEEDED,
  SERVICE_UTILITY_CAPS_FAILED,
  SERVICE_UTILITY_SEMANTIC_CAPS_REQUEST,
  SERVICE_UTILITY_SEMANTIC_CAPS_SUCCEEDED,
  SERVICE_UTILITY_SEMANTIC_CAPS_FAILED,
  SERVICE_UTILITY_FAILED_TO_START,
  SERVICE_UTILITY_EVENT_MAX,
};

void ReportUmaEvent(ServiceUtilityProcessHostEvent id) {
  UMA_HISTOGRAM_ENUMERATION("CloudPrint.ServiceUtilityProcessHostEvent",
                            id,
                            SERVICE_UTILITY_EVENT_MAX);
}

// NOTE: changes to this class need to be reviewed by the security team.
class ServiceSandboxedProcessLauncherDelegate
    : public content::SandboxedProcessLauncherDelegate {
 public:
  ServiceSandboxedProcessLauncherDelegate() {}

  bool PreSpawnTarget(sandbox::TargetPolicy* policy) override {
    // Ignore result of SetAlternateDesktop. Service process may run as windows
    // service and it fails to create a window station.
    base::IgnoreResult(policy->SetAlternateDesktop(false));
    return true;
  }

  sandbox::policy::SandboxType GetSandboxType() override {
    return sandbox::policy::SandboxType::kPdfConversion;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ServiceSandboxedProcessLauncherDelegate);
};

// This implementation does not do any font pre-caching.
// TODO(thestig): Can this be deleted and the PdfToEmfConverterClient be made
// optional?
class ServicePdfToEmfConverterClientImpl
    : public printing::mojom::PdfToEmfConverterClient {
 public:
  explicit ServicePdfToEmfConverterClientImpl(
      mojo::PendingReceiver<printing::mojom::PdfToEmfConverterClient> receiver)
      : receiver_(this, std::move(receiver)) {}

 private:
  // mojom::PdfToEmfConverterClient:
  void PreCacheFontCharacters(
      const std::vector<uint8_t>& logfont_data,
      const std::u16string& characters,
      PreCacheFontCharactersCallback callback) override {
    std::move(callback).Run();
  }

  mojo::Receiver<printing::mojom::PdfToEmfConverterClient> receiver_;
};

}  // namespace

class ServiceUtilityProcessHost::PdfToEmfState {
 public:
  explicit PdfToEmfState(base::WeakPtr<ServiceUtilityProcessHost> host)
      : weak_host_(host) {}

  ~PdfToEmfState() { Stop(); }

  bool Start(base::ReadOnlySharedMemoryRegion pdf_region,
             const printing::PdfRenderSettings& conversion_settings) {
    weak_host_->GetHost()->BindReceiver(
        pdf_to_emf_converter_factory_.BindNewPipeAndPassReceiver());

    pdf_to_emf_converter_factory_.set_disconnect_handler(base::BindOnce(
        &PdfToEmfState::OnFailed, weak_host_,
        std::string("Connection to PdfToEmfConverterFactory error.")));

    mojo::PendingRemote<printing::mojom::PdfToEmfConverterClient>
        pdf_to_emf_converter_client_remote;
    pdf_to_emf_converter_client_impl_ =
        std::make_unique<ServicePdfToEmfConverterClientImpl>(
            pdf_to_emf_converter_client_remote
                .InitWithNewPipeAndPassReceiver());

    pdf_to_emf_converter_factory_->CreateConverter(
        std::move(pdf_region), conversion_settings,
        std::move(pdf_to_emf_converter_client_remote),
        base::BindOnce(
            &ServiceUtilityProcessHost::OnRenderPDFPagesToMetafilesPageCount,
            weak_host_));
    return true;
  }

  void GotPageCount(
      mojo::PendingRemote<printing::mojom::PdfToEmfConverter> converter,
      uint32_t page_count) {
    DCHECK(!pdf_to_emf_converter_.is_bound());
    pdf_to_emf_converter_.Bind(std::move(converter));
    pdf_to_emf_converter_.set_disconnect_handler(
        base::BindOnce(&PdfToEmfState::OnFailed, weak_host_,
                       std::string("Connection to PdfToEmfConverter error.")));
    page_count_ = page_count;
  }

  void GetMorePages() {
    const int kMaxNumberOfTempFilesPerDocument = 3;
    while (pages_in_progress_ < kMaxNumberOfTempFilesPerDocument &&
           current_page_ < page_count_) {
      ++pages_in_progress_;

      pdf_to_emf_converter_->ConvertPage(
          current_page_++,
          base::BindOnce(
              &ServiceUtilityProcessHost::OnRenderPDFPagesToMetafilesPageDone,
              weak_host_));
    }
  }

  // Returns true if all pages processed and client should not expect more
  // results.
  bool OnPageProcessed() {
    --pages_in_progress_;
    GetMorePages();
    if (pages_in_progress_ || current_page_ < page_count_)
      return false;
    Stop();
    return true;
  }

  bool has_page_count() const { return page_count_ > 0; }

 private:
  static void OnFailed(const base::WeakPtr<ServiceUtilityProcessHost>& host,
                       const std::string& error_message) {
    LOG(ERROR) << "Failed to convert PDF: " << error_message;
    host->OnChildDisconnected();
  }

  void Stop() {
    // Disconnect interface ptrs so that the printing service process stop.
    pdf_to_emf_converter_factory_.reset();
    pdf_to_emf_converter_.reset();
  }

  base::WeakPtr<ServiceUtilityProcessHost> weak_host_;
  int page_count_ = 0;
  int current_page_ = 0;
  int pages_in_progress_ = 0;

  std::unique_ptr<ServicePdfToEmfConverterClientImpl>
      pdf_to_emf_converter_client_impl_;

  mojo::Remote<printing::mojom::PdfToEmfConverter> pdf_to_emf_converter_;

  mojo::Remote<printing::mojom::PdfToEmfConverterFactory>
      pdf_to_emf_converter_factory_;
};

ServiceUtilityProcessHost::ServiceUtilityProcessHost(
    Client* client,
    base::SingleThreadTaskRunner* client_task_runner)
    : client_(client),
      client_task_runner_(client_task_runner),
      waiting_for_reply_(false) {
  child_process_host_ =
      ChildProcessHost::Create(this, ChildProcessHost::IpcMode::kNormal);
}

ServiceUtilityProcessHost::~ServiceUtilityProcessHost() {
  // We need to kill the child process when the host dies.
  process_.Terminate(content::RESULT_CODE_NORMAL_EXIT, false);
}

bool ServiceUtilityProcessHost::StartRenderPDFPagesToMetafile(
    const base::FilePath& pdf_path,
    const printing::PdfRenderSettings& render_settings) {
  ReportUmaEvent(SERVICE_UTILITY_METAFILE_REQUEST);
  base::File pdf_file(pdf_path, base::File::FLAG_OPEN | base::File::FLAG_READ |
                                    base::File::FLAG_DELETE_ON_CLOSE);
  if (!pdf_file.IsValid())
    return false;

  int64_t size = pdf_file.GetLength();
  if (size <= 0 || size >= std::numeric_limits<int>::max())
    return false;

  base::MappedReadOnlyRegion memory =
      base::ReadOnlySharedMemoryRegion::Create(size);
  if (!memory.IsValid())
    return false;

  int result =
      pdf_file.Read(0, static_cast<char*>(memory.mapping.memory()), size);
  if (result != size)
    return false;

  if (!StartProcess(/*sandbox=*/true))
    return false;

  DCHECK(!waiting_for_reply_);
  waiting_for_reply_ = true;

  pdf_to_emf_state_ =
      std::make_unique<PdfToEmfState>(weak_ptr_factory_.GetWeakPtr());
  return pdf_to_emf_state_->Start(std::move(memory.region), render_settings);
}

bool ServiceUtilityProcessHost::StartGetPrinterCapsAndDefaults(
    const std::string& printer_name) {
  ReportUmaEvent(SERVICE_UTILITY_CAPS_REQUEST);
  if (!StartProcess(/*sandbox=*/false))
    return false;
  DCHECK(!waiting_for_reply_);
  waiting_for_reply_ = true;
  return Send(new ChromeUtilityMsg_GetPrinterCapsAndDefaults(printer_name));
}

bool ServiceUtilityProcessHost::StartGetPrinterSemanticCapsAndDefaults(
    const std::string& printer_name) {
  ReportUmaEvent(SERVICE_UTILITY_SEMANTIC_CAPS_REQUEST);
  if (!StartProcess(/*sandbox=*/false))
    return false;
  DCHECK(!waiting_for_reply_);
  waiting_for_reply_ = true;
  return Send(
      new ChromeUtilityMsg_GetPrinterSemanticCapsAndDefaults(printer_name));
}

bool ServiceUtilityProcessHost::StartProcess(bool sandbox) {
  base::FilePath exe_path = GetUtilityProcessCmd();
  if (exe_path.empty()) {
    NOTREACHED() << "Unable to get utility process binary name.";
    return false;
  }

  // NOTE: This call to |CreateChannelMojo()| requires a working
  // ServiceManagerConnection to have already been established.
  child_process_host_->CreateChannelMojo();

  base::CommandLine cmd_line(exe_path);
  cmd_line.AppendSwitchASCII(switches::kProcessType, switches::kUtilityProcess);
  cmd_line.AppendSwitch(switches::kLang);
  cmd_line.AppendArg(switches::kPrefetchArgumentOther);

  if (Launch(&cmd_line, sandbox)) {
    ReportUmaEvent(SERVICE_UTILITY_STARTED);
    return true;
  }
  ReportUmaEvent(SERVICE_UTILITY_FAILED_TO_START);
  return false;
}

bool ServiceUtilityProcessHost::Launch(base::CommandLine* cmd_line,
                                       bool sandbox) {
  const base::CommandLine& service_command_line =
      *base::CommandLine::ForCurrentProcess();
  static const char* const kForwardSwitches[] = {
    switches::kDisableLogging,
    switches::kEnableLogging,
    switches::kIPCConnectionTimeout,
    switches::kLoggingLevel,
    switches::kUtilityStartupDialog,
    switches::kV,
    switches::kVModule,
#if defined(OS_WIN)
    switches::kDisableHighResTimer,
    switches::kRaiseTimerFrequency,
#endif
  };
  cmd_line->CopySwitchesFrom(service_command_line, kForwardSwitches,
                             base::size(kForwardSwitches));

  mojo::OutgoingInvitation mojo_invitation =
      std::move(*child_process_host_->GetMojoInvitation());
  if (sandbox) {
    mojo::PlatformChannel channel;
    base::HandlesToInheritVector handles;
    channel.PrepareToPassRemoteEndpoint(&handles, cmd_line);

    // Need to call SetCommandLineFlagsForSandboxType() here, as this process
    // launch path bypasses content::UtilityProcessHost::StartProcess().
    ServiceSandboxedProcessLauncherDelegate delegate;
    sandbox::policy::SetCommandLineFlagsForSandboxType(
        cmd_line, delegate.GetSandboxType());

    base::Process process;
    sandbox::ResultCode result = content::StartSandboxedProcess(
        &delegate, cmd_line, handles, &process);
    if (result != sandbox::SBOX_ALL_OK)
      return false;

    process_ = std::move(process);
    mojo::OutgoingInvitation::Send(std::move(mojo_invitation),
                                   process_.Handle(),
                                   channel.TakeLocalEndpoint());
  } else {
    mojo::NamedPlatformChannel::Options options;
    mojo::NamedPlatformChannel channel(options);
    channel.PassServerNameOnCommandLine(cmd_line);

    cmd_line->AppendSwitch(sandbox::policy::switches::kNoSandbox);
    process_ = base::LaunchProcess(*cmd_line, base::LaunchOptions());
    mojo::OutgoingInvitation::Send(std::move(mojo_invitation),
                                   process_.Handle(),
                                   channel.TakeServerEndpoint());
  }

  return true;
}

bool ServiceUtilityProcessHost::Send(IPC::Message* msg) {
  if (child_process_host_)
    return child_process_host_->Send(msg);
  delete msg;
  return false;
}

base::FilePath ServiceUtilityProcessHost::GetUtilityProcessCmd() {
  return ChildProcessHost::GetChildPath(ChildProcessHost::CHILD_NORMAL);
}

void ServiceUtilityProcessHost::OnChildDisconnected() {
  if (waiting_for_reply_) {
    // If we are yet to receive a reply then notify the client that the
    // child died.
    client_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&Client::OnChildDied, client_.get()));
    ReportUmaEvent(SERVICE_UTILITY_DISCONNECTED);
  }

  // The child process has died for some reason. This host is no longer needed.
  delete this;
}

bool ServiceUtilityProcessHost::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ServiceUtilityProcessHost, message)
    IPC_MESSAGE_HANDLER(
        ChromeUtilityHostMsg_GetPrinterCapsAndDefaults_Succeeded,
        OnGetPrinterCapsAndDefaultsSucceeded)
    IPC_MESSAGE_HANDLER(ChromeUtilityHostMsg_GetPrinterCapsAndDefaults_Failed,
                        OnGetPrinterCapsAndDefaultsFailed)
    IPC_MESSAGE_HANDLER(
        ChromeUtilityHostMsg_GetPrinterSemanticCapsAndDefaults_Succeeded,
        OnGetPrinterSemanticCapsAndDefaultsSucceeded)
    IPC_MESSAGE_HANDLER(
        ChromeUtilityHostMsg_GetPrinterSemanticCapsAndDefaults_Failed,
        OnGetPrinterSemanticCapsAndDefaultsFailed)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

const base::Process& ServiceUtilityProcessHost::GetProcess() {
  return process_;
}

void ServiceUtilityProcessHost::OnMetafileSpooled(bool success) {
  if (!success || pdf_to_emf_state_->OnPageProcessed())
    OnPDFToEmfFinished(success);
}

void ServiceUtilityProcessHost::OnRenderPDFPagesToMetafilesPageCount(
    mojo::PendingRemote<printing::mojom::PdfToEmfConverter> converter,
    uint32_t page_count) {
  DCHECK(waiting_for_reply_);
  if (page_count == 0 || pdf_to_emf_state_->has_page_count())
    return OnPDFToEmfFinished(false);

  pdf_to_emf_state_->GotPageCount(std::move(converter), page_count);
  pdf_to_emf_state_->GetMorePages();
}

void ServiceUtilityProcessHost::OnRenderPDFPagesToMetafilesPageDone(
    base::ReadOnlySharedMemoryRegion emf_region,
    float scale_factor) {
  DCHECK(waiting_for_reply_);
  if (!pdf_to_emf_state_ || !emf_region.IsValid())
    return OnPDFToEmfFinished(false);

  base::PostTaskAndReplyWithResult(
      client_task_runner_.get(), FROM_HERE,
      base::BindOnce(&Client::MetafileAvailable, client_.get(), scale_factor,
                     std::move(emf_region)),
      base::BindOnce(&ServiceUtilityProcessHost::OnMetafileSpooled,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ServiceUtilityProcessHost::OnPDFToEmfFinished(bool success) {
  if (!waiting_for_reply_)
    return;

  waiting_for_reply_ = false;
  ReportUmaEvent(success ? SERVICE_UTILITY_METAFILE_SUCCEEDED
                         : SERVICE_UTILITY_METAFILE_FAILED);
  client_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&Client::OnRenderPDFPagesToMetafileDone,
                                client_.get(), success));
  pdf_to_emf_state_.reset();

  // The child process has finished at this point. This host is done as well.
  delete this;
}

void ServiceUtilityProcessHost::OnGetPrinterCapsAndDefaultsSucceeded(
    const std::string& printer_name,
    const printing::PrinterCapsAndDefaults& caps_and_defaults) {
  DCHECK(waiting_for_reply_);
  ReportUmaEvent(SERVICE_UTILITY_CAPS_SUCCEEDED);
  waiting_for_reply_ = false;
  client_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&Client::OnGetPrinterCapsAndDefaults, client_.get(), true,
                     printer_name, caps_and_defaults));
  // The child process disconnects itself and this host deletes itself via
  // OnChildDisconnected().
}

void ServiceUtilityProcessHost::OnGetPrinterSemanticCapsAndDefaultsSucceeded(
    const std::string& printer_name,
    const printing::PrinterSemanticCapsAndDefaults& caps_and_defaults) {
  DCHECK(waiting_for_reply_);
  ReportUmaEvent(SERVICE_UTILITY_SEMANTIC_CAPS_SUCCEEDED);
  waiting_for_reply_ = false;
  client_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&Client::OnGetPrinterSemanticCapsAndDefaults,
                     client_.get(), true, printer_name, caps_and_defaults));
  // The child process disconnects itself and this host deletes itself via
  // OnChildDisconnected().
}

void ServiceUtilityProcessHost::OnGetPrinterCapsAndDefaultsFailed(
    const std::string& printer_name) {
  DCHECK(waiting_for_reply_);
  ReportUmaEvent(SERVICE_UTILITY_CAPS_FAILED);
  waiting_for_reply_ = false;
  client_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&Client::OnGetPrinterCapsAndDefaults, client_.get(), false,
                     printer_name, printing::PrinterCapsAndDefaults()));
  // The child process disconnects itself and this host deletes itself via
  // OnChildDisconnected().
}

void ServiceUtilityProcessHost::OnGetPrinterSemanticCapsAndDefaultsFailed(
    const std::string& printer_name) {
  DCHECK(waiting_for_reply_);
  ReportUmaEvent(SERVICE_UTILITY_SEMANTIC_CAPS_FAILED);
  waiting_for_reply_ = false;
  client_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&Client::OnGetPrinterSemanticCapsAndDefaults,
                                client_.get(), false, printer_name,
                                printing::PrinterSemanticCapsAndDefaults()));
  // The child process disconnects itself and this host deletes itself via
  // OnChildDisconnected().
}

bool ServiceUtilityProcessHost::Client::MetafileAvailable(
    float scale_factor,
    base::ReadOnlySharedMemoryRegion emf_region) {
  base::ReadOnlySharedMemoryMapping mapping = emf_region.Map();
  if (!mapping.IsValid()) {
    OnRenderPDFPagesToMetafileDone(false);
    return false;
  }
  printing::Emf emf;
  if (!emf.InitFromData(mapping.GetMemoryAsSpan<const uint8_t>())) {
    OnRenderPDFPagesToMetafileDone(false);
    return false;
  }
  OnRenderPDFPagesToMetafilePageDone(scale_factor, emf);
  return true;
}
