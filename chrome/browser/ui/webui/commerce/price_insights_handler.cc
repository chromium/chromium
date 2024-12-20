// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/commerce/price_insights_handler.h"

#include "chrome/browser/feedback/public/feedback_source.h"
#include "chrome/browser/feedback/show_feedback_page.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/webui/commerce/shopping_insights_side_panel_ui.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace commerce {

PriceInsightsHandler::PriceInsightsHandler(
    mojo::PendingReceiver<price_insights::mojom::PriceInsightsHandler> receiver,
    ShoppingInsightsSidePanelUI& insights_side_panel_ui,
    Profile* profile)
    : receiver_(this, std::move(receiver)),
      insights_side_panel_ui_(insights_side_panel_ui),
      profile_(profile) {}

PriceInsightsHandler::~PriceInsightsHandler() = default;

void PriceInsightsHandler::ShowSidePanelUI() {
  auto embedder = insights_side_panel_ui_->embedder();
  if (embedder) {
    embedder->ShowUI();
  }
}

void PriceInsightsHandler::ShowFeedback() {
  auto* browser = chrome::FindLastActiveWithProfile(profile_);
  if (!browser) {
    return;
  }

  chrome::ShowFeedbackPage(
      browser, feedback::kFeedbackSourcePriceInsights,
      /*description_template=*/std::string(),
      /*description_placeholder_text=*/
      l10n_util::GetStringUTF8(IDS_SHOPPING_INSIGHTS_FEEDBACK_FORM_TITLE),
      /*category_tag=*/"price_insights",
      /*extra_diagnostics=*/std::string());
}

}  // namespace commerce
