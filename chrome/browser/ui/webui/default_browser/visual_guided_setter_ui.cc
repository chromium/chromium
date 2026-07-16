// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/default_browser/visual_guided_setter_ui.h"

#include <string>
#include <utility>

#include "base/containers/span.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/theme_source.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/chrome_unscaled_resources.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/visual_guided_setter_resources.h"
#include "chrome/grit/visual_guided_setter_resources_map.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "net/base/url_util.h"
#include "ui/webui/webui_util.h"

VisualGuidedSetterUI::VisualGuidedSetterUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui, /*enable_chrome_send=*/false) {
  bool can_pin_to_taskbar = false;
  std::string value;
  if (net::GetValueForKeyInQuery(web_ui->GetWebContents()->GetVisibleURL(),
                                 "can_pin_to_taskbar", &value)) {
    can_pin_to_taskbar = value == "true";
  }

  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      profile, chrome::kChromeUIDefaultBrowserVisualGuidedSetterHost);
  content::URLDataSource::Add(profile, std::make_unique<ThemeSource>(profile));

  source->AddLocalizedString(
      "title", IDS_DEFAULT_BROWSER_VISUAL_GUIDED_SETTER_PAGE_TITLE);
  source->AddLocalizedString(
      "headerTitle", IDS_DEFAULT_BROWSER_VISUAL_GUIDED_SETTER_HEADER_TITLE);
  source->AddLocalizedString(
      "contentTitle", IDS_DEFAULT_BROWSER_VISUAL_GUIDED_SETTER_CONTENT_TITLE);
  source->AddLocalizedString(
      "contentDescription",
      IDS_DEFAULT_BROWSER_VISUAL_GUIDED_SETTER_CONTENT_DESCRIPTION);
  source->AddLocalizedString(
      "instructionStep1",
      IDS_DEFAULT_BROWSER_VISUAL_GUIDED_SETTER_INSTRUCTION_STEP_1);
  source->AddLocalizedString(
      "instructionStep2",
      IDS_DEFAULT_BROWSER_VISUAL_GUIDED_SETTER_INSTRUCTION_STEP_2);
  source->AddLocalizedString(
      "resizedToFit", IDS_DEFAULT_BROWSER_VISUAL_GUIDED_SETTER_RESIZED_TO_FIT);
  source->AddLocalizedString(
      "didntWork", IDS_DEFAULT_BROWSER_VISUAL_GUIDED_SETTER_DIDNT_WORK);
  source->AddLocalizedString(
      "openWindowsSettings",
      IDS_DEFAULT_BROWSER_VISUAL_GUIDED_SETTER_OPEN_WINDOWS_SETTINGS);

  source->AddBoolean("canPinToTaskbar", can_pin_to_taskbar);

  source->AddResourcePath("chrome_logo.svg", IDR_PRODUCT_LOGO_SVG);

  webui::SetupWebUIDataSource(
      source, base::span(kVisualGuidedSetterResources),
      IDR_VISUAL_GUIDED_SETTER_VISUAL_GUIDED_SETTER_HTML);
}

WEB_UI_CONTROLLER_TYPE_IMPL(VisualGuidedSetterUI)

VisualGuidedSetterUI::~VisualGuidedSetterUI() = default;

void VisualGuidedSetterUI::BindInterface(
    mojo::PendingReceiver<visual_guided_setter::mojom::PageHandlerFactory>
        receiver) {
  page_factory_receiver_.reset();
  page_factory_receiver_.Bind(std::move(receiver));
}

void VisualGuidedSetterUI::CreatePageHandler(
    mojo::PendingRemote<visual_guided_setter::mojom::Page> page,
    mojo::PendingReceiver<visual_guided_setter::mojom::PageHandler> handler) {
  page_handler_ = std::make_unique<VisualGuidedSetterPageHandler>(
      std::move(handler), std::move(page), web_ui()->GetWebContents());
}
