// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ui/webui/system/system_info_ui.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/escape.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/feedback/system_logs/about_system_logs_fetcher.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/about_sys_resources.h"
#include "chrome/grit/about_sys_resources_map.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/key_value_pair_viewer_shared_resources.h"
#include "chrome/grit/key_value_pair_viewer_shared_resources_map.h"
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

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/crosapi/browser_manager.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/common/webui_url_constants.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

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

#if BUILDFLAG(IS_CHROMEOS)
#if BUILDFLAG(IS_CHROMEOS_ASH)
  std::u16string other_system_page_url(chrome::kChromeUISystemURL16);
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  std::u16string other_system_page_url(chrome::kOsUISystemURL);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  auto os_link_container = l10n_util::GetStringFUTF16(
      IDS_ABOUT_SYS_OS_LINK_CONTAINER, other_system_page_url);
  html_source->AddString("osLinkContainer", os_link_container);
#endif  // BUILDFLAG(IS_CHROMEOS)

  webui::SetupWebUIDataSource(
      html_source, base::make_span(kAboutSysResources, kAboutSysResourcesSize),
      IDR_ABOUT_SYS_ABOUT_SYS_HTML);
  html_source->AddResourcePaths(
      base::make_span(kKeyValuePairViewerSharedResources,
                      kKeyValuePairViewerSharedResourcesSize));
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

  // Callbacks for SystemInfo request messages. This asynchronously requests
  // system info and eventually returns it to the front end.
  void HandleRequestSystemInfo(const base::Value::List& args);

  void OnSystemInfo(std::unique_ptr<SystemLogsResponse> sys_info);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  void IsLacrosEnabled(const base::Value::List& args);
  void OpenLacrosSystemPage(const base::Value::List& args);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

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

#if BUILDFLAG(IS_CHROMEOS_ASH)
  web_ui()->RegisterMessageCallback(
      "isLacrosEnabled",
      base::BindRepeating(&SystemInfoUIHandler::IsLacrosEnabled,
                          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "openLacrosSystemPage",
      base::BindRepeating(&SystemInfoUIHandler::OpenLacrosSystemPage,
                          base::Unretained(this)));
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

void SystemInfoUIHandler::HandleRequestSystemInfo(
    const base::Value::List& args) {
  AllowJavascript();
  callback_id_ = args[0].GetString();

  system_logs::SystemLogsFetcher* fetcher =
      system_logs::BuildAboutSystemLogsFetcher(web_ui());
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

#if BUILDFLAG(IS_CHROMEOS_ASH)
void SystemInfoUIHandler::IsLacrosEnabled(const base::Value::List& args) {
  CHECK_EQ(1U, args.size());

  AllowJavascript();
  const bool is_lacros_enabled = crosapi::browser_util::IsLacrosEnabled();
  std::string callback_id = args[0].GetString();
  ResolveJavascriptCallback(base::Value(callback_id),
                            base::Value(is_lacros_enabled));
}

void SystemInfoUIHandler::OpenLacrosSystemPage(const base::Value::List& args) {
  // Note: This will only be called by the UI when Lacros is available.
  CHECK(crosapi::BrowserManager::Get());
  crosapi::BrowserManager::Get()->SwitchToTab(
      GURL(chrome::kChromeUISystemURL),
      /*path_behavior=*/NavigateParams::RESPECT);
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

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
