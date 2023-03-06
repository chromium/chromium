// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/side_panel/companion/companion_page_handler.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/webui/side_panel/companion/companion_side_panel_untrusted_ui.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/web_contents.h"
#include "net/base/url_util.h"
#include "url/gurl.h"

CompanionPageHandler::CompanionPageHandler(
    mojo::PendingReceiver<side_panel::mojom::CompanionPageHandler> receiver,
    mojo::PendingRemote<side_panel::mojom::CompanionPage> page,
    Browser* browser,
    CompanionSidePanelUntrustedUI* companion_untrusted_ui)
    : receiver_(this, std::move(receiver)),
      page_(std::move(page)),
      browser_(browser),
      companion_untrusted_ui_(companion_untrusted_ui) {
  DCHECK(browser_);
  browser_->tab_strip_model()->AddObserver(this);

  // Observe the active web contents and then pass the current active visible
  // URL to the WebUI.
  Observe(browser_->tab_strip_model()->GetActiveWebContents());
  NotifyURLChanged();
}

CompanionPageHandler::~CompanionPageHandler() {
  browser_->tab_strip_model()->RemoveObserver(this);
  Observe(nullptr);
}

void CompanionPageHandler::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  if (selection.active_tab_changed() && selection.new_contents) {
    Observe(selection.new_contents);
    NotifyURLChanged();
  }
}

void CompanionPageHandler::PrimaryPageChanged(content::Page& page) {
  NotifyURLChanged();
}

void CompanionPageHandler::ShowUI() {
  if (auto embedder = companion_untrusted_ui_->embedder()) {
    embedder->ShowUI();
  }
}

void CompanionPageHandler::NotifyURLChanged() {
  page_->OnURLChanged(
      GetCompanionURLWithQueryParams(web_contents()->GetVisibleURL()).spec());
}

GURL CompanionPageHandler::GetCompanionURLWithQueryParams(
    GURL url_query_param_value) {
  GURL url_with_query_params = GetHomepageURLForCompanion();

  // Add relevant query parameters to the homepage URL.
  url_with_query_params = net::AppendOrReplaceQueryParameter(
      url_with_query_params, kUrlQueryParameterKey,
      url_query_param_value.spec());
  url_with_query_params = net::AppendOrReplaceQueryParameter(
      url_with_query_params, kOriginQueryParameterKey,
      kOriginQueryParameterValue);
  return url_with_query_params;
}

GURL CompanionPageHandler::GetHomepageURLForCompanion() {
  return GURL(features::kHomepageURLForCompanion.Get());
}
