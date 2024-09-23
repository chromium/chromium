// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ui/webui/support_tool/support_tool_ui.h"

#include <optional>
#include <set>
#include <string>
#include <vector>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_ui_util.h"
#include "chrome/browser/support_tool/data_collection_module.pb.h"
#include "chrome/browser/support_tool/data_collector.h"
#include "chrome/browser/support_tool/screenshot_data_collector.h"
#include "chrome/browser/support_tool/support_tool_handler.h"
#include "chrome/browser/support_tool/support_tool_util.h"
#include "chrome/browser/ui/chrome_select_file_policy.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/webui/support_tool/support_tool_ui_utils.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/support_tool_resources.h"
#include "chrome/grit/support_tool_resources_map.h"
#include "components/feedback/redaction_tool/pii_types.h"
#include "components/policy/core/common/management/management_service.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "net/base/url_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/shell_dialogs/select_file_dialog.h"
#include "ui/shell_dialogs/selected_file_info.h"
#include "url/gurl.h"

bool SupportToolUIConfig::IsWebUIEnabled(
    content::BrowserContext* browser_context) {
  Profile* profile = Profile::FromBrowserContext(browser_context);
  return base::FeatureList::IsEnabled(features::kSupportTool) &&
         SupportToolUI::IsEnabled(profile);
}

namespace {

// The filename prefix for the file to export the generated file.
constexpr char kFilenamePrefix[] = "support_packet";

// The path we use to show URL generator page
// chrome://support-tool/url-generator
constexpr char kUrlGeneratorPath[] = "url-generator";

// Records the open page action to `kSupportToolWebUIActionHistogram`. There are
// two possible types of pages that a user can navigate into: main support tool
// page and URL generator page. We check which was was opened from `url`.
void RecordOpenPageMetric(const GURL& url) {
  // We check if the URL has path. If not, it means it's support tool main page.
  if (!url.has_path()) {
    base::UmaHistogramEnumeration(
        kSupportToolWebUIActionHistogram,
        SupportToolWebUIActionType::kOpenSupportToolPage);
    return;
  }
  std::string trimmed_path;
  // We remove '/' characters from the path. `url.path_piece()` will return the
  // '/' along with the path value.
  base::TrimString(url.path_piece(), "/", &trimmed_path);
  // Check if the path is URL generator path.
  if (trimmed_path.compare(kUrlGeneratorPath) == 0) {
    base::UmaHistogramEnumeration(
        kSupportToolWebUIActionHistogram,
        SupportToolWebUIActionType::kOpenURLGeneratorPage);
    return;
  }
  base::UmaHistogramEnumeration(
      kSupportToolWebUIActionHistogram,
      SupportToolWebUIActionType::kOpenSupportToolPage);
}

void CreateAndAddSupportToolHTMLSource(Profile* profile, const GURL& url) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      profile, chrome::kChromeUISupportToolHost);

  source->AddString("caseId", GetSupportCaseIDFromURL(url));
  source->AddBoolean("enableScreenshot", base::FeatureList::IsEnabled(
                                             features::kSupportToolScreenshot));

  source->AddLocalizedStrings(SupportToolUI::GetLocalizedStrings());

  webui::SetupWebUIDataSource(
      source, base::make_span(kSupportToolResources, kSupportToolResourcesSize),
      IDR_SUPPORT_TOOL_SUPPORT_TOOL_CONTAINER_HTML);

  source->AddResourcePath(kUrlGeneratorPath,
                          IDR_SUPPORT_TOOL_URL_GENERATOR_CONTAINER_HTML);
}

}  // namespace

const char kSupportToolWebUIActionHistogram[] =
    "Browser.SupportTool.SupportToolWebUIAction";

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

  void HandleTakeScreenshot(const base::Value::List& args);

  void HandleStartDataCollection(const base::Value::List& args);

  void HandleCancelDataCollection(const base::Value::List& args);

  void HandleStartDataExport(const base::Value::List& args);

  void HandleShowExportedDataInFolder(const base::Value::List& args);

  void HandleGenerateCustomizedURL(const base::Value::List& args);

  void HandleGenerateSupportToken(const base::Value::List& args);

  // SelectFileDialog::Listener implementation.
  void FileSelected(const ui::SelectedFileInfo& file, int index) override;

  void FileSelectionCanceled() override;

 private:
  base::Value::List GetAccountsList();

  void OnScreenshotTaken(std::optional<SupportToolError> error);

  void OnDataCollectionDone(const PIIMap& detected_pii,
                            std::set<SupportToolError> errors);

  void OnDataExportDone(base::FilePath path, std::set<SupportToolError> errors);

  bool include_screenshot_ = false;
  std::unique_ptr<ScreenshotDataCollector> screenshot_data_collector_;
  std::set<redaction::PIIType> selected_pii_to_keep_;
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
      "takeScreenshot",
      base::BindRepeating(&SupportToolMessageHandler::HandleTakeScreenshot,
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
      "generateCustomizedUrl",
      base::BindRepeating(
          &SupportToolMessageHandler::HandleGenerateCustomizedURL,
          weak_ptr_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "generateSupportToken",
      base::BindRepeating(
          &SupportToolMessageHandler::HandleGenerateSupportToken,
          weak_ptr_factory_.GetWeakPtr()));
}

base::Value::List SupportToolMessageHandler::GetAccountsList() {
  Profile* profile = Profile::FromWebUI(web_ui());
  base::Value::List account_list;

  // Guest session and incognito mode do not have a primary account (or an
  // IdentityManager).
  if (profile->IsGuestSession() || profile->IsIncognitoProfile()) {
    return account_list;
  }

  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  for (const auto& account : signin_ui_util::GetOrderedAccountsForDisplay(
           identity_manager,
           /*restrict_to_accounts_eligible_for_sync=*/false)) {
    if (!account.IsEmpty()) {
      account_list.Append(account.email);
    }
  }
  return account_list;
}

void SupportToolMessageHandler::HandleGetEmailAddresses(
    const base::Value::List& args) {
  AllowJavascript();
  CHECK_EQ(1U, args.size());
  const base::Value& callback_id = args[0];

  ResolveJavascriptCallback(callback_id, GetAccountsList());
}

void SupportToolMessageHandler::HandleGetDataCollectors(
    const base::Value::List& args) {
  AllowJavascript();
  CHECK_EQ(1U, args.size());
  const base::Value& callback_id = args[0];

  std::string module_query;
  net::GetValueForKeyInQuery(web_ui()->GetWebContents()->GetURL(),
                             support_tool_ui::kModuleQuery, &module_query);

  ResolveJavascriptCallback(callback_id,
                            GetDataCollectorItemsInQuery(module_query));
}

void SupportToolMessageHandler::HandleGetAllDataCollectors(
    const base::Value::List& args) {
  AllowJavascript();
  CHECK_EQ(1U, args.size());
  const base::Value& callback_id = args[0];
  ResolveJavascriptCallback(callback_id, GetAllDataCollectorItems());
}

void SupportToolMessageHandler::HandleTakeScreenshot(
    const base::Value::List& args) {
  screenshot_data_collector_ = std::make_unique<ScreenshotDataCollector>();
  screenshot_data_collector_->CollectDataAndDetectPII(
      base::BindOnce(&SupportToolMessageHandler::OnScreenshotTaken,
                     weak_ptr_factory_.GetWeakPtr()),
      /*task_runner_for_redaction_tool=*/nullptr,
      /*redaction_tool_container=*/nullptr);
}

void SupportToolMessageHandler::OnScreenshotTaken(
    std::optional<SupportToolError> error) {
  if (error) {
    LOG(ERROR) << error.value().error_message;
  }
  AllowJavascript();
  FireWebUIListener("screenshot-received",
                    screenshot_data_collector_->GetScreenshotBase64());
}

// Starts data collection with the issue details and selected set of data
// collectors that are sent from UI. Returns the result to UI in the format UI
// accepts.
void SupportToolMessageHandler::HandleStartDataCollection(
    const base::Value::List& args) {
  CHECK_EQ(4U, args.size());
  const base::Value& callback_id = args[0];
  const base::Value::Dict* issue_details = args[1].GetIfDict();
  DCHECK(issue_details);
  const base::Value::List* data_collectors = args[2].GetIfList();
  DCHECK(data_collectors);
  const std::string* editedScreenshotBase64 = args[3].GetIfString();
  DCHECK(editedScreenshotBase64);
  if (*editedScreenshotBase64 != "") {
    include_screenshot_ = true;
    screenshot_data_collector_->SetScreenshotBase64(
        std::move(*editedScreenshotBase64));
  } else {
    include_screenshot_ = false;
  }
  std::set<support_tool::DataCollectorType> included_data_collectors =
      GetIncludedDataCollectorTypes(data_collectors);
  // Send error message to UI if there's no selected data collectors to include.
  if (included_data_collectors.empty()) {
    ResolveJavascriptCallback(
        callback_id, GetStartDataCollectionResult(
                         /*success=*/false,
                         /*error_message=*/l10n_util::GetStringUTF16(
                             IDS_SUPPORT_TOOL_SELECT_DATA_COLLECTOR_ERROR)));
    return;
  }
  this->handler_ =
      GetSupportToolHandler(*issue_details->FindString("caseId"),
                            *issue_details->FindString("emailAddress"),
                            *issue_details->FindString("issueDescription"),
                            std::nullopt, Profile::FromWebUI(web_ui()),
                            GetIncludedDataCollectorTypes(data_collectors));
  this->handler_->CollectSupportData(
      base::BindOnce(&SupportToolMessageHandler::OnDataCollectionDone,
                     weak_ptr_factory_.GetWeakPtr()));
  ResolveJavascriptCallback(
      callback_id, GetStartDataCollectionResult(
                       /*success=*/true, /*error_message=*/std::u16string()));
}

void SupportToolMessageHandler::OnDataCollectionDone(
    const PIIMap& detected_pii,
    std::set<SupportToolError> errors) {
  AllowJavascript();
  FireWebUIListener("data-collection-completed",
                    GetDetectedPIIDataItems(detected_pii));
}

void SupportToolMessageHandler::HandleCancelDataCollection(
    const base::Value::List& args) {
  base::UmaHistogramEnumeration(
      kSupportToolWebUIActionHistogram,
      SupportToolWebUIActionType::kCancelDataCollection);
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
  if (select_file_dialog_) {
    return;
  }

  selected_pii_to_keep_ = GetPIITypesToKeep(pii_items);

  AllowJavascript();
  content::WebContents* web_contents = web_ui()->GetWebContents();
  gfx::NativeWindow owning_window =
      web_contents ? web_contents->GetTopLevelNativeWindow()
                   : gfx::NativeWindow();
  select_file_dialog_ = ui::SelectFileDialog::Create(
      this,
      std::make_unique<ChromeSelectFilePolicy>(web_ui()->GetWebContents()));

  DownloadPrefs* download_prefs =
      DownloadPrefs::FromBrowserContext(web_contents->GetBrowserContext());
  base::FilePath suggested_path = download_prefs->SaveFilePath();
  ui::SelectFileDialog::FileTypeInfo file_types;

  select_file_dialog_->SelectFile(
      ui::SelectFileDialog::SELECT_SAVEAS_FILE,
      /*title=*/std::u16string(),
      /*default_path=*/
      GetFilepathToExport(suggested_path, kFilenamePrefix,
                          handler_->GetCaseId(),
                          handler_->GetDataCollectionTimestamp()),
      /*file_types=*/&file_types,
      /*file_type_index=*/0,
      /*default_extension=*/base::FilePath::StringType(), owning_window,
      /*params=*/nullptr);
}

void SupportToolMessageHandler::FileSelected(const ui::SelectedFileInfo& file,
                                             int index) {
  base::UmaHistogramEnumeration(
      kSupportToolWebUIActionHistogram,
      SupportToolWebUIActionType::kCreateSupportPacket);
  FireWebUIListener("support-data-export-started");
  select_file_dialog_.reset();
  if (include_screenshot_) {
    this->handler_->AddDataCollector(std::move(screenshot_data_collector_));
  }
  this->handler_->ExportCollectedData(
      std::move(selected_pii_to_keep_), file.path(),
      base::BindOnce(&SupportToolMessageHandler::OnDataExportDone,
                     weak_ptr_factory_.GetWeakPtr()));
}

void SupportToolMessageHandler::FileSelectionCanceled() {
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
  const auto& export_error =
      base::ranges::find(errors, SupportToolErrorCode::kDataExportError,
                         &SupportToolError::error_code);
  if (export_error == errors.end()) {
    data_export_result.Set("success", true);
    data_export_result.Set("path", path.BaseName().AsUTF8Unsafe());
    data_export_result.Set("error", std::string());
  } else {
    // If a data export error is found in the returned set of errors, send the
    // error message to UI with empty string as path since it means the export
    // operation has failed.
    data_export_result.Set("success", false);
    data_export_result.Set("path", std::string());
    data_export_result.Set("error", export_error->error_message);
  }
  FireWebUIListener("data-export-completed", data_export_result);
}

void SupportToolMessageHandler::HandleShowExportedDataInFolder(
    const base::Value::List& args) {
  platform_util::ShowItemInFolder(Profile::FromWebUI(web_ui()), data_path_);
}

void SupportToolMessageHandler::HandleGenerateCustomizedURL(
    const base::Value::List& args) {
  base::UmaHistogramEnumeration(kSupportToolWebUIActionHistogram,
                                SupportToolWebUIActionType::kGenerateURL);
  CHECK_EQ(3U, args.size());
  const base::Value& callback_id = args[0];
  std::string case_id = args[1].GetString();
  const base::Value::List* data_collectors = args[2].GetIfList();
  CHECK(data_collectors);
  ResolveJavascriptCallback(callback_id,
                            GenerateCustomizedURL(case_id, data_collectors));
}

void SupportToolMessageHandler::HandleGenerateSupportToken(
    const base::Value::List& args) {
  base::UmaHistogramEnumeration(kSupportToolWebUIActionHistogram,
                                SupportToolWebUIActionType::kGenerateToken);
  CHECK_EQ(2U, args.size());
  const base::Value& callback_id = args[0];
  const base::Value::List* data_collectors = args[1].GetIfList();
  CHECK(data_collectors);
  ResolveJavascriptCallback(callback_id, GenerateSupportToken(data_collectors));
}

////////////////////////////////////////////////////////////////////////////////
//
// SupportToolUI
//
////////////////////////////////////////////////////////////////////////////////

SupportToolUI::SupportToolUI(content::WebUI* web_ui) : WebUIController(web_ui) {
  web_ui->AddMessageHandler(std::make_unique<SupportToolMessageHandler>());

  const GURL& url = web_ui->GetWebContents()->GetVisibleURL();
  // Set up the chrome://support-tool/ source.
  CreateAndAddSupportToolHTMLSource(Profile::FromWebUI(web_ui), url);
  // Record the page open action to histogram.
  RecordOpenPageMetric(url);
}

SupportToolUI::~SupportToolUI() = default;

// static
base::Value::Dict SupportToolUI::GetLocalizedStrings() {
  base::Value::Dict localized_strings;
  localized_strings.Set(
      "issueDetailsPageTitle",
      l10n_util::GetStringUTF16(IDS_SUPPORT_TOOL_ISSUE_DETAILS_PAGE_TITLE));
  localized_strings.Set(
      "dataSelectionPageTitle",
      l10n_util::GetStringUTF16(IDS_SUPPORT_TOOL_DATA_SELECTION_PAGE_TITLE));
  localized_strings.Set(
      "reviewPiiPageTitle",
      l10n_util::GetStringUTF16(IDS_SUPPORT_TOOL_REVIEW_PII_PAGE_TITLE));
  localized_strings.Set(
      "dataExportDonePageTitle",
      l10n_util::GetStringUTF16(IDS_SUPPORT_TOOL_DATA_EXPORT_DONE_PAGE_TITLE));
  localized_strings.Set(
      "dataExportedText",
      l10n_util::GetStringUTF16(IDS_SUPPORT_TOOL_DATA_EXPORTED_TEXT));
  localized_strings.Set("supportCaseId", l10n_util::GetStringUTF16(
                                             IDS_SUPPORT_TOOL_SUPPORT_CASE_ID));
  localized_strings.Set("email",
                        l10n_util::GetStringUTF16(IDS_SUPPORT_TOOL_EMAIL));
  localized_strings.Set(
      "describeIssueText",
      l10n_util::GetStringUTF16(IDS_SUPPORT_TOOL_DESCRIBE_ISSUE_TEXT));
  localized_strings.Set("issueDescriptionPlaceholder",
                        l10n_util::GetStringUTF16(
                            IDS_SUPPORT_TOOL_ISSUE_DESCRIPTION_PLACEHOLDER));
  localized_strings.Set(
      "piiWarningText",
      l10n_util::GetStringUTF16(IDS_SUPPORT_TOOL_PII_WARNING_TEXT));
  localized_strings.Set(
      "includeAllPiiRadioButton",
      l10n_util::GetStringUTF16(IDS_SUPPORT_TOOL_INCLUDE_ALL_PII_RADIO_BUTTON));
  localized_strings.Set(
      "removePiiRadioButton",
      l10n_util::GetStringUTF16(IDS_SUPPORT_TOOL_REMOVE_PII_RADIO_BUTTON));
  localized_strings.Set(
      "piiRemovalDisclaimer",
      l10n_util::GetStringUTF16(IDS_SUPPORT_TOOL_PII_REMOVAL_DISCLAIMER));
  localized_strings.Set("manuallySelectPiiRadioButton",
                        l10n_util::GetStringUTF16(
                            IDS_SUPPORT_TOOL_MANUALLY_SELECT_PII_RADIO_BUTTON));
  localized_strings.Set(
      "cancelButtonText",
      l10n_util::GetStringUTF16(IDS_SUPPORT_TOOL_CANCEL_BUTTON_TEXT));
  localized_strings.Set(
      "exportButtonText",
      l10n_util::GetStringUTF16(IDS_SUPPORT_TOOL_EXPORT_BUTTON_TEXT));
  localized_strings.Set(
      "backButtonText",
      l10n_util::GetStringUTF16(IDS_SUPPORT_TOOL_BACK_BUTTON_TEXT));
  localized_strings.Set(
      "dismissButtonText",
      l10n_util::GetStringUTF16(IDS_SUPPORT_TOOL_DISMISS_BUTTON_TEXT));
  localized_strings.Set(
      "continueButtonText",
      l10n_util::GetStringUTF16(IDS_SUPPORT_TOOL_CONTINUE_BUTTON_TEXT));
  localized_strings.Set(
      "dataCollectionSpinner",
      l10n_util::GetStringUTF16(IDS_SUPPORT_TOOL_DATA_COLLECTION_SPINNER));
  localized_strings.Set(
      "dataExportSpinner",
      l10n_util::GetStringUTF16(IDS_SUPPORT_TOOL_DATA_EXPORT_SPINNER));
  localized_strings.Set("supportToolTabTitle",
                        l10n_util::GetStringUTF16(IDS_SUPPORT_TOOL_TAB_TITLE));
  localized_strings.Set(
      "urlGeneratorPageTitle",
      l10n_util::GetStringUTF16(IDS_SUPPORT_TOOL_URL_GENERATOR_PAGE_TITLE));
  localized_strings.Set(
      "dataCollectorListTitle",
      l10n_util::GetStringUTF16(IDS_SUPPORT_TOOL_DATA_COLLECTOR_LIST_TITLE));
  localized_strings.Set(
      "getLinkText", l10n_util::GetStringUTF16(IDS_SUPPORT_TOOL_GET_LINK_TEXT));
  localized_strings.Set(
      "copyLinkDescription",
      l10n_util::GetStringUTF16(IDS_SUPPORT_TOOL_COPY_LINK_DESCRIPTION));
  localized_strings.Set(
      "copyLinkButtonText",
      l10n_util::GetStringUTF16(IDS_SUPPORT_TOOL_COPY_LINK_BUTTON_TEXT));
  localized_strings.Set(
      "copyTokenButtonText",
      l10n_util::GetStringUTF16(IDS_SUPPORT_TOOL_COPY_TOKEN_BUTTON_TEXT));
  localized_strings.Set("linkCopied", l10n_util::GetStringUTF16(
                                          IDS_SUPPORT_TOOL_LINK_COPIED_TEXT));
  localized_strings.Set("tokenCopied", l10n_util::GetStringUTF16(
                                           IDS_SUPPORT_TOOL_TOKEN_COPIED_TEXT));
  localized_strings.Set(
      "dontIncludeEmailAddress",
      l10n_util::GetStringUTF16(IDS_SUPPORT_TOOL_DONT_INCLUDE_EMAIL));
  localized_strings.Set("selectAll",
                        l10n_util::GetStringUTF16(IDS_SUPPORT_TOOL_SELECT_ALL));
  return localized_strings;
}

// static
bool SupportToolUI::IsEnabled(Profile* profile) {
  return policy::ManagementServiceFactory::GetForPlatform()->IsManaged() ||
         !profile->IsGuestSession();
}
