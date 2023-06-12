// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/waffle/waffle_ui.h"

#include "base/feature_list.h"
#include "base/functional/callback_forward.h"
#include "base/json/json_writer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/signin_features.h"
#include "chrome/browser/ui/webui/waffle/waffle_handler.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/signin_resources.h"
#include "chrome/grit/waffle_resources.h"
#include "chrome/grit/waffle_resources_map.h"
#include "components/search_engines/template_url_data.h"
#include "components/search_engines/template_url_prepopulate_data.h"
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
    choice_value.Set("name", choice->short_name());
    choice_value_list.Append(std::move(choice_value));
  }

  std::string json_choice_list;
  base::JSONWriter::Write(choice_value_list, &json_choice_list);
  return json_choice_list;
}
}  // namespace

WaffleUI::WaffleUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui, true) {
  CHECK(base::FeatureList::IsEnabled(kWaffle));
  auto* profile = Profile::FromWebUI(web_ui);

  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      web_ui->GetWebContents()->GetBrowserContext(),
      chrome::kChromeUIWaffleHost);

  source->AddLocalizedString("title", IDS_WAFFLE_PAGE_TITLE);
  source->AddLocalizedString("subtitle", IDS_WAFFLE_PAGE_SUBTITLE);
  source->AddLocalizedString("firstButton", IDS_WAFFLE_FIRST_BUTTON_TITLE);
  source->AddLocalizedString("secondButton", IDS_WAFFLE_SECOND_BUTTON_TITLE);

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
      source, base::make_span(kWaffleResources, kWaffleResourcesSize),
      IDR_WAFFLE_WAFFLE_HTML);
}

WEB_UI_CONTROLLER_TYPE_IMPL(WaffleUI)

WaffleUI::~WaffleUI() = default;

void WaffleUI::BindInterface(
    mojo::PendingReceiver<waffle::mojom::PageHandlerFactory> receiver) {
  page_factory_receiver_.reset();
  page_factory_receiver_.Bind(std::move(receiver));
}

void WaffleUI::Initialize(base::OnceClosure display_dialog_callback) {
  CHECK(display_dialog_callback);
  display_dialog_callback_ = std::move(display_dialog_callback);
}

void WaffleUI::CreatePageHandler(
    mojo::PendingReceiver<waffle::mojom::PageHandler> receiver) {
  page_handler_ = std::make_unique<WaffleHandler>(
      std::move(receiver), std::move(display_dialog_callback_));
}
