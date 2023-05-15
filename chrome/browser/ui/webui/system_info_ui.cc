// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/system_info_ui.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/escape.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/feedback/system_logs/about_system_logs_fetcher.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/generated_resources.h"
#include "components/feedback/system_logs/system_logs_fetcher.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "net/base/directory_lister.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/webui/jstemplate_builder.h"
#include "ui/base/webui/web_ui_util.h"

using content::WebContents;
using content::WebUIMessageHandler;
using system_logs::SystemLogsResponse;

namespace {

void CreateAndAddSystemInfoUIDataSource(Profile* profile) {
  content::WebUIDataSource* html_source =
      content::WebUIDataSource::CreateAndAdd(profile,
                                             chrome::kChromeUISystemInfoHost);

  static constexpr webui::LocalizedString kStrings[] = {
      {"title", IDS_ABOUT_SYS_TITLE},
      {"description", IDS_ABOUT_SYS_DESC},
      {"tableTitle", IDS_ABOUT_SYS_TABLE_TITLE},
      {"logFileTableTitle", IDS_ABOUT_SYS_LOG_FILE_TABLE_TITLE},
      {"expandAllBtn", IDS_ABOUT_SYS_EXPAND_ALL},
      {"collapseAllBtn", IDS_ABOUT_SYS_COLLAPSE_ALL},
      {"expandBtn", IDS_ABOUT_SYS_EXPAND},
      {"collapseBtn", IDS_ABOUT_SYS_COLLAPSE},
      {"parseError", IDS_ABOUT_SYS_PARSE_ERROR},
  };
  html_source->AddLocalizedStrings(kStrings);

  html_source->AddResourcePath("about_sys.js", IDR_ABOUT_SYS_JS);
  html_source->AddResourcePath("about_sys.css", IDR_ABOUT_SYS_CSS);
  html_source->SetDefaultResource(IDR_ABOUT_SYS_HTML);
  html_source->UseStringsJs();
}

}  // namespace

// The handler for Javascript messages related to the "system" view.
class SystemInfoUIHandler : public WebUIMessageHandler {
 public:
  SystemInfoUIHandler();

  SystemInfoUIHandler(const SystemInfoUIHandler&) = delete;
  SystemInfoUIHandler& operator=(const SystemInfoUIHandler&) = delete;

  ~SystemInfoUIHandler() override;

  // WebUIMessageHandler implementation.
  void RegisterMessages() override;
  void OnJavascriptDisallowed() override;

  // Callback for the "requestSystemInfo" message. This asynchronously requests
  // system info and eventually returns it to the front end.
  void HandleRequestSystemInfo(const base::Value::List& args);

  void OnSystemInfo(std::unique_ptr<SystemLogsResponse> sys_info);

 private:
  std::string callback_id_;
  base::WeakPtrFactory<SystemInfoUIHandler> weak_ptr_factory_{this};
};

////////////////////////////////////////////////////////////////////////////////
//
// SystemInfoUIHandler
//
////////////////////////////////////////////////////////////////////////////////
SystemInfoUIHandler::SystemInfoUIHandler() {}

SystemInfoUIHandler::~SystemInfoUIHandler() {}

void SystemInfoUIHandler::OnJavascriptDisallowed() {
  weak_ptr_factory_.InvalidateWeakPtrs();
  callback_id_.clear();
}

void SystemInfoUIHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "requestSystemInfo",
      base::BindRepeating(&SystemInfoUIHandler::HandleRequestSystemInfo,
                          base::Unretained(this)));
}

void SystemInfoUIHandler::HandleRequestSystemInfo(
    const base::Value::List& args) {
  AllowJavascript();
  callback_id_ = args[0].GetString();

  system_logs::SystemLogsFetcher* fetcher =
      system_logs::BuildAboutSystemLogsFetcher();
  fetcher->Fetch(base::BindOnce(&SystemInfoUIHandler::OnSystemInfo,
                                weak_ptr_factory_.GetWeakPtr()));
}

void SystemInfoUIHandler::OnSystemInfo(
    std::unique_ptr<SystemLogsResponse> sys_info) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!sys_info)
    return;
  base::Value::List data;
  for (SystemLogsResponse::const_iterator it = sys_info->begin();
       it != sys_info->end(); ++it) {
    base::Value::Dict val;
    val.Set("statName", it->first);
    val.Set("statValue", it->second);
    data.Append(std::move(val));
  }
  ResolveJavascriptCallback(base::Value(callback_id_), data);
  callback_id_.clear();
}

////////////////////////////////////////////////////////////////////////////////
//
// SystemInfoUI
//
////////////////////////////////////////////////////////////////////////////////

SystemInfoUI::SystemInfoUI(content::WebUI* web_ui) : WebUIController(web_ui) {
  web_ui->AddMessageHandler(std::make_unique<SystemInfoUIHandler>());

  // Set up the chrome://system/ source.
  CreateAndAddSystemInfoUIDataSource(Profile::FromWebUI(web_ui));
}
