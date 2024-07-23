// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/net_export_ui.h"

#include <stdint.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/lazy_instance.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/net/net_export_helper.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/chrome_select_file_policy.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/url_constants.h"
#include "components/grit/dev_ui_components_resources.h"
#include "components/net_log/net_export_file_writer.h"
#include "components/net_log/net_export_ui_constants.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "extensions/buildflags/buildflags.h"
#include "net/log/net_log_capture_mode.h"
#include "ui/shell_dialogs/select_file_dialog.h"
#include "ui/shell_dialogs/selected_file_info.h"

#if BUILDFLAG(IS_ANDROID)
#include "components/browser_ui/share/android/intent_helper.h"
#endif

using content::BrowserThread;
using content::WebContents;
using content::WebUIMessageHandler;

namespace {

// May only be accessed on the UI thread
base::LazyInstance<base::FilePath>::Leaky
    last_save_dir = LAZY_INSTANCE_INITIALIZER;

void CreateAndAddNetExportHTMLSource(Profile* profile) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      profile, chrome::kChromeUINetExportHost);

  source->UseStringsJs();
  source->AddResourcePath(net_log::kNetExportUICSS, IDR_NET_LOG_NET_EXPORT_CSS);
  source->AddResourcePath(net_log::kNetExportUIJS, IDR_NET_LOG_NET_EXPORT_JS);
  source->SetDefaultResource(IDR_NET_LOG_NET_EXPORT_HTML);
}

// This class receives javascript messages from the renderer.
// Note that the WebUI infrastructure runs on the UI thread, therefore all of
// this class's public methods are expected to run on the UI thread.
class NetExportMessageHandler final
    : public WebUIMessageHandler,
      public ui::SelectFileDialog::Listener,
      public net_log::NetExportFileWriter::StateObserver {
 public:
  NetExportMessageHandler();

  NetExportMessageHandler(const NetExportMessageHandler&) = delete;
  NetExportMessageHandler& operator=(const NetExportMessageHandler&) = delete;

  ~NetExportMessageHandler() override;

  // WebUIMessageHandler implementation.
  void RegisterMessages() override;

  // Messages
  void OnEnableNotifyUIWithState(const base::Value::List& list);
  void OnStartNetLog(const base::Value::List& list);
  void OnStopNetLog(const base::Value::List& list);
  void OnSendNetLog(const base::Value::List& list);
  void OnShowFile(const base::Value::List& list);

  // ui::SelectFileDialog::Listener implementation.
  void FileSelected(const ui::SelectedFileInfo& file, int index) override;
  void FileSelectionCanceled() override;

  // net_log::NetExportFileWriter::StateObserver implementation.
  void OnNewState(const base::Value::Dict& state) override;

 private:
  // Send NetLog data via email.
  static void SendEmail(const base::FilePath& file_to_send);

  void StartNetLog(const base::FilePath& path);

  // Reveal |path| in the shell on desktop platforms.
  void ShowFileInShell(const base::FilePath& path);

  // chrome://net-export can be used on both mobile and desktop platforms.
  // On mobile a user cannot pick where their NetLog file is saved to.
  // Instead, everything is saved on the user's temp directory. Thus the
  // mobile user has the UI available to send their NetLog file as an
  // email while the desktop user, who gets to choose their NetLog file's
  // location, does not. Furthermore, since every time a user starts logging
  // to a new NetLog file on mobile platforms it overwrites the previous
  // NetLog file, a warning message appears on the Start Logging button
  // that informs the user of this. This does not exist on the desktop
  // UI.
  static bool UsingMobileUI();

  // Fires net-log-info-changed event to update the JavaScript UI in the
  // renderer.
  void NotifyUIWithState(const base::Value::Dict& state);

  // Opens the SelectFileDialog UI with the default path to save a
  // NetLog file.
  void ShowSelectFileDialog(const base::FilePath& default_path);

  // Cached pointer to SystemNetworkContextManager's NetExportFileWriter.
  raw_ptr<net_log::NetExportFileWriter> file_writer_;

  base::ScopedObservation<net_log::NetExportFileWriter,
                          net_log::NetExportFileWriter::StateObserver>
      state_observation_manager_{this};

  // The capture mode and file size bound that the user chose in the UI when
  // logging started is cached here and is read after a file path is chosen in
  // the save dialog. Their values are only valid while the save dialog is open
  // on the desktop UI.
  net::NetLogCaptureMode capture_mode_;
  uint64_t max_log_file_size_;

  scoped_refptr<ui::SelectFileDialog> select_file_dialog_;

  base::WeakPtrFactory<NetExportMessageHandler> weak_ptr_factory_{this};
};

NetExportMessageHandler::NetExportMessageHandler()
    : file_writer_(g_browser_process->system_network_context_manager()
                       ->GetNetExportFileWriter()) {
  file_writer_->Initialize();
}

NetExportMessageHandler::~NetExportMessageHandler() {
  // There may be a pending file dialog, it needs to be told that the user
  // has gone away so that it doesn't try to call back.
  if (select_file_dialog_)
    select_file_dialog_->ListenerDestroyed();

  file_writer_->StopNetLog();
}

void NetExportMessageHandler::RegisterMessages() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  web_ui()->RegisterMessageCallback(
      net_log::kEnableNotifyUIWithStateHandler,
      base::BindRepeating(&NetExportMessageHandler::OnEnableNotifyUIWithState,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      net_log::kStartNetLogHandler,
      base::BindRepeating(&NetExportMessageHandler::OnStartNetLog,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      net_log::kStopNetLogHandler,
      base::BindRepeating(&NetExportMessageHandler::OnStopNetLog,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      net_log::kSendNetLogHandler,
      base::BindRepeating(&NetExportMessageHandler::OnSendNetLog,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      net_log::kShowFile,
      base::BindRepeating(&NetExportMessageHandler::OnShowFile,
                          base::Unretained(this)));
}

// The net-export UI is not notified of state changes until this function runs.
// After this function, NotifyUIWithState() will be called on all |file_writer_|
// state changes.
void NetExportMessageHandler::OnEnableNotifyUIWithState(
    const base::Value::List& list) {
  AllowJavascript();
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!state_observation_manager_.IsObserving()) {
    state_observation_manager_.Observe(file_writer_.get());
  }
  NotifyUIWithState(file_writer_->GetState());
}

void NetExportMessageHandler::OnStartNetLog(const base::Value::List& params) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Determine the capture mode.
  capture_mode_ = net::NetLogCaptureMode::kDefault;
  if (!params.empty() && params[0].is_string()) {
    capture_mode_ = net_log::NetExportFileWriter::CaptureModeFromString(
        params[0].GetString());
  }

  // Determine the max file size.
  max_log_file_size_ = net_log::NetExportFileWriter::kNoLimit;
  if (params.size() > 1 && params[1].is_int() && params[1].GetInt() > 0) {
    max_log_file_size_ = params[1].GetInt();
  }

  if (UsingMobileUI()) {
    StartNetLog(base::FilePath());
  } else {
    base::FilePath initial_dir = last_save_dir.Pointer()->empty() ?
        DownloadPrefs::FromBrowserContext(
            web_ui()->GetWebContents()->GetBrowserContext())->DownloadPath() :
        *last_save_dir.Pointer();
    base::FilePath initial_path =
        initial_dir.Append(FILE_PATH_LITERAL("chrome-net-export-log.json"));
    ShowSelectFileDialog(initial_path);
  }
}

void NetExportMessageHandler::OnStopNetLog(const base::Value::List& list) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  base::Value::Dict ui_thread_polled_data;

  Profile* profile = Profile::FromWebUI(web_ui());
  ui_thread_polled_data.Set("prerenderInfo",
                            chrome_browser_net::GetPrerenderInfo(profile));
  ui_thread_polled_data.Set("extensionInfo",
                            chrome_browser_net::GetExtensionInfo(profile));
#if BUILDFLAG(IS_WIN)
  ui_thread_polled_data.Set("serviceProviders",
                            chrome_browser_net::GetWindowsServiceProviders());
#endif

  file_writer_->StopNetLog(std::move(ui_thread_polled_data));
}

void NetExportMessageHandler::OnSendNetLog(const base::Value::List& list) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  file_writer_->GetFilePathToCompletedLog(
      base::BindOnce(&NetExportMessageHandler::SendEmail));
}

void NetExportMessageHandler::OnShowFile(const base::Value::List& list) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  file_writer_->GetFilePathToCompletedLog(
      base::BindOnce(&NetExportMessageHandler::ShowFileInShell,
                     weak_ptr_factory_.GetWeakPtr()));
}

void NetExportMessageHandler::FileSelected(const ui::SelectedFileInfo& file,
                                           int index) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(select_file_dialog_);
  *last_save_dir.Pointer() = file.path().DirName();

  StartNetLog(file.path());

  // IMPORTANT: resetting the dialog may lead to the deletion of |path|, so keep
  // this line last.
  select_file_dialog_ = nullptr;
}

void NetExportMessageHandler::FileSelectionCanceled() {
  DCHECK(select_file_dialog_);
  select_file_dialog_ = nullptr;
}

void NetExportMessageHandler::OnNewState(const base::Value::Dict& state) {
  NotifyUIWithState(state);
}

// static
void NetExportMessageHandler::SendEmail(const base::FilePath& file_to_send) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
#if BUILDFLAG(IS_ANDROID)
  if (file_to_send.empty())
    return;
  std::string email;
  std::string subject = "net_internals_log";
  std::string title = "Issue number: ";
  std::string body =
      "Please add some informative text about the network issues.";
  base::FilePath::StringType file_to_attach(file_to_send.value());
  browser_ui::SendEmail(base::UTF8ToUTF16(email), base::UTF8ToUTF16(subject),
                        base::UTF8ToUTF16(body), base::UTF8ToUTF16(title),
                        base::UTF8ToUTF16(file_to_attach));
#endif
}

void NetExportMessageHandler::StartNetLog(const base::FilePath& path) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  file_writer_->StartNetLog(
      path, capture_mode_, max_log_file_size_,
      base::CommandLine::ForCurrentProcess()->GetCommandLineString(),
      chrome::GetChannelName(chrome::WithExtendedStable(true)),
      Profile::FromWebUI(web_ui())
          ->GetDefaultStoragePartition()
          ->GetNetworkContext());
}

void NetExportMessageHandler::ShowFileInShell(const base::FilePath& path) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (path.empty())
    return;

  // (The |profile| parameter is relevant for Chrome OS)
  Profile* profile = Profile::FromWebUI(web_ui());

  platform_util::ShowItemInFolder(profile, path);
}

// static
bool NetExportMessageHandler::UsingMobileUI() {
#if BUILDFLAG(IS_ANDROID)
  return true;
#else
  return false;
#endif
}

void NetExportMessageHandler::NotifyUIWithState(
    const base::Value::Dict& state) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(web_ui());
  FireWebUIListener(net_log::kNetLogInfoChangedEvent, state);
}

void NetExportMessageHandler::ShowSelectFileDialog(
    const base::FilePath& default_path) {
  // User may have clicked more than once before the save dialog appears.
  // This prevents creating more than one save dialog.
  if (select_file_dialog_)
    return;

  WebContents* webcontents = web_ui()->GetWebContents();

  select_file_dialog_ = ui::SelectFileDialog::Create(
      this, std::make_unique<ChromeSelectFilePolicy>(webcontents));
  ui::SelectFileDialog::FileTypeInfo file_type_info{
      {FILE_PATH_LITERAL("json")}};
  gfx::NativeWindow owning_window = webcontents->GetTopLevelNativeWindow();
  select_file_dialog_->SelectFile(
      ui::SelectFileDialog::SELECT_SAVEAS_FILE, std::u16string(), default_path,
      &file_type_info, 0, base::FilePath::StringType(), owning_window);
}

}  // namespace

NetExportUI::NetExportUI(content::WebUI* web_ui) : WebUIController(web_ui) {
  web_ui->AddMessageHandler(std::make_unique<NetExportMessageHandler>());

  // Set up the chrome://net-export/ source.
  CreateAndAddNetExportHTMLSource(Profile::FromWebUI(web_ui));
}
