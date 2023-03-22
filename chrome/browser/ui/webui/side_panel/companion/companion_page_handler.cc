// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/side_panel/companion/companion_page_handler.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/webui/side_panel/companion/companion_side_panel_untrusted_ui.h"
#include "chrome/common/webui_url_constants.h"
#include "components/prefs/pref_service.h"
#include "components/unified_consent/pref_names.h"
#include "content/public/browser/web_contents.h"
#include "net/base/url_util.h"
#include "url/gurl.h"
CompanionPageHandler::CompanionPageHandler(
    mojo::PendingReceiver<side_panel::mojom::CompanionPageHandler> receiver,
    mojo::PendingRemote<side_panel::mojom::CompanionPage> page,
    Browser* browser,
    CompanionSidePanelUntrustedUI* companion_untrusted_ui)
    : content::WebContentsObserver(
          browser->tab_strip_model()->GetActiveWebContents()),
      receiver_(this, std::move(receiver)),
      page_(std::move(page)),
      companion_untrusted_ui_(companion_untrusted_ui) {
  DCHECK(browser);
  InitializePage();
}

CompanionPageHandler::~CompanionPageHandler() = default;

void CompanionPageHandler::PrimaryPageChanged(content::Page& page) {
  if (!IsMsbbEnabled()) {
    return;
  }
  NotifyURLChanged();
}

void CompanionPageHandler::ShowUI() {
  if (auto embedder = companion_untrusted_ui_->embedder()) {
    embedder->ShowUI();
  }
}

void CompanionPageHandler::InitializePage() {
  if (!IsMsbbEnabled()) {
    page_->OnInitializePage(GetCompanionURLWithQueryParams(GURL()).spec());
    return;
  }

  page_->OnInitializePage(
      GetCompanionURLWithQueryParams(web_contents()->GetVisibleURL()).spec());
}

void CompanionPageHandler::NotifyURLChanged() {
  page_->OnURLChanged(
      GetCompanionURLWithQueryParams(web_contents()->GetVisibleURL()).spec());
}

GURL CompanionPageHandler::GetCompanionURLWithQueryParams(
    GURL url_query_param_value) {
  GURL url_with_query_params = GetHomepageURLForCompanion();

  // Add relevant query parameters to the homepage URL.
  if (!url_query_param_value.is_empty()) {
    url_with_query_params = net::AppendOrReplaceQueryParameter(
        url_with_query_params, kUrlQueryParameterKey,
        url_query_param_value.spec());
  }
  url_with_query_params = net::AppendOrReplaceQueryParameter(
      url_with_query_params, kOriginQueryParameterKey,
      kOriginQueryParameterValue);
  return url_with_query_params;
}

GURL CompanionPageHandler::GetHomepageURLForCompanion() {
  return GURL(features::kHomepageURLForCompanion.Get());
}

bool CompanionPageHandler::IsMsbbEnabled() {
  auto* profile = Profile::FromWebUI(companion_untrusted_ui_->web_ui());
  return profile->GetPrefs()->GetBoolean(
      unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled);
}
