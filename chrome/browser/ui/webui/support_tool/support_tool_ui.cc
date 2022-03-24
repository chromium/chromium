// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/support_tool/support_tool_ui.h"

#include <string>

#include "base/base64url.h"
#include "base/check.h"
#include "base/containers/contains.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/signin_ui_util.h"
#include "chrome/browser/support_tool/data_collection_module.pb.h"
#include "chrome/browser/support_tool/support_tool_util.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/support_tool_resources.h"
#include "chrome/grit/support_tool_resources_map.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "net/base/url_util.h"
#include "url/gurl.h"

const char kSupportCaseIDQuery[] = "case_id";
const char kModuleQuery[] = "module";

namespace {
// Returns the support case ID that's extracted from `url` with query
// `kSupportCaseIDQuery`. Returns empty string if `url` doesn't contain support
// case ID.
std::string GetSupportCaseIDFromURL(const GURL& url) {
  std::string support_case_id;
  if (url.has_query()) {
    net::GetValueForKeyInQuery(url, kSupportCaseIDQuery, &support_case_id);
  }
  return support_case_id;
}

content::WebUIDataSource* CreateSupportToolHTMLSource(const GURL& url) {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUISupportToolHost);

  source->AddString("caseId", GetSupportCaseIDFromURL(url));

  webui::SetupWebUIDataSource(
      source, base::make_span(kSupportToolResources, kSupportToolResourcesSize),
      IDR_SUPPORT_TOOL_SUPPORT_TOOL_CONTAINER_HTML);

  return source;
}

// Returns the human readable name corresponding to `data_collector_type`.
std::string GetDataCollectorName(
    support_tool::DataCollectorType data_collector_type) {
  // This function will return translatable strings in future. For now, return
  // string constants until we have the translatable strings ready.
  switch (data_collector_type) {
    case support_tool::CHROME_INTERNAL:
      return "Internal";
    case support_tool::CRASH_IDS:
      return "Crash IDs";
    case support_tool::MEMORY_DETAILS:
      return "Memory Details";
    case support_tool::CHROMEOS_UI_HIERARCHY:
      return "UI Hierarchy";
    case support_tool::CHROMEOS_COMMAND_LINE:
      return "Command Line";
    case support_tool::CHROMEOS_DEVICE_EVENT:
      return "Device Event";
    case support_tool::CHROMEOS_IWL_WIFI_DUMP:
      return "IWL WiFi Dump";
    case support_tool::CHROMEOS_TOUCH_EVENTS:
      return "Touch Events";
    case support_tool::CHROMEOS_CROS_API:
      return "CROS API";
    case support_tool::CHROMEOS_LACROS:
      return "Lacros";
    case support_tool::CHROMEOS_REVEN:
      return "Chrome OS Reven";
    default:
      return "Error: Undefined";
  }
}

// Decodes `module_query` string and initializes contents of `module`.
void InitDataCollectionModuleFromURLQuery(
    support_tool::DataCollectionModule* module,
    const std::string& module_query) {
  std::string query_decoded;
  if (!module_query.empty() &&
      base::Base64UrlDecode(module_query,
                            base::Base64UrlDecodePolicy::IGNORE_PADDING,
                            &query_decoded)) {
    module->ParseFromString(query_decoded);
  }
}

base::Value::Dict GetDataCollectorItemForType(
    const support_tool::DataCollectionModule& module,
    const support_tool::DataCollectorType& type) {
  base::Value::Dict dict;
  dict.Set("name", GetDataCollectorName(type));
  dict.Set("protoEnum", type);
  dict.Set("isIncluded",
           base::Contains(module.included_data_collectors(), type));
  return dict;
}

// Creates base::Value::List according to the format Support Tool UI
// accepts and fills the contents with by decoding `module_query` to its
// support_tool.pb components. Support Tool UI requests data collector items in
// format:
// type DataCollectorItem = {
//  name: string,
//  isIncluded: boolean,
//  protoEnum: number,
// }
base::Value::List GetDataCollectorItemsInQuery(std::string module_query) {
  base::Value::List data_collector_list;
  support_tool::DataCollectionModule module;
  InitDataCollectionModuleFromURLQuery(&module, module_query);
  for (const auto& type : kDataCollectors) {
    data_collector_list.Append(GetDataCollectorItemForType(module, type));
  }
#if BUILDFLAG(IS_CHROMEOS_ASH)
  for (const auto& type : kDataCollectorsChromeosAsh) {
    data_collector_list.Append(GetDataCollectorItemForType(module, type));
  }
#if BUILDFLAG(IS_CHROMEOS_WITH_HW_DETAILS)
  for (const auto& type : kDataCollectorsChromeosHwDetails) {
    data_collector_list.Append(GetDataCollectorItemForType(module, type));
  }
#endif  // BUILDFLAG(IS_CHROMEOS_WITH_HW_DETAILS)
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  return data_collector_list;
}
}  // namespace

////////////////////////////////////////////////////////////////////////////////
//
// SupportToolMessageHandler
//
////////////////////////////////////////////////////////////////////////////////

// The handler for Javascript messages related to Support Tool.
class SupportToolMessageHandler : public content::WebUIMessageHandler {
 public:
  SupportToolMessageHandler() = default;

  SupportToolMessageHandler(const SupportToolMessageHandler&) = delete;
  SupportToolMessageHandler& operator=(const SupportToolMessageHandler&) =
      delete;

  ~SupportToolMessageHandler() override = default;

  // WebUIMessageHandler implementation.
  void RegisterMessages() override;

  void HandleGetEmailAddresses(const base::Value::List& args);

  void HandleGetDataCollectors(const base::Value::List& args);

  void HandleStartDataCollection(const base::Value::List& args);

  void HandleCancelDataCollection(const base::Value::List& args);

 private:
  base::Value::List GetAccountsList();

  base::WeakPtrFactory<SupportToolMessageHandler> weak_ptr_factory_{this};
};

void SupportToolMessageHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "getEmailAddresses",
      base::BindRepeating(&SupportToolMessageHandler::HandleGetEmailAddresses,
                          weak_ptr_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "getDataCollectors",
      base::BindRepeating(&SupportToolMessageHandler::HandleGetDataCollectors,
                          weak_ptr_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "startDataCollection",
      base::BindRepeating(&SupportToolMessageHandler::HandleStartDataCollection,
                          weak_ptr_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "cancelDataCollection",
      base::BindRepeating(
          &SupportToolMessageHandler::HandleCancelDataCollection,
          weak_ptr_factory_.GetWeakPtr()));
}

base::Value::List SupportToolMessageHandler::GetAccountsList() {
  Profile* profile = Profile::FromWebUI(web_ui());
  base::Value::List account_list;

  // Guest mode does not have a primary account (or an IdentityManager).
  if (profile->IsGuestSession())
    return account_list;

  for (const auto& account : signin_ui_util::GetOrderedAccountsForDisplay(
           profile, /*restrict_to_accounts_eligible_for_sync=*/false)) {
    if (!account.IsEmpty())
      account_list.Append(base::Value(account.email));
  }
  return account_list;
}

void SupportToolMessageHandler::HandleGetEmailAddresses(
    const base::Value::List& args) {
  AllowJavascript();
  CHECK_EQ(1U, args.size());
  const base::Value& callback_id = args[0];

  ResolveJavascriptCallback(callback_id, base::Value(GetAccountsList()));
}

void SupportToolMessageHandler::HandleGetDataCollectors(
    const base::Value::List& args) {
  AllowJavascript();
  CHECK_EQ(1U, args.size());
  const base::Value& callback_id = args[0];

  std::string module_query;
  net::GetValueForKeyInQuery(web_ui()->GetWebContents()->GetURL(), kModuleQuery,
                             &module_query);

  ResolveJavascriptCallback(
      callback_id, base::Value(GetDataCollectorItemsInQuery(module_query)));
}

void SupportToolMessageHandler::HandleStartDataCollection(
    const base::Value::List& args) {
  CHECK_EQ(2U, args.size());
  const base::Value::Dict* issue_details = args[0].GetIfDict();
  DCHECK(issue_details);
  const base::Value::List* data_collectors = args[1].GetIfList();
  DCHECK(data_collectors);
  // TODO(b/219730597): Create SupportToolHandler from `issue_details` and
  // `data_collectors`. Will be added in follow-up CL.
}

void SupportToolMessageHandler::HandleCancelDataCollection(
    const base::Value::List& args) {
  // TODO(b/200511640): Cancel data collection.
}

////////////////////////////////////////////////////////////////////////////////
//
// SupportToolUI
//
////////////////////////////////////////////////////////////////////////////////

SupportToolUI::SupportToolUI(content::WebUI* web_ui) : WebUIController(web_ui) {
  web_ui->AddMessageHandler(std::make_unique<SupportToolMessageHandler>());

  // Set up the chrome://support-tool/ source.
  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource::Add(
      profile, CreateSupportToolHTMLSource(web_ui->GetWebContents()->GetURL()));
}

SupportToolUI::~SupportToolUI() = default;
