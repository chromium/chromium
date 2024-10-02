// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ui/webui/search_engine_choice/search_engine_choice_ui.h"

#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/json/json_writer.h"
#include "base/not_fatal_until.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engine_choice/search_engine_choice_dialog_service.h"
#include "chrome/browser/search_engine_choice/search_engine_choice_dialog_service_factory.h"
#include "chrome/browser/search_engine_choice/search_engine_choice_service_factory.h"
#include "chrome/browser/ui/webui/search_engine_choice/icon_utils.h"
#include "chrome/browser/ui/webui/search_engine_choice/search_engine_choice_handler.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/chrome_unscaled_resources.h"
#include "chrome/grit/search_engine_choice_resources.h"
#include "chrome/grit/search_engine_choice_resources_map.h"
#include "chrome/grit/signin_resources.h"
#include "components/search_engines/search_engine_choice/search_engine_choice_service.h"
#include "components/search_engines/search_engine_choice/search_engine_choice_utils.h"
#include "components/search_engines/search_engines_switches.h"
#include "components/search_engines/template_url.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/strings/grit/components_branded_strings.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui_data_source.h"

namespace {
std::string GetChoiceListJSON(
    SearchEngineChoiceDialogService& search_engine_choice_dialog_service) {
  base::Value::List choice_value_list;
  const TemplateURL::TemplateURLVector choices =
      search_engine_choice_dialog_service.GetSearchEngines();

  for (const auto& choice : choices) {
    base::Value::Dict choice_value;

    std::string_view icon_path =
        GetSearchEngineGeneratedIconPath(choice->keyword());
    if (icon_path.empty()) {
      icon_path = "chrome://theme/IDR_DEFAULT_FAVICON";
    }
    choice_value.Set("prepopulateId", choice->prepopulate_id());
    choice_value.Set("name", choice->short_name());
    choice_value.Set("iconPath", icon_path);
    choice_value.Set("url", choice->url());
    choice_value.Set("marketingSnippet",
                     search_engines::GetMarketingSnippetString(choice->data()));
    choice_value.Set("showMarketingSnippet", false);
    choice_value_list.Append(std::move(choice_value));
  }
  std::string json_choice_list;
  base::JSONWriter::Write(choice_value_list, &json_choice_list);
  return json_choice_list;
}
}  // namespace

bool SearchEngineChoiceUIConfig::IsWebUIEnabled(
    content::BrowserContext* browser_context) {
  Profile* profile = Profile::FromBrowserContext(browser_context);
  return SearchEngineChoiceDialogServiceFactory::GetForProfile(profile);
}

SearchEngineChoiceUI::SearchEngineChoiceUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui, true),
      profile_(CHECK_DEREF(Profile::FromWebUI(web_ui))) {

  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      web_ui->GetWebContents()->GetBrowserContext(),
      chrome::kChromeUISearchEngineChoiceHost);

  source->AddLocalizedString("title", IDS_SEARCH_ENGINE_CHOICE_PAGE_TITLE);
  source->AddLocalizedString("subtitle",
                             IDS_SEARCH_ENGINE_CHOICE_PAGE_SUBTITLE);
  source->AddLocalizedString("subtitleInfoLink",
                             IDS_SEARCH_ENGINE_CHOICE_PAGE_SUBTITLE_INFO_LINK);
  source->AddLocalizedString(
      "subtitleInfoLinkA11yLabel",
      IDS_SEARCH_ENGINE_CHOICE_PAGE_SUBTITLE_INFO_LINK_A11Y_LABEL);
  source->AddLocalizedString("submitButtonText",
                             IDS_SEARCH_ENGINE_CHOICE_BUTTON_TITLE);
  source->AddLocalizedString("infoDialogTitle",
                             IDS_SEARCH_ENGINE_CHOICE_INFO_DIALOG_TITLE);
  source->AddLocalizedString(
      "infoDialogFirstParagraph",
      IDS_SEARCH_ENGINE_CHOICE_INFO_DIALOG_BODY_FIRST_PARAGRAPH);
  source->AddLocalizedString(
      "infoDialogSecondParagraph",
      IDS_SEARCH_ENGINE_CHOICE_INFO_DIALOG_BODY_SECOND_PARAGRAPH);
  source->AddLocalizedString(
      "infoDialogThirdParagraph",
      IDS_SEARCH_ENGINE_CHOICE_INFO_DIALOG_BODY_THIRD_PARAGRAPH);
  source->AddLocalizedString("choiceListA11yLabel",
                             IDS_SEARCH_ENGINE_CHOICE_LIST_A11Y_LABEL);
  source->AddLocalizedString("infoDialogButtonText", IDS_CLOSE);
  source->AddLocalizedString("productLogoAltText",
                             IDS_SHORT_PRODUCT_LOGO_ALT_TEXT);
  source->AddLocalizedString("moreButtonText",
                             IDS_SEARCH_ENGINE_CHOICE_MORE_BUTTON);
  source->AddLocalizedString("guestCheckboxText",
                             IDS_SEARCH_ENGINE_CHOICE_GUEST_SESSION_CHECKBOX);
  source->AddResourcePath("images/left_illustration.svg",
                          IDR_SIGNIN_IMAGES_SHARED_LEFT_BANNER_SVG);
  source->AddResourcePath("images/left_illustration_dark.svg",
                          IDR_SIGNIN_IMAGES_SHARED_LEFT_BANNER_DARK_SVG);
  source->AddResourcePath("images/right_illustration.svg",
                          IDR_SIGNIN_IMAGES_SHARED_RIGHT_BANNER_SVG);
  source->AddResourcePath("images/right_illustration_dark.svg",
                          IDR_SIGNIN_IMAGES_SHARED_RIGHT_BANNER_DARK_SVG);
  source->AddResourcePath("images/product-logo.svg", IDR_PRODUCT_LOGO_SVG);
  source->AddResourcePath("tangible_sync_style_shared_lit.css.js",
                          IDR_SIGNIN_TANGIBLE_SYNC_STYLE_SHARED_LIT_CSS_JS);
  source->AddResourcePath("signin_vars.css.js", IDR_SIGNIN_SIGNIN_VARS_CSS_JS);

  SearchEngineChoiceDialogService* search_engine_choice_dialog_service =
      SearchEngineChoiceDialogServiceFactory::GetForProfile(&profile_.get());
  source->AddString(
      "choiceList",
      GetChoiceListJSON(CHECK_DEREF(search_engine_choice_dialog_service)));

  search_engines::SearchEngineChoiceService* search_engine_choice_service =
      search_engines::SearchEngineChoiceServiceFactory::GetForProfile(
          &profile_.get());
  source->AddBoolean(
      "showGuestCheckbox",
      search_engine_choice_service->IsProfileEligibleForDseGuestPropagation());

  webui::SetupWebUIDataSource(
      source,
      base::make_span(kSearchEngineChoiceResources,
                      kSearchEngineChoiceResourcesSize),
      IDR_SEARCH_ENGINE_CHOICE_SEARCH_ENGINE_CHOICE_HTML);
}

WEB_UI_CONTROLLER_TYPE_IMPL(SearchEngineChoiceUI)

SearchEngineChoiceUI::~SearchEngineChoiceUI() = default;

void SearchEngineChoiceUI::BindInterface(
    mojo::PendingReceiver<search_engine_choice::mojom::PageHandlerFactory>
        receiver) {
  page_factory_receiver_.reset();
  page_factory_receiver_.Bind(std::move(receiver));
}

void SearchEngineChoiceUI::Initialize(
    base::OnceClosure display_dialog_callback,
    base::OnceClosure on_choice_made_callback,
    SearchEngineChoiceDialogService::EntryPoint entry_point) {
  display_dialog_callback_ = std::move(display_dialog_callback);
  on_choice_made_callback_ = std::move(on_choice_made_callback);
  entry_point_ = entry_point;

  if (entry_point_ != SearchEngineChoiceDialogService::EntryPoint::kDialog) {
    // This callback should always be populated.
    // TODO(b/344899110): Cleanup once the bug root cause is found.
    CHECK(on_choice_made_callback_, base::NotFatalUntil::M131);
  }
}

void SearchEngineChoiceUI::HandleSearchEngineChoiceMade(
    int prepopulate_id,
    bool save_guest_mode_selection) {
  if (entry_point_ != SearchEngineChoiceDialogService::EntryPoint::kDialog) {
    // If this callback is null, then this method has already been called, which
    // is a bug. (Or the initialization was skipped, but that would point to
    // users manually entering the URL, which is not supported)
    // TODO(b/344899110): Cleanup once the bug root cause is found.
    CHECK(on_choice_made_callback_, base::NotFatalUntil::M131);
  }

  SearchEngineChoiceDialogService* search_engine_choice_dialog_service =
      SearchEngineChoiceDialogServiceFactory::GetForProfile(&profile_.get());
  search_engine_choice_dialog_service->NotifyChoiceMade(
      prepopulate_id, save_guest_mode_selection, entry_point_);

  // Notify that the choice was made.
  if (on_choice_made_callback_) {
    std::move(on_choice_made_callback_).Run();
  }
}

void SearchEngineChoiceUI::HandleLearnMoreLinkClicked() {
  SearchEngineChoiceDialogService* search_engine_choice_dialog_service =
      SearchEngineChoiceDialogServiceFactory::GetForProfile(&profile_.get());

  search_engine_choice_dialog_service->NotifyLearnMoreLinkClicked(entry_point_);
}

void SearchEngineChoiceUI::HandleMoreButtonClicked() {
  SearchEngineChoiceDialogService* search_engine_choice_dialog_service =
      SearchEngineChoiceDialogServiceFactory::GetForProfile(&profile_.get());

  search_engine_choice_dialog_service->NotifyMoreButtonClicked(entry_point_);
}

void SearchEngineChoiceUI::CreatePageHandler(
    mojo::PendingReceiver<search_engine_choice::mojom::PageHandler> receiver) {
  page_handler_ = std::make_unique<SearchEngineChoiceHandler>(
      std::move(receiver), std::move(display_dialog_callback_),
      base::BindOnce(&SearchEngineChoiceUI::HandleSearchEngineChoiceMade,
                     weak_ptr_factory_.GetWeakPtr()),
      base::BindRepeating(&SearchEngineChoiceUI::HandleLearnMoreLinkClicked,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&SearchEngineChoiceUI::HandleMoreButtonClicked,
                     weak_ptr_factory_.GetWeakPtr()));
}
