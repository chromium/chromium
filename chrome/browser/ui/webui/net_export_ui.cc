// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/net_export_ui.h"

#include <stdint.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/scoped_observer.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
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

#if defined(OS_ANDROID)
#include "chrome/browser/android/intent_helper.h"
#endif

using content::BrowserThread;
using content::WebContents;
using content::WebUIMessageHandler;

namespace {

// May only be accessed on the UI thread
base::LazyInstance<base::FilePath>::Leaky
    last_save_dir = LAZY_INSTANCE_INITIALIZER;

content::WebUIDataSource* CreateNetExportHTMLSource() {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUINetExportHost);

  source->UseStringsJs();
  source->AddResourcePath(net_log::kNetExportUIJS, IDR_NET_LOG_NET_EXPORT_JS);
  source->SetDefaultResource(IDR_NET_LOG_NET_EXPORT_HTML);
  return source;
}

void SetIfNotNull(base::DictionaryValue* dict,
                  const base::StringPiece& path,
                  std::unique_ptr<base::Value> in_value) {
  if (in_value) {
    dict->Set(path, std::move(in_value));
  }
}

// This class receives javascript messages from the renderer.
// Note that the WebUI infrastructure runs on the UI thread, therefore all of
// this class's public methods are expected to run on the UI thread.
class NetExportMessageHandler
    : public WebUIMessageHandler,
      public base::SupportsWeakPtr<NetExportMessageHandler>,
      public ui::SelectFileDialog::Listener,
      public net_log::NetExportFileWriter::StateObserver {
 public:
  NetExportMessageHandler();
  ~NetExportMessageHandler() override;

  // WebUIMessageHandler implementation.
  void RegisterMessages() override;

  // Messages
  void OnEnableNotifyUIWithState(const base::ListValue* list);
  void OnStartNetLog(const base::ListValue* list);
  void OnStopNetLog(const base::ListValue* list);
  void OnSendNetLog(const base::ListValue* list);
  void OnShowFile(const base::ListValue* list);

  // ui::SelectFileDialog::Listener implementation.
  void FileSelected(const base::FilePath& path,
                    int index,
                    void* params) override;
  void FileSelectionCanceled(void* params) override;

  // net_log::NetExportFileWriter::StateObserver implementation.
  void OnNewState(const base::DictionaryValue& state) override;

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

  // Calls NetExportView.onExportNetLogInfoChanged JavaScript function in the
  // renderer.
  void NotifyUIWithState(std::unique_ptr<base::DictionaryValue> state);

  // Opens the SelectFileDialog UI with the default path to save a
  // NetLog file.
  void ShowSelectFileDialog(const base::FilePath& default_path);

  // Cached pointer to SystemNetworkContextManager's NetExportFileWriter.
  net_log::NetExportFileWriter* file_writer_;

  ScopedObserver<net_log::NetExportFileWriter,
                 net_log::NetExportFileWriter::StateObserver>
      state_observer_manager_;

  // The capture mode and file size bound that the user chose in the UI when
  // logging started is cached here and is read after a file path is chosen in
  // the save dialog. Their values are only valid while the save dialog is open
  // on the desktop UI.
  net::NetLogCaptureMode capture_mode_;
  uint64_t max_log_file_size_;

  scoped_refptr<ui::SelectFileDialog> select_file_dialog_;

  base::WeakPtrFactory<NetExportMessageHandler> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(NetExportMessageHandler);
};

NetExportMessageHandler::NetExportMessageHandler()
    : file_writer_(g_browser_process->system_network_context_manager()
                       ->GetNetExportFileWriter()),
      state_observer_manager_(this) {
  file_writer_->Initialize();
}

NetExportMessageHandler::~NetExportMessageHandler() {
  // There may be a pending file dialog, it needs to be told that the user
  // has gone away so that it doesn't try to call back.
  if (select_file_dialog_)
    select_file_dialog_->ListenerDestroyed();

  file_writer_->StopNetLog(nullptr);
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
    const base::ListValue* list) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!state_observer_manager_.IsObservingSources()) {
    state_observer_manager_.Add(file_writer_);
  }
  NotifyUIWithState(file_writer_->GetState());
}

void NetExportMessageHandler::OnStartNetLog(const base::ListValue* list) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  base::Value::ConstListView params = list->GetList();

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

void NetExportMessageHandler::OnStopNetLog(const base::ListValue* list) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  std::unique_ptr<base::DictionaryValue> ui_thread_polled_data(
      new base::DictionaryValue());

  Profile* profile = Profile::FromWebUI(web_ui());
  SetIfNotNull(ui_thread_polled_data.get(), "prerenderInfo",
               chrome_browser_net::GetPrerenderInfo(profile));
  SetIfNotNull(ui_thread_polled_data.get(), "extensionInfo",
               chrome_browser_net::GetExtensionInfo(profile));
#if defined(OS_WIN)
  SetIfNotNull(ui_thread_polled_data.get(), "serviceProviders",
               chrome_browser_net::GetWindowsServiceProviders());
#endif

  file_writer_->StopNetLog(std::move(ui_thread_polled_data));
}

void NetExportMessageHandler::OnSendNetLog(const base::ListValue* list) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  file_writer_->GetFilePathToCompletedLog(
      base::BindOnce(&NetExportMessageHandler::SendEmail));
}

void NetExportMessageHandler::OnShowFile(const base::ListValue* list) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  file_writer_->GetFilePathToCompletedLog(
      base::BindOnce(&NetExportMessageHandler::ShowFileInShell, AsWeakPtr()));
}

void NetExportMessageHandler::FileSelected(const base::FilePath& path,
                                           int index,
                                           void* params) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(select_file_dialog_);
  *last_save_dir.Pointer() = path.DirName();

  StartNetLog(path);

  // IMPORTANT: resetting the dialog may lead to the deletion of |path|, so keep
  // this line last.
  select_file_dialog_ = nullptr;
}

void NetExportMessageHandler::FileSelectionCanceled(void* params) {
  DCHECK(select_file_dialog_);
  select_file_dialog_ = nullptr;
}

void NetExportMessageHandler::OnNewState(const base::DictionaryValue& state) {
  NotifyUIWithState(state.CreateDeepCopy());
}

// static
void NetExportMessageHandler::SendEmail(const base::FilePath& file_to_send) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
#if defined(OS_ANDROID)
  if (file_to_send.empty())
    return;
  std::string email;
  std::string subject = "net_internals_log";
  std::string title = "Issue number: ";
  std::string body =
      "Please add some informative text about the network issues.";
  base::FilePath::StringType file_to_attach(file_to_send.value());
  chrome::android::SendEmail(
      base::UTF8ToUTF16(email), base::UTF8ToUTF16(subject),
      base::UTF8ToUTF16(body), base::UTF8ToUTF16(title),
      base::UTF8ToUTF16(file_to_attach));
#endif
}

void NetExportMessageHandler::StartNetLog(const base::FilePath& path) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  file_writer_->StartNetLog(
      path, capture_mode_, max_log_file_size_,
      base::CommandLine::ForCurrentProcess()->GetCommandLineString(),
      chrome::GetChannelName(),
      content::BrowserContext::GetDefaultStoragePartition(
          Profile::FromWebUI(web_ui()))
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
#if defined(OS_ANDROID)
  return true;
#else
  return false;
#endif
}

void NetExportMessageHandler::NotifyUIWithState(
    std::unique_ptr<base::DictionaryValue> state) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(web_ui());
  web_ui()->CallJavascriptFunctionUnsafe(net_log::kOnExportNetLogInfoChanged,
                                         *state);
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
  ui::SelectFileDialog::FileTypeInfo file_type_info;
  file_type_info.extensions = {{FILE_PATH_LITERAL("json")}};
  gfx::NativeWindow owning_window = webcontents->GetTopLevelNativeWindow();
  select_file_dialog_->SelectFile(
      ui::SelectFileDialog::SELECT_SAVEAS_FILE, base::string16(), default_path,
      &file_type_info, 0, base::FilePath::StringType(), owning_window, nullptr);
}

}  // namespace

NetExportUI::NetExportUI(content::WebUI* web_ui) : WebUIController(web_ui) {
  web_ui->AddMessageHandler(std::make_unique<NetExportMessageHandler>());

  // Set up the chrome://net-export/ source.
  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource::Add(profile, CreateNetExportHTMLSource());
}
