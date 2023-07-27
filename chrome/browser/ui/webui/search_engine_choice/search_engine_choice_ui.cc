// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/search_engine_choice/search_engine_choice_ui.h"

#include "base/feature_list.h"
#include "base/functional/callback_forward.h"
#include "base/json/json_writer.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/search_engine_choice/search_engine_choice_handler.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/search_engine_choice_resources.h"
#include "chrome/grit/search_engine_choice_resources_map.h"
#include "chrome/grit/signin_resources.h"
#include "components/search_engines/template_url_data.h"
#include "components/search_engines/template_url_prepopulate_data.h"
#include "components/signin/public/base/signin_switches.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui_data_source.h"

namespace {
std::string GetChoiceListJSON(Profile* profile) {
  if (!profile) {
    return "";
  }

  base::Value::List choice_value_list;
  auto* pref_service = profile->GetPrefs();
  const std::vector<std::unique_ptr<TemplateURLData>> choices =
      TemplateURLPrepopulateData::GetPrepopulatedEngines(
          pref_service, /*default_search_provider_index=*/nullptr);

  for (const auto& choice : choices) {
    base::Value::Dict choice_value;
    choice_value.Set("id", base::NumberToString(choice->prepopulate_id));
    choice_value.Set("name", choice->short_name());
    choice_value_list.Append(std::move(choice_value));
  }

  std::string json_choice_list;
  base::JSONWriter::Write(choice_value_list, &json_choice_list);
  return json_choice_list;
}
}  // namespace

SearchEngineChoiceUI::SearchEngineChoiceUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui, true) {
  CHECK(base::FeatureList::IsEnabled(switches::kSearchEngineChoice));
  auto* profile = Profile::FromWebUI(web_ui);

  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      web_ui->GetWebContents()->GetBrowserContext(),
      chrome::kChromeUISearchEngineChoiceHost);

  source->AddLocalizedString("title", IDS_SEARCH_ENGINE_CHOICE_PAGE_TITLE);
  source->AddLocalizedString("subtitle",
                             IDS_SEARCH_ENGINE_CHOICE_PAGE_SUBTITLE);
  source->AddLocalizedString("subtitleInfoLink",
                             IDS_SEARCH_ENGINE_CHOICE_PAGE_SUBTITLE_INFO_LINK);
  source->AddLocalizedString("buttonText",
                             IDS_SEARCH_ENGINE_CHOICE_BUTTON_TITLE);
  source->AddLocalizedString("infoTitle",
                             IDS_SEARCH_ENGINE_CHOICE_INFO_DIALOG_TITLE);

  source->AddResourcePath("images/left_illustration.svg",
                          IDR_SIGNIN_IMAGES_SHARED_LEFT_BANNER_SVG);
  source->AddResourcePath("images/left_illustration_dark.svg",
                          IDR_SIGNIN_IMAGES_SHARED_LEFT_BANNER_DARK_SVG);
  source->AddResourcePath("images/right_illustration.svg",
                          IDR_SIGNIN_IMAGES_SHARED_RIGHT_BANNER_SVG);
  source->AddResourcePath("images/right_illustration_dark.svg",
                          IDR_SIGNIN_IMAGES_SHARED_RIGHT_BANNER_DARK_SVG);

  source->AddString("choiceList", GetChoiceListJSON(profile));

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
    base::OnceCallback<void(int)> display_dialog_callback) {
  CHECK(display_dialog_callback);
  display_dialog_callback_ = std::move(display_dialog_callback);
}

void SearchEngineChoiceUI::CreatePageHandler(
    mojo::PendingReceiver<search_engine_choice::mojom::PageHandler> receiver) {
  page_handler_ = std::make_unique<SearchEngineChoiceHandler>(
      std::move(receiver), std::move(display_dialog_callback_));
}
