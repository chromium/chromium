// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/support_tool/support_tool_ui.h"

#include <set>
#include <string>
#include <vector>

#include "base/base64url.h"
#include "base/bind.h"
#include "base/check.h"
#include "base/containers/contains.h"
#include "base/strings/string_piece_forward.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/signin_ui_util.h"
#include "chrome/browser/support_tool/data_collection_module.pb.h"
#include "chrome/browser/support_tool/data_collector.h"
#include "chrome/browser/support_tool/support_tool_handler.h"
#include "chrome/browser/support_tool/support_tool_util.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/support_tool_resources.h"
#include "chrome/grit/support_tool_resources_map.h"
#include "components/feedback/pii_types.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "net/base/url_util.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
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
    case support_tool::CHROMEOS_DBUS:
      return "DBus Details";
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

std::set<support_tool::DataCollectorType> GetIncludedDataCollectorTypes(
    const base::Value::List* data_collector_items) {
  std::set<support_tool::DataCollectorType> included_data_collectors;
  for (const auto& item : *data_collector_items) {
    const base::Value::Dict* item_as_dict = item.GetIfDict();
    DCHECK(item_as_dict);
    absl::optional<bool> isIncluded = item_as_dict->FindBool("isIncluded");
    if (isIncluded && isIncluded.value()) {
      included_data_collectors.insert(
          static_cast<support_tool::DataCollectorType>(
              item_as_dict->FindInt("protoEnum").value()));
    }
  }
  return included_data_collectors;
}

// Returns the human readable name corresponding to `data_collector_type`.
std::string GetPIITypeDescription(feedback::PIIType type_enum) {
  // This function will return translatable strings in future. For now, return
  // string constants until we have the translatable strings ready.
  switch (type_enum) {
    case feedback::PIIType::kAndroidAppStoragePath:
      return "Android App Storage Paths";
    case feedback::PIIType::kBSSID:
      return "BSSID (Basic Service Set Identifier)";
    case feedback::PIIType::kCellID:
      return "Cell Tower Identifier";
    case feedback::PIIType::kEmail:
      return "Email Address";
    case feedback::PIIType::kGaiaID:
      return "GAIA (Google Accounts and ID Administration) ID";
    case feedback::PIIType::kHash:
      return "Hashes";
    case feedback::PIIType::kIPPAddress:
      return "IPP (Internet Printing Protocol) Addresses";
    case feedback::PIIType::kIPAddress:
      return "IP (Internet Protocol) Address";
    case feedback::PIIType::kLocationAreaCode:
      return "Location Area Code";
    case feedback::PIIType::kMACAddress:
      return "MAC Address";
    case feedback::PIIType::kUIHierarchyWindowTitles:
      return "Window Titles";
    case feedback::PIIType::kURL:
      return "URLs";
    case feedback::PIIType::kUUID:
      return "Universal Unique Identifiers (UUIDs)";
    case feedback::PIIType::kSerial:
      return "Serial Numbers";
    case feedback::PIIType::kSSID:
      return "SSID (Service Set Identifier)";
    case feedback::PIIType::kVolumeLabel:
      return "Volume Labels";
    default:
      return "Error: Undefined";
  }
}

// type PIIDataItem = {
//   piiTypeDescription: string,
//   piiType: number,
//   detectedData: string,
//   count: number,
//   keep: boolean,
// }
base::Value::List GetDetectedPIIDataItems(const PIIMap& detected_pii) {
  base::Value::List detected_pii_data_items;
  for (const auto& pii_entry : detected_pii) {
    base::Value::Dict pii_data_item;
    pii_data_item.Set("piiTypeDescription",
                      GetPIITypeDescription(pii_entry.first));
    pii_data_item.Set("piiType", static_cast<int>(pii_entry.first));
    pii_data_item.Set(
        "detectedData",
        base::JoinString(std::vector<base::StringPiece>(
                             pii_entry.second.begin(), pii_entry.second.end()),
                         ", "));
    pii_data_item.Set("count", static_cast<int>(pii_entry.second.size()));
    // TODO(b/200511640): Set `keep` field to the value we'll get from URL's
    // pii_masking_on query if it exists.
    pii_data_item.Set("keep", false);
    detected_pii_data_items.Append(base::Value(std::move(pii_data_item)));
  }
  return detected_pii_data_items;
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

  void OnDataCollectionDone(const PIIMap& detected_pii,
                            std::set<SupportToolError> errors);

  std::unique_ptr<SupportToolHandler> handler_;
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
  this->handler_ =
      GetSupportToolHandler(*issue_details->FindString("caseId"),
                            *issue_details->FindString("emailAddress"),
                            *issue_details->FindString("issueDescription"),
                            GetIncludedDataCollectorTypes(data_collectors));
  this->handler_->CollectSupportData(
      base::BindOnce(&SupportToolMessageHandler::OnDataCollectionDone,
                     weak_ptr_factory_.GetWeakPtr()));
}

void SupportToolMessageHandler::OnDataCollectionDone(
    const PIIMap& detected_pii,
    std::set<SupportToolError> errors) {
  AllowJavascript();
  FireWebUIListener("data-collection-completed",
                    base::Value(GetDetectedPIIDataItems(detected_pii)));
}

void SupportToolMessageHandler::HandleCancelDataCollection(
    const base::Value::List& args) {
  AllowJavascript();
  // Deleting the SupportToolHandler object will stop data collection.
  this->handler_.reset();
  FireWebUIListener("data-collection-cancelled");
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
