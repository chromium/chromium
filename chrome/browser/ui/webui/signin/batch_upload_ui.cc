// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/batch_upload_ui.h"

#include "base/functional/callback_helpers.h"
#include "base/strings/to_string.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/tabs/tab_model.h"
#include "chrome/browser/ui/webui/favicon_source.h"
#include "chrome/browser/ui/webui/plural_string_handler.h"
#include "chrome/browser/ui/webui/signin/batch_upload_handler.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/batch_upload_resources.h"
#include "chrome/grit/batch_upload_resources_map.h"
#include "chrome/grit/generated_resources.h"
#include "components/favicon_base/favicon_url_parser.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync/base/data_type.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/webui/webui_util.h"

namespace {

std::pair<AccountInfo, std::vector<syncer::LocalDataDescription>>
GetSampleData() {
  AccountInfo account_info;
  account_info.email = "sample@gmail.com";
  std::vector<syncer::LocalDataDescription> descritpions;

  for (syncer::DataType type : {syncer::PASSWORDS, syncer::CONTACT_INFO}) {
    syncer::LocalDataDescription& description = descritpions.emplace_back();
    description.type = type;
    for (int i = 0; i < 3; ++i) {
      syncer::LocalDataItemModel& model =
          description.local_data_models.emplace_back();
      model.id = base::ToString(i);
      model.title = "sample_" + std::get<std::string>(model.id);
      model.subtitle = "sample_sub_" + std::get<std::string>(model.id);
    }
  }

  return {account_info, descritpions};
}

// Sample/debugging implementation that closes the browser tab regardless of the
// item map.
void CloseBrowserTabOnCompletionSample(
    Browser* browser,
    const std::map<syncer::DataType,
                   std::vector<syncer::LocalDataItemModel::DataId>>& items) {
  browser->GetTabStripModel()->GetActiveTab()->Close();
}

}  // namespace

BatchUploadUI::BatchUploadUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui, true) {
  Profile* profile = Profile::FromWebUI(web_ui);
  // Set up the chrome://batch-upload source.
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      profile, chrome::kChromeUIBatchUploadHost);

  // Add required resources.
  webui::SetupWebUIDataSource(source, kBatchUploadResources,
                              IDR_BATCH_UPLOAD_BATCH_UPLOAD_HTML);

  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"batchUploadTitle", IDS_BATCH_UPLOAD_TITLE},
      {"saveToAccount", IDS_BATCH_UPLOAD_SAVE_TO_ACCOUNT_OK_BUTTON_LABEL},
      {"cancel", IDS_CANCEL},
      {"lastItemSelectedScreenReader",
       IDS_BATCH_UPLOAD_LAST_ITEM_SELECTED_SCREEN_READER},
      {"itemCountSelectedScreenReader",
       IDS_BATCH_UPLOAD_SCREEN_READER_ITEM_COUNT_SELECTED},
      {"selectAllScreenReader", IDS_BATCH_UPLOAD_SCREEN_READER_SELECT_ALL},
      {"selectNoneScreenReader", IDS_BATCH_UPLOAD_SCREEN_READER_SELECT_NONE},
  };
  source->AddLocalizedStrings(kLocalizedStrings);

  source->UseStringsJs();
  source->EnableReplaceI18nInJS();

  std::unique_ptr<PluralStringHandler> plural_string_handler =
      std::make_unique<PluralStringHandler>();
  // Add the section titles variables for all eligible types in the dialog.
  // These will be updated based on the number of selected items in each
  // sections.
  for (syncer::DataType data_type : BatchUploadService::AvailableTypesOrder()) {
    int section_title_id = BatchUploadHandler::GetTypeSectionTitleId(data_type);
    // For Themes add the resource in the main webui source since themes has a
    // special way to display its title that requires a non plural localized
    // string with a variable. Also add the resource in the
    // `plural_string_handler` to simplify initialization of the view.
    if (data_type == syncer::DataType::THEMES) {
      source->AddLocalizedString(base::ToString(section_title_id),
                                 section_title_id);
    }

    plural_string_handler->AddLocalizedString(base::ToString(section_title_id),
                                              section_title_id);
  }
  web_ui->AddMessageHandler(std::move(plural_string_handler));

  content::URLDataSource::Add(
      profile, std::make_unique<FaviconSource>(
                   profile, chrome::FaviconUrlFormat::kFavicon2));
}

BatchUploadUI::~BatchUploadUI() = default;

WEB_UI_CONTROLLER_TYPE_IMPL(BatchUploadUI)

void BatchUploadUI::Initialize(
    const AccountInfo& account_info,
    Browser* browser,
    std::vector<syncer::LocalDataDescription> local_data_description_list,
    base::RepeatingCallback<void(int)> update_view_height_callback,
    base::RepeatingCallback<void(bool)> allow_web_view_input_callback,
    BatchUploadSelectedDataTypeItemsCallback completion_callback) {
  initialize_handler_callback_ = base::BindOnce(
      &BatchUploadUI::OnMojoHandlersReady, base::Unretained(this), account_info,
      browser, std::move(local_data_description_list),
      update_view_height_callback, allow_web_view_input_callback,
      std::move(completion_callback));
}

void BatchUploadUI::Clear() {
  handler_.reset();
}

void BatchUploadUI::BindInterface(
    mojo::PendingReceiver<batch_upload::mojom::PageHandlerFactory> receiver) {
  page_factory_receiver_.reset();
  page_factory_receiver_.Bind(std::move(receiver));
}

void BatchUploadUI::CreateBatchUploadHandler(
    mojo::PendingRemote<batch_upload::mojom::Page> page,
    mojo::PendingReceiver<batch_upload::mojom::PageHandler> receiver) {
  // For regular usages, `Initialize()` should be called right after loading the
  // Url into the view. This assumes that the URL was loaded directly into
  // Chrome for debugging purposes - fill it with sample data.
  if (!initialize_handler_callback_) {
    auto [account_info, descriptions] = GetSampleData();
    Browser* browser = chrome::FindLastActive();
    BatchUploadSelectedDataTypeItemsCallback sample_completion_callback =
        base::BindOnce(&CloseBrowserTabOnCompletionSample, browser);
    Initialize(account_info, browser, std::move(descriptions),
               base::DoNothing(), base::DoNothing(),
               std::move(sample_completion_callback));
  }

  CHECK(initialize_handler_callback_);
  std::move(initialize_handler_callback_)
      .Run(std::move(page), std::move(receiver));
}

void BatchUploadUI::OnMojoHandlersReady(
    const AccountInfo& account_info,
    Browser* browser,
    std::vector<syncer::LocalDataDescription> local_data_description_list,
    base::RepeatingCallback<void(int)> update_view_height_callback,
    base::RepeatingCallback<void(bool)> allow_web_view_input_callback,
    BatchUploadSelectedDataTypeItemsCallback completion_callback,
    mojo::PendingRemote<batch_upload::mojom::Page> page,
    mojo::PendingReceiver<batch_upload::mojom::PageHandler> receiver) {
  CHECK(!handler_);
  handler_ = std::make_unique<BatchUploadHandler>(
      std::move(receiver), std::move(page), account_info, browser,
      std::move(local_data_description_list), update_view_height_callback,
      allow_web_view_input_callback, std::move(completion_callback));
}
