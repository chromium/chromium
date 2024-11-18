// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/ABC): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ui/webui/signin/batch_upload_ui.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/webui/favicon_source.h"
#include "chrome/browser/ui/webui/plural_string_handler.h"
#include "chrome/browser/ui/webui/signin/batch_upload_handler.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/batch_upload_resources.h"
#include "chrome/grit/batch_upload_resources_map.h"
#include "chrome/grit/generated_resources.h"
#include "components/favicon_base/favicon_url_parser.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_ui_data_source.h"

BatchUploadUI::BatchUploadUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui, true) {
  Profile* profile = Profile::FromWebUI(web_ui);
  // Set up the chrome://batch-upload source.
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      profile, chrome::kChromeUIBatchUploadHost);

  // Add required resources.
  webui::SetupWebUIDataSource(
      source, base::make_span(kBatchUploadResources, kBatchUploadResourcesSize),
      IDR_BATCH_UPLOAD_BATCH_UPLOAD_HTML);

  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"batchUploadTitle", IDS_BATCH_UPLOAD_TITLE},
      {"saveToAccount", IDS_BATCH_UPLOAD_SAVE_TO_ACCOUNT_OK_BUTTON_LABEL},
      {"cancel", IDS_CANCEL},
  };
  source->AddLocalizedStrings(kLocalizedStrings);

  source->UseStringsJs();
  source->EnableReplaceI18nInJS();

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
  std::unique_ptr<PluralStringHandler> plural_string_handler =
      std::make_unique<PluralStringHandler>();
  // Add the section titles variables. These will be updated based on the number
  // of selected items in each sections.
  std::set<std::string> section_title_ids;
  for (const syncer::LocalDataDescription& local_data_description :
       local_data_description_list) {
    int section_title_id =
        BatchUploadHandler::GetTypeSectionTitleId(local_data_description.type);
    plural_string_handler->AddLocalizedString(base::ToString(section_title_id),
                                              section_title_id);
  }
  web_ui()->AddMessageHandler(std::move(plural_string_handler));

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
