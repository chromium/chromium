// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/search_engine_choice/search_engine_choice_ui.h"

#include "base/check_deref.h"
#include "base/check_is_test.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/json/json_writer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engine_choice/search_engine_choice_service.h"
#include "chrome/browser/search_engine_choice/search_engine_choice_service_factory.h"
#include "chrome/browser/ui/webui/search_engine_choice/search_engine_choice_handler.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/chrome_unscaled_resources.h"
#include "chrome/grit/search_engine_choice_resources.h"
#include "chrome/grit/search_engine_choice_resources_map.h"
#include "chrome/grit/signin_resources.h"
#include "components/grit/components_scaled_resources.h"
#include "components/search_engines/search_engine_utils.h"
#include "components/search_engines/search_engine_choice_utils.h"
#include "components/search_engines/template_url_data.h"
#include "components/search_engines/template_url_prepopulate_data.h"
#include "components/strings/grit/components_branded_strings.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui_data_source.h"

namespace {
// Start of generated code.
// This code is generated using `generate_search_engine_icons.py`. Don't modify
// it manually.
void AddGeneratedIconResources(content::WebUIDataSource* source) {
  CHECK(source);
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  source->AddResourcePath("images/google_com.png", IDR_GOOGLE_COM_PNG);
#endif
  source->AddResourcePath("images/baidu_com.png", IDR_BAIDU_COM_PNG);
  source->AddResourcePath("images/bing_com.png", IDR_BING_COM_PNG);
  source->AddResourcePath("images/search_brave_com.png",
                          IDR_SEARCH_BRAVE_COM_PNG);
  source->AddResourcePath("images/coccoc_com.png", IDR_COCCOC_COM_PNG);
  source->AddResourcePath("images/daum_net.png", IDR_DAUM_NET_PNG);
  source->AddResourcePath("images/duckduckgo_com.png", IDR_DUCKDUCKGO_COM_PNG);
  source->AddResourcePath("images/ecosia_org.png", IDR_ECOSIA_ORG_PNG);
  source->AddResourcePath("images/karmasearch_org.png",
                          IDR_KARMASEARCH_ORG_PNG);
  source->AddResourcePath("images/lilo_org.png", IDR_LILO_ORG_PNG);
  source->AddResourcePath("images/mail_ru.png", IDR_MAIL_RU_PNG);
  source->AddResourcePath("images/mojeek_com.png", IDR_MOJEEK_COM_PNG);
  source->AddResourcePath("images/naver_com.png", IDR_NAVER_COM_PNG);
  source->AddResourcePath("images/nona_de.png", IDR_NONA_DE_PNG);
  source->AddResourcePath("images/panda_search_org.png",
                          IDR_PANDA_SEARCH_ORG_PNG);
  source->AddResourcePath("images/quendu_com.png", IDR_QUENDU_COM_PNG);
  source->AddResourcePath("images/qwant_com.png", IDR_QWANT_COM_PNG);
  source->AddResourcePath("images/seznam_cz.png", IDR_SEZNAM_CZ_PNG);
  source->AddResourcePath("images/seznam_sk.png", IDR_SEZNAM_SK_PNG);
  source->AddResourcePath("images/so_com.png", IDR_SO_COM_PNG);
  source->AddResourcePath("images/sogou_com.png", IDR_SOGOU_COM_PNG);
  source->AddResourcePath("images/yahoo_com.png", IDR_YAHOO_COM_PNG);
  source->AddResourcePath("images/ar_yahoo_com.png", IDR_AR_YAHOO_COM_PNG);
  source->AddResourcePath("images/at_yahoo_com.png", IDR_AT_YAHOO_COM_PNG);
  source->AddResourcePath("images/au_yahoo_com.png", IDR_AU_YAHOO_COM_PNG);
  source->AddResourcePath("images/br_yahoo_com.png", IDR_BR_YAHOO_COM_PNG);
  source->AddResourcePath("images/ca_yahoo_com.png", IDR_CA_YAHOO_COM_PNG);
  source->AddResourcePath("images/ch_yahoo_com.png", IDR_CH_YAHOO_COM_PNG);
  source->AddResourcePath("images/cl_yahoo_com.png", IDR_CL_YAHOO_COM_PNG);
  source->AddResourcePath("images/co_yahoo_com.png", IDR_CO_YAHOO_COM_PNG);
  source->AddResourcePath("images/de_yahoo_com.png", IDR_DE_YAHOO_COM_PNG);
  source->AddResourcePath("images/dk_yahoo_com.png", IDR_DK_YAHOO_COM_PNG);
  source->AddResourcePath("images/es_yahoo_com.png", IDR_ES_YAHOO_COM_PNG);
  source->AddResourcePath("images/fi_yahoo_com.png", IDR_FI_YAHOO_COM_PNG);
  source->AddResourcePath("images/fr_yahoo_com.png", IDR_FR_YAHOO_COM_PNG);
  source->AddResourcePath("images/hk_yahoo_com.png", IDR_HK_YAHOO_COM_PNG);
  source->AddResourcePath("images/id_yahoo_com.png", IDR_ID_YAHOO_COM_PNG);
  source->AddResourcePath("images/in_yahoo_com.png", IDR_IN_YAHOO_COM_PNG);
  source->AddResourcePath("images/yahoo_co_jp.png", IDR_YAHOO_CO_JP_PNG);
  source->AddResourcePath("images/mx_yahoo_com.png", IDR_MX_YAHOO_COM_PNG);
  source->AddResourcePath("images/malaysia_yahoo_com.png",
                          IDR_MALAYSIA_YAHOO_COM_PNG);
  source->AddResourcePath("images/nl_yahoo_com.png", IDR_NL_YAHOO_COM_PNG);
  source->AddResourcePath("images/nz_yahoo_com.png", IDR_NZ_YAHOO_COM_PNG);
  source->AddResourcePath("images/pe_yahoo_com.png", IDR_PE_YAHOO_COM_PNG);
  source->AddResourcePath("images/ph_yahoo_com.png", IDR_PH_YAHOO_COM_PNG);
  source->AddResourcePath("images/se_yahoo_com.png", IDR_SE_YAHOO_COM_PNG);
  source->AddResourcePath("images/sg_yahoo_com.png", IDR_SG_YAHOO_COM_PNG);
  source->AddResourcePath("images/th_yahoo_com.png", IDR_TH_YAHOO_COM_PNG);
  source->AddResourcePath("images/tr_yahoo_com.png", IDR_TR_YAHOO_COM_PNG);
  source->AddResourcePath("images/tw_yahoo_com.png", IDR_TW_YAHOO_COM_PNG);
  source->AddResourcePath("images/uk_yahoo_com.png", IDR_UK_YAHOO_COM_PNG);
  source->AddResourcePath("images/yandex_by.png", IDR_YANDEX_BY_PNG);
  source->AddResourcePath("images/yandex_com.png", IDR_YANDEX_COM_PNG);
  source->AddResourcePath("images/yandex_kz.png", IDR_YANDEX_KZ_PNG);
  source->AddResourcePath("images/yandex_ru.png", IDR_YANDEX_RU_PNG);
  source->AddResourcePath("images/yandex_com_tr.png", IDR_YANDEX_COM_TR_PNG);
  source->AddResourcePath("images/yep_com.png", IDR_YEP_COM_PNG);
  source->AddResourcePath("images/info_com.png", IDR_INFO_COM_PNG);
  source->AddResourcePath("images/metager_de.png", IDR_METAGER_DE_PNG);
  source->AddResourcePath("images/oceanhero_today.png",
                          IDR_OCEANHERO_TODAY_PNG);
  source->AddResourcePath("images/privacywall_org.png",
                          IDR_PRIVACYWALL_ORG_PNG);
}
// End of generated code.

std::string GetChoiceListJSON(Profile& profile) {
  base::Value::List choice_value_list;
  SearchEngineChoiceService* search_engine_choice_service =
      SearchEngineChoiceServiceFactory::GetForProfile(&profile);
  const std::vector<std::unique_ptr<TemplateURLData>> choices =
      search_engine_choice_service->GetSearchEngines();

  for (const auto& choice : choices) {
    base::Value::Dict choice_value;
    // The icon path that's generated in `AddGeneratedIconResources` is of the
    // format 'keyword'.png while replacing all the '.' in 'keyword' by '_'.
    std::u16string engine_keyword = choice->keyword();
    std::replace(engine_keyword.begin(), engine_keyword.end(), '.', '_');
    std::replace(engine_keyword.begin(), engine_keyword.end(), '-', '_');
    const std::u16string icon_path = u"images/" + engine_keyword + u".png";

    choice_value.Set("prepopulate_id", choice->prepopulate_id);
    choice_value.Set("name", choice->short_name());
    choice_value.Set("icon_path", icon_path);
    choice_value.Set("url", choice->url());
    choice_value_list.Append(std::move(choice_value));
  }
  std::string json_choice_list;
  base::JSONWriter::Write(choice_value_list, &json_choice_list);
  return json_choice_list;
}
}  // namespace

SearchEngineChoiceUI::SearchEngineChoiceUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui, true),
      profile_(CHECK_DEREF(Profile::FromWebUI(web_ui))) {
  CHECK(search_engines::IsChoiceScreenFlagEnabled(
      search_engines::ChoicePromo::kAny));

  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      web_ui->GetWebContents()->GetBrowserContext(),
      chrome::kChromeUISearchEngineChoiceHost);

  // TODO(b/280753567): Differentiate new from existing users. For new users use
  // IDS_SEARCH_ENGINE_CHOICE_PAGE_TITLE if
  // FirstRunServiceFactory::GetForBrowserContextIfExists(profile_.get()) is
  // present indicating it's part of FRE.
  source->AddLocalizedString("title",
                             IDS_SEARCH_ENGINE_CHOICE_PAGE_EXISTING_USER_TITLE);
  source->AddLocalizedString("subtitle",
                             IDS_SEARCH_ENGINE_CHOICE_PAGE_SUBTITLE);
  source->AddLocalizedString("subtitleInfoLink",
                             IDS_SEARCH_ENGINE_CHOICE_PAGE_SUBTITLE_INFO_LINK);
  source->AddLocalizedString(
      "subtitleInfoLinkA11yLabel",
      IDS_SEARCH_ENGINE_CHOICE_PAGE_SUBTITLE_INFO_LINK_A11Y_LABEL);
  source->AddLocalizedString("buttonText",
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
  source->AddLocalizedString("infoDialogButtonText", IDS_CLOSE);
  source->AddLocalizedString("productLogoAltText",
                             IDS_SHORT_PRODUCT_LOGO_ALT_TEXT);
  source->AddLocalizedString("fakeOmniboxText",
                             IDS_SEARCH_ENGINE_CHOICE_FAKE_OMNIBOX_TEXT);

  AddGeneratedIconResources(source);
  source->AddResourcePath("images/left_illustration.svg",
                          IDR_SIGNIN_IMAGES_SHARED_LEFT_BANNER_SVG);
  source->AddResourcePath("images/left_illustration_dark.svg",
                          IDR_SIGNIN_IMAGES_SHARED_LEFT_BANNER_DARK_SVG);
  source->AddResourcePath("images/right_illustration.svg",
                          IDR_SIGNIN_IMAGES_SHARED_RIGHT_BANNER_SVG);
  source->AddResourcePath("images/right_illustration_dark.svg",
                          IDR_SIGNIN_IMAGES_SHARED_RIGHT_BANNER_DARK_SVG);
  source->AddResourcePath("images/product-logo.svg", IDR_PRODUCT_LOGO_SVG);
  source->AddResourcePath("tangible_sync_style_shared.css.js",
                          IDR_SIGNIN_TANGIBLE_SYNC_STYLE_SHARED_CSS_JS);
  source->AddResourcePath("signin_vars.css.js", IDR_SIGNIN_SIGNIN_VARS_CSS_JS);

  source->AddString("choiceList", GetChoiceListJSON(profile_.get()));

  webui::SetupChromeRefresh2023(source);

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
    base::OnceCallback<void()> display_dialog_callback) {
  CHECK(display_dialog_callback);
  display_dialog_callback_ = std::move(display_dialog_callback);
}

void SearchEngineChoiceUI::HandleSearchEngineChoiceMade(int prepopulate_id) {
  SearchEngineChoiceService* search_engine_choice_service =
      SearchEngineChoiceServiceFactory::GetForProfile(&profile_.get());
  search_engine_choice_service->NotifyChoiceMade(prepopulate_id);
}

void SearchEngineChoiceUI::CreatePageHandler(
    mojo::PendingReceiver<search_engine_choice::mojom::PageHandler> receiver) {
  page_handler_ = std::make_unique<SearchEngineChoiceHandler>(
      std::move(receiver), std::move(display_dialog_callback_),
      base::BindOnce(&SearchEngineChoiceUI::HandleSearchEngineChoiceMade,
                     weak_ptr_factory_.GetWeakPtr()));
}
