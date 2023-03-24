// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/side_panel/companion/companion_page_handler.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/webui/side_panel/companion/companion_side_panel_untrusted_ui.h"
#include "chrome/browser/ui/webui/side_panel/companion/companion_url_builder.h"
#include "chrome/common/webui_url_constants.h"
#include "components/prefs/pref_service.h"
#include "components/unified_consent/pref_names.h"
#include "content/public/browser/web_contents.h"
#include "net/base/url_util.h"
#include "url/gurl.h"

namespace companion {

CompanionPageHandler::CompanionPageHandler(
    mojo::PendingReceiver<side_panel::mojom::CompanionPageHandler> receiver,
    mojo::PendingRemote<side_panel::mojom::CompanionPage> page,
    Browser* browser,
    CompanionSidePanelUntrustedUI* companion_untrusted_ui)
    : content::WebContentsObserver(
          browser->tab_strip_model()->GetActiveWebContents()),
      receiver_(this, std::move(receiver)),
      page_(std::move(page)),
      companion_untrusted_ui_(companion_untrusted_ui),
      url_builder_(std::make_unique<CompanionUrlBuilder>(
          browser->profile()->GetPrefs())) {
  DCHECK(browser);
  NotifyURLChanged();
}

CompanionPageHandler::~CompanionPageHandler() = default;

void CompanionPageHandler::PrimaryPageChanged(content::Page& page) {
  if (!url_builder_->IsMsbbEnabled()) {
    return;
  }
  NotifyURLChanged();
}

void CompanionPageHandler::ShowUI() {
  if (auto embedder = companion_untrusted_ui_->embedder()) {
    embedder->ShowUI();
  }
}

void CompanionPageHandler::NotifyURLChanged() {
  GURL companion_url =
      url_builder_->BuildCompanionURL(web_contents()->GetVisibleURL());
  page_->OnURLChanged(companion_url.spec());
}

}  // namespace companion
