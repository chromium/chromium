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
#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/string_piece_forward.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/signin_ui_util.h"
#include "chrome/browser/support_tool/data_collection_module.pb.h"
#include "chrome/browser/support_tool/data_collector.h"
#include "chrome/browser/support_tool/support_tool_handler.h"
#include "chrome/browser/support_tool/support_tool_util.h"
#include "chrome/browser/ui/chrome_select_file_policy.h"
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
#include "ui/gfx/native_widget_types.h"
#include "ui/shell_dialogs/select_file_dialog.h"
#include "url/gurl.h"
#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/file_manager/path_util.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

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

  source->AddResourcePath("url-generator",
                          IDR_SUPPORT_TOOL_URL_GENERATOR_CONTAINER_HTML);

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

// Returns data collector item for `type`. Sets isIncluded field true if
// `module` contains `type`.
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

// Returns data collector item for `type`. Sets isIncluded to false for all data
// collector items.
base::Value::Dict GetDataCollectorItemForType(
    const support_tool::DataCollectorType& type) {
  base::Value::Dict dict;
  dict.Set("name", GetDataCollectorName(type));
  dict.Set("protoEnum", type);
  dict.Set("isIncluded", false);
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
// Returns only the data collectors that are available for user's device.
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

// Creates base::Value::List according to the format Support Tool UI
// accepts and fills the contents with all data collectors with isIncluded:
// false as a default choice. Support Tool UI requests data collector items in
// format:
// type DataCollectorItem = {
//  name: string,
//  isIncluded: boolean,
//  protoEnum: number,
// }
base::Value::List GetAllDataCollectors() {
  base::Value::List data_collector_list;
  for (const auto& type : kDataCollectors) {
    data_collector_list.Append(GetDataCollectorItemForType(type));
  }
  for (const auto& type : kDataCollectorsChromeosAsh) {
    data_collector_list.Append(GetDataCollectorItemForType(type));
  }
  for (const auto& type : kDataCollectorsChromeosHwDetails) {
    data_collector_list.Append(GetDataCollectorItemForType(type));
  }
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

// Returns start data collection result in a structure that Support Tool UI
// accepts. The returned type is as follow: type StartDataCollectionResult = {
//   success: boolean,
//   errorMessage: string,
// }
base::Value GetStartDataCollectionResult(bool success,
                                         std::string error_message) {
  base::Value::Dict result;
  result.Set("success", success);
  result.Set("errorMessage", error_message);
  return base::Value(std::move(result));
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

// Returns the current time in YYYY_MM_DD_HH_mm format.
std::string GetTimestampString(base::Time timestamp) {
  base::Time::Exploded tex;
  timestamp.LocalExplode(&tex);
  return base::StringPrintf("%04d_%02d_%02d_%02d_%02d", tex.year, tex.month,
                            tex.day_of_month, tex.hour, tex.minute);
}

base::FilePath GetDefaultFileToExport(base::FilePath suggested_path,
                                      const std::string& case_id,
                                      base::Time timestamp) {
  std::string timestamp_string = GetTimestampString(timestamp);
  std::string filename =
      case_id.empty()
          ? base::StringPrintf("support_packet_%s", timestamp_string.c_str())
          : base::StringPrintf("support_packet_%s_%s", case_id.c_str(),
                               timestamp_string.c_str());
  return suggested_path.AppendASCII(filename);
}

std::set<feedback::PIIType> GetSelectedPIIToKeep(
    const base::Value::List* pii_items) {
  std::set<feedback::PIIType> pii_to_keep;
  for (const auto& item : *pii_items) {
    const base::Value::Dict* item_as_dict = item.GetIfDict();
    DCHECK(item_as_dict);
    absl::optional<bool> keep = item_as_dict->FindBool("keep");
    if (keep && keep.value()) {
      pii_to_keep.insert(static_cast<feedback::PIIType>(
          item_as_dict->FindInt("piiType").value()));
    }
  }
  return pii_to_keep;
}

std::string GetDataCollectionModuleQuery(
    std::set<support_tool::DataCollectorType> included_data_collectors) {
  support_tool::DataCollectionModule module;
  for (const auto& data_collector : included_data_collectors) {
    module.add_included_data_collectors(data_collector);
  }
  std::string module_serialized;
  module.SerializeToString(&module_serialized);
  std::string data_collection_url_query;
  base::Base64UrlEncode(module_serialized,
                        base::Base64UrlEncodePolicy::OMIT_PADDING,
                        &data_collection_url_query);
  return data_collection_url_query;
}

// Returns a URL generation result in the type Support Tool UI expects.
// type UrlGenerationResult = {
//   success: boolean,
//   url: string,
//   errorMessage: string,
// }
base::Value::Dict GetURLGenerationResult(bool success,
                                         std::string url,
                                         std::string error_message) {
  base::Value::Dict url_generation_response;
  url_generation_response.Set("success", success);
  url_generation_response.Set("url", url);
  url_generation_response.Set("errorMessage", error_message);
  return url_generation_response;
}

// Generates a customized chrome://support-tool URL from given `case_id` and
// `data_collector_items` and returns the result in a format Support Tool UI
// expects. Returns a result with error when there's no data collector selected
// in `data_collector_items`.
base::Value::Dict GenerateCustomizedURL(
    std::string case_id,
    const base::Value::List* data_collector_items) {
  base::Value::Dict url_generation_response;
  std::set<support_tool::DataCollectorType> included_data_collectors =
      GetIncludedDataCollectorTypes(data_collector_items);
  if (included_data_collectors.empty()) {
    // If there's no selected data collector to add, consider this as an error.
    return GetURLGenerationResult(
        /*success=*/false, /*url=*/std::string(), /*error_message=*/
        "No data collectors included. Please select a data collector.");
  }
  GURL customized_url("chrome://support-tool");
  if (!case_id.empty()) {
    customized_url =
        net::AppendQueryParameter(customized_url, kSupportCaseIDQuery, case_id);
  }
  customized_url = net::AppendQueryParameter(
      customized_url, kModuleQuery,
      GetDataCollectionModuleQuery(included_data_collectors));
  return GetURLGenerationResult(/*success=*/true, /*url=*/customized_url.spec(),
                                /*error_message=*/std::string());
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
//
// SupportToolMessageHandler
//
////////////////////////////////////////////////////////////////////////////////

// The handler for Javascript messages related to Support Tool.
class SupportToolMessageHandler : public content::WebUIMessageHandler,
                                  public ui::SelectFileDialog::Listener {
 public:
  SupportToolMessageHandler() = default;

  SupportToolMessageHandler(const SupportToolMessageHandler&) = delete;
  SupportToolMessageHandler& operator=(const SupportToolMessageHandler&) =
      delete;

  ~SupportToolMessageHandler() override;

  // WebUIMessageHandler implementation.
  void RegisterMessages() override;

  void HandleGetEmailAddresses(const base::Value::List& args);

  void HandleGetDataCollectors(const base::Value::List& args);

  void HandleGetAllDataCollectors(const base::Value::List& args);

  void HandleStartDataCollection(const base::Value::List& args);

  void HandleCancelDataCollection(const base::Value::List& args);

  void HandleStartDataExport(const base::Value::List& args);

  void HandleShowExportedDataInFolder(const base::Value::List& args);

  void HandleGenerateCustomizedURL(const base::Value::List& args);

  // SelectFileDialog::Listener implementation.
  void FileSelected(const base::FilePath& path,
                    int index,
                    void* params) override;

  void FileSelectionCanceled(void* params) override;

 private:
  base::Value::List GetAccountsList();

  void OnDataCollectionDone(const PIIMap& detected_pii,
                            std::set<SupportToolError> errors);

  void OnDataExportDone(base::FilePath path, std::set<SupportToolError> errors);

  std::set<feedback::PIIType> selected_pii_to_keep_;
  base::Time data_collection_time_;
  base::FilePath data_path_;
  std::unique_ptr<SupportToolHandler> handler_;
  scoped_refptr<ui::SelectFileDialog> select_file_dialog_;
  base::WeakPtrFactory<SupportToolMessageHandler> weak_ptr_factory_{this};
};

SupportToolMessageHandler::~SupportToolMessageHandler() {
  if (select_file_dialog_) {
    select_file_dialog_->ListenerDestroyed();
  }
}

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
      "getAllDataCollectors",
      base::BindRepeating(
          &SupportToolMessageHandler::HandleGetAllDataCollectors,
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
  web_ui()->RegisterMessageCallback(
      "startDataExport",
      base::BindRepeating(&SupportToolMessageHandler::HandleStartDataExport,
                          weak_ptr_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "showExportedDataInFolder",
      base::BindRepeating(
          &SupportToolMessageHandler::HandleShowExportedDataInFolder,
          weak_ptr_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "generateCustomizedURL",
      base::BindRepeating(
          &SupportToolMessageHandler::HandleGenerateCustomizedURL,
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

void SupportToolMessageHandler::HandleGetAllDataCollectors(
    const base::Value::List& args) {
  AllowJavascript();
  CHECK_EQ(1U, args.size());
  const base::Value& callback_id = args[0];
  ResolveJavascriptCallback(callback_id, base::Value(GetAllDataCollectors()));
}

// Starts data collection with the issue details and selected set of data
// collectors that are sent from UI. Returns the result to UI in the format UI
// accepts.
void SupportToolMessageHandler::HandleStartDataCollection(
    const base::Value::List& args) {
  CHECK_EQ(3U, args.size());
  const base::Value& callback_id = args[0];
  const base::Value::Dict* issue_details = args[1].GetIfDict();
  DCHECK(issue_details);
  const base::Value::List* data_collectors = args[2].GetIfList();
  DCHECK(data_collectors);
  std::set<support_tool::DataCollectorType> included_data_collectors =
      GetIncludedDataCollectorTypes(data_collectors);
  // Send error message to UI if there's no selected data collectors to include.
  if (included_data_collectors.empty()) {
    ResolveJavascriptCallback(
        callback_id,
        GetStartDataCollectionResult(
            /*success=*/false,
            /*error_message=*/"No data collector selected. Please select at "
                              "least one data collector."));
    return;
  }
  this->handler_ =
      GetSupportToolHandler(*issue_details->FindString("caseId"),
                            *issue_details->FindString("emailAddress"),
                            *issue_details->FindString("issueDescription"),
                            GetIncludedDataCollectorTypes(data_collectors));
  // TODO(b/214196981): Add data collection timestamp as a class field to
  // SupportToolHandler.
  data_collection_time_ = base::Time::NowFromSystemTime();
  this->handler_->CollectSupportData(
      base::BindOnce(&SupportToolMessageHandler::OnDataCollectionDone,
                     weak_ptr_factory_.GetWeakPtr()));
  ResolveJavascriptCallback(
      callback_id, GetStartDataCollectionResult(
                       /*success=*/true, /*error_message=*/std::string()));
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

void SupportToolMessageHandler::HandleStartDataExport(
    const base::Value::List& args) {
  CHECK_EQ(1U, args.size());
  const base::Value::List* pii_items = args[0].GetIfList();
  DCHECK(pii_items);
  // Early return if the select file dialog is already active.
  if (select_file_dialog_)
    return;

  selected_pii_to_keep_ = GetSelectedPIIToKeep(pii_items);

  AllowJavascript();
  content::WebContents* web_contents = web_ui()->GetWebContents();
  gfx::NativeWindow owning_window =
      web_contents ? web_contents->GetTopLevelNativeWindow()
                   : gfx::kNullNativeWindow;
  select_file_dialog_ = ui::SelectFileDialog::Create(
      this,
      std::make_unique<ChromeSelectFilePolicy>(web_ui()->GetWebContents()));

  DownloadPrefs* download_prefs =
      DownloadPrefs::FromBrowserContext(web_contents->GetBrowserContext());
  base::FilePath suggested_path = download_prefs->SaveFilePath();

  select_file_dialog_->SelectFile(
      ui::SelectFileDialog::SELECT_SAVEAS_FILE,
      /*title=*/std::u16string(),
      /*default_path=*/
      GetDefaultFileToExport(suggested_path, handler_->GetCaseID(),
                             data_collection_time_),
      /*file_types=*/nullptr,
      /*file_type_index=*/0,
      /*default_extension=*/base::FilePath::StringType(), owning_window,
      /*params=*/nullptr);
}

void SupportToolMessageHandler::FileSelected(const base::FilePath& path,
                                             int index,
                                             void* params) {
  FireWebUIListener("support-data-export-started");
  select_file_dialog_.reset();
  this->handler_->ExportCollectedData(
      std::move(selected_pii_to_keep_), path,
      base::BindOnce(&SupportToolMessageHandler::OnDataExportDone,
                     weak_ptr_factory_.GetWeakPtr()));
}

void SupportToolMessageHandler::FileSelectionCanceled(void* params) {
  selected_pii_to_keep_.clear();
  select_file_dialog_.reset();
}

// Checks `errors` and fires WebUIListener with the error message or the
// exported path according to the returned errors.
// type DataExportResult = {
//  success: boolean,
//  path: string,
//  error: string,
// }
void SupportToolMessageHandler::OnDataExportDone(
    base::FilePath path,
    std::set<SupportToolError> errors) {
  data_path_ = path;
  base::Value::Dict data_export_result;
  const auto& export_error = std::find_if(
      errors.begin(), errors.end(), [](const SupportToolError& error) {
        return (error.error_code == SupportToolErrorCode::kDataExportError);
      });
  if (export_error == errors.end()) {
    data_export_result.Set("success", true);
    std::string displayed_path = data_path_.AsUTF8Unsafe();
#if BUILDFLAG(IS_CHROMEOS_ASH)
    displayed_path = file_manager::util::GetPathDisplayTextForSettings(
        Profile::FromWebUI(web_ui()), displayed_path);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
    data_export_result.Set("path", displayed_path);
    data_export_result.Set("error", std::string());
  } else {
    // If a data export error is found in the returned set of errors, send the
    // error message to UI with empty string as path since it means the export
    // operation has failed.
    data_export_result.Set("success", false);
    data_export_result.Set("path", std::string());
    data_export_result.Set("error", export_error->error_message);
  }
  FireWebUIListener("data-export-completed",
                    base::Value(std::move(data_export_result)));
}

void SupportToolMessageHandler::HandleShowExportedDataInFolder(
    const base::Value::List& args) {
  platform_util::ShowItemInFolder(Profile::FromWebUI(web_ui()), data_path_);
}

void SupportToolMessageHandler::HandleGenerateCustomizedURL(
    const base::Value::List& args) {
  CHECK_EQ(3U, args.size());
  const base::Value& callback_id = args[0];
  std::string case_id = args[1].GetString();
  const base::Value::List* data_collectors = args[2].GetIfList();
  DCHECK(data_collectors);
  ResolveJavascriptCallback(callback_id, base::Value(GenerateCustomizedURL(
                                             case_id, data_collectors)));
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
