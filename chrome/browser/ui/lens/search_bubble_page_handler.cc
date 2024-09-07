// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/lens/search_bubble_page_handler.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/themes/theme_service_utils.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/lens/lens_overlay_theme_utils.h"
#include "content/public/browser/web_contents.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/color_utils.h"

namespace lens {

SearchBubblePageHandler::SearchBubblePageHandler(
    TopChromeWebUIController* webui_controller,
    mojo::PendingReceiver<lens::mojom::SearchBubblePageHandler> receiver,
    mojo::PendingRemote<lens::mojom::SearchBubblePage> page,
    content::WebContents* web_contents,
    const PrefService* pref_service)
    : web_contents_(web_contents),
      pref_service_(pref_service),
      webui_controller_(webui_controller),
      receiver_(this, std::move(receiver)),
      page_(std::move(page)) {
  CHECK(web_contents_);
  SetTheme();
}

SearchBubblePageHandler::~SearchBubblePageHandler() = default;

void SearchBubblePageHandler::ShowUI() {
  auto embedder = webui_controller_->embedder();
  if (embedder) {
    embedder->ShowUI();
  }
}

void SearchBubblePageHandler::CloseUI() {
  auto embedder = webui_controller_->embedder();
  if (embedder) {
    embedder->CloseUI();
  }
}

lens::mojom::SearchboxThemePtr MakeTheme(content::WebContents* web_contents,
                                         const PrefService* pref_service) {
  auto theme = lens::mojom::SearchboxTheme::New();
  const ui::ColorProvider& color_provider = web_contents->GetColorProvider();
  theme->background_color = color_provider.GetColor(kColorNewTabPageBackground);
  theme->text_color = color_provider.GetColor(kColorNewTabPageText);
  if (!CurrentThemeIsGrayscale(pref_service) &&
      CurrentThemeUserColor(pref_service).has_value()) {
    theme->logo_color = color_provider.GetColor(kColorNewTabPageLogo);
  }
  theme->is_dark =
      lens::LensOverlayShouldUseDarkMode(ThemeServiceFactory::GetForProfile(
          Profile::FromBrowserContext(web_contents->GetBrowserContext())));
  return theme;
}

void SearchBubblePageHandler::SetTheme() {
  if (web_contents_) {
    page_->SetTheme(MakeTheme(web_contents_, pref_service_));
  }
}

}  // namespace lens
