// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/scanning/scanning_ui.h"

#include <memory>
#include <string>
#include <utility>

#include "base/containers/span.h"
#include "base/memory/ptr_util.h"
#include "chromeos/components/scanning/mojom/scanning.mojom.h"
#include "chromeos/components/scanning/scanning_paths_provider.h"
#include "chromeos/components/scanning/url_constants.h"
#include "chromeos/grit/chromeos_scanning_app_resources.h"
#include "chromeos/grit/chromeos_scanning_app_resources_map.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/resources/grit/webui_generated_resources.h"
#include "ui/resources/grit/webui_resources.h"

namespace chromeos {

namespace {

constexpr char kGeneratedPath[] =
    "@out_folder@/gen/chromeos/components/scanning/resources/";

// TODO(jschettler): Replace with webui::SetUpWebUIDataSource() once it no
// longer requires a dependency on //chrome/browser.
void SetUpWebUIDataSource(content::WebUIDataSource* source,
                          base::span<const GritResourceMap> resources,
                          const std::string& generated_path,
                          int default_resource) {
  for (const auto& resource : resources) {
    std::string path = resource.name;
    if (path.rfind(generated_path, 0) == 0)
      path = path.substr(generated_path.size());

    source->AddResourcePath(path, resource.value);
  }

  source->SetDefaultResource(default_resource);
  source->AddResourcePath("test_loader.html", IDR_WEBUI_HTML_TEST_LOADER_HTML);
  source->AddResourcePath("test_loader.js", IDR_WEBUI_JS_TEST_LOADER_JS);
}

void AddScanningAppStrings(content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"a4OptionText", IDS_SCANNING_APP_A4_OPTION_TEXT},
      {"appTitle", IDS_SCANNING_APP_TITLE},
      {"blackAndWhiteOptionText", IDS_SCANNING_APP_BLACK_AND_WHITE_OPTION_TEXT},
      {"cancelButtonText", IDS_SCANNING_APP_CANCEL_BUTTON_TEXT},
      {"cancelFailedToastText", IDS_SCANNING_APP_CANCEL_FAILED_TOAST_TEXT},
      {"cancelingScanningText", IDS_SCANNING_APP_CANCELING_SCANNING_TEXT},
      {"colorModeDropdownLabel", IDS_SCANNING_APP_COLOR_MODE_DROPDOWN_LABEL},
      {"colorOptionText", IDS_SCANNING_APP_COLOR_OPTION_TEXT},
      {"defaultSourceOptionText", IDS_SCANNING_APP_DEFAULT_SOURCE_OPTION_TEXT},
      {"doneButtonText", IDS_SCANNING_APP_DONE_BUTTON_TEXT},
      {"fileNotFoundToastText", IDS_SCANNING_APP_FILE_NOT_FOUND_TOAST_TEXT},
      {"fileTypeDropdownLabel", IDS_SCANNING_APP_FILE_TYPE_DROPDOWN_LABEL},
      {"fitToScanAreaOptionText",
       IDS_SCANNING_APP_FIT_TO_SCAN_AREA_OPTION_TEXT},
      {"flatbedOptionText", IDS_SCANNING_APP_FLATBED_OPTION_TEXT},
      {"getHelpLinkText", IDS_SCANNING_APP_GET_HELP_LINK_TEXT},
      {"grayscaleOptionText", IDS_SCANNING_APP_GRAYSCALE_OPTION_TEXT},
      {"jpgOptionText", IDS_SCANNING_APP_JPG_OPTION_TEXT},
      {"letterOptionText", IDS_SCANNING_APP_LETTER_OPTION_TEXT},
      {"moreSettings", IDS_SCANNING_APP_MORE_SETTINGS},
      {"myFilesSelectOption", IDS_SCANNING_APP_MY_FILES_SELECT_OPTION},
      {"noScannersHelpLinkLabel", IDS_SCANNING_APP_NO_SCANNERS_HELP_LINK_LABEL},
      {"noScannersHelpText", IDS_SCANNING_APP_NO_SCANNERS_HELP_TEXT},
      {"noScannersText", IDS_SCANNING_APP_NO_SCANNERS_TEXT},
      {"okButtonLabel", IDS_SCANNING_APP_OK_BUTTON_LABEL},
      {"oneSidedDocFeederOptionText",
       IDS_SCANNING_APP_ONE_SIDED_DOC_FEEDER_OPTION_TEXT},
      {"pdfOptionText", IDS_SCANNING_APP_PDF_OPTION_TEXT},
      {"pngOptionText", IDS_SCANNING_APP_PNG_OPTION_TEXT},
      {"pageSizeDropdownLabel", IDS_SCANNING_APP_PAGE_SIZE_DROPDOWN_LABEL},
      {"resolutionDropdownLabel", IDS_SCANNING_APP_RESOLUTION_DROPDOWN_LABEL},
      {"resolutionOptionText", IDS_SCANNING_APP_RESOLUTION_OPTION_TEXT},
      {"scanButtonText", IDS_SCANNING_APP_SCAN_BUTTON_TEXT},
      {"scanCanceledToastText", IDS_SCANNING_APP_SCAN_CANCELED_TOAST_TEXT},
      {"scanFailedDialogBodyText",
       IDS_SCANNING_APP_SCAN_FAILED_DIALOG_BODY_TEXT},
      {"scanFailedDialogTitleText",
       IDS_SCANNING_APP_SCAN_FAILED_DIALOG_TITLE_TEXT},
      {"scanPreviewHelperText", IDS_SCANNING_APP_SCAN_PREVIEW_HELPER_TEXT},
      {"scanPreviewProgressText", IDS_SCANNING_APP_SCAN_PREVIEW_PROGRESS_TEXT},
      {"scanToDropdownLabel", IDS_SCANNING_APP_SCAN_TO_DROPDOWN_LABEL},
      {"scannerDropdownLabel", IDS_SCANNING_APP_SCANNER_DROPDOWN_LABEL},
      {"selectFolderOption", IDS_SCANNING_APP_SELECT_FOLDER_OPTION},
      {"showFileLocationLabel", IDS_SCANNING_APP_SHOW_FILE_LOCATION_LABEL},
      {"sourceDropdownLabel", IDS_SCANNING_APP_SOURCE_DROPDOWN_LABEL},
      {"startScanFailedToast", IDS_SCANNING_APP_START_SCAN_FAILED_TOAST},
      {"twoSidedDocFeederOptionText",
       IDS_SCANNING_APP_TWO_SIDED_DOC_FEEDER_OPTION_TEXT}};

  for (const auto& str : kLocalizedStrings)
    html_source->AddLocalizedString(str.name, str.id);

  html_source->UseStringsJs();
}

void AddScanningAppPluralStrings(ScanningHandler* handler) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"fileSavedText", IDS_SCANNING_APP_FILE_SAVED_TEXT},
      {"scannedImagesAriaLabel", IDS_SCANNING_APP_SCANNED_IMAGES_ARIA_LABEL}};

  for (const auto& str : kLocalizedStrings)
    handler->AddStringToPluralMap(str.name, str.id);
}

}  // namespace

ScanningUI::ScanningUI(
    content::WebUI* web_ui,
    BindScanServiceCallback callback,
    const ScanningHandler::SelectFilePolicyCreator& select_file_policy_creator,
    std::unique_ptr<ScanningPathsProvider> scanning_paths_provider,
    const ScanningHandler::OpenFilesAppFunction& open_files_app_fn)
    : ui::MojoWebUIController(web_ui, true /* enable_chrome_send */),
      bind_pending_receiver_callback_(std::move(callback)) {
  auto html_source = base::WrapUnique(
      content::WebUIDataSource::Create(kChromeUIScanningAppHost));
  html_source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ScriptSrc,
      "script-src chrome://resources chrome://test 'self';");
  html_source->DisableTrustedTypesCSP();

  const auto resources = base::make_span(kChromeosScanningAppResources,
                                         kChromeosScanningAppResourcesSize);
  SetUpWebUIDataSource(html_source.get(), resources, kGeneratedPath,
                       IDR_SCANNING_APP_INDEX_HTML);

  html_source->AddResourcePath("scanning.mojom-lite.js",
                               IDR_SCANNING_MOJO_LITE_JS);
  html_source->AddResourcePath("file_path.mojom-lite.js",
                               IDR_SCANNING_APP_FILE_PATH_MOJO_LITE_JS);

  AddScanningAppStrings(html_source.get());

  auto handler = std::make_unique<ScanningHandler>(
      select_file_policy_creator, std::move(scanning_paths_provider),
      open_files_app_fn);
  AddScanningAppPluralStrings(handler.get());

  web_ui->AddMessageHandler(std::move(handler));
  content::WebUIDataSource::Add(web_ui->GetWebContents()->GetBrowserContext(),
                                html_source.release());
}

ScanningUI::~ScanningUI() = default;

void ScanningUI::BindInterface(
    mojo::PendingReceiver<scanning::mojom::ScanService> pending_receiver) {
  bind_pending_receiver_callback_.Run(std::move(pending_receiver));
}

WEB_UI_CONTROLLER_TYPE_IMPL(ScanningUI)

}  // namespace chromeos
