// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_COMMERCE_PRICE_INSIGHTS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_COMMERCE_PRICE_INSIGHTS_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/commerce/shopping_insights_side_panel_ui.h"
#include "components/commerce/core/mojom/price_insights.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace commerce {

class PriceInsightsHandler
    : public price_insights::mojom::PriceInsightsHandler {
 public:
  PriceInsightsHandler(
      mojo::PendingReceiver<price_insights::mojom::PriceInsightsHandler>
          receiver,
      ShoppingInsightsSidePanelUI& insights_side_panel_ui,
      Profile* profile);
  PriceInsightsHandler(const PriceInsightsHandler&) = delete;
  PriceInsightsHandler& operator=(const PriceInsightsHandler&) = delete;
  ~PriceInsightsHandler() override;

  void ShowSidePanelUI() override;
  void ShowFeedback() override;

 private:
  mojo::Receiver<price_insights::mojom::PriceInsightsHandler> receiver_;

  // This handler is owned by |insights_side_panel_ui_| so we expect
  // |insights_side_panel_ui_| to remain valid for the lifetime of |this|.
  raw_ref<ShoppingInsightsSidePanelUI> insights_side_panel_ui_;
  raw_ptr<Profile> profile_;
};

}  // namespace commerce

#endif  // CHROME_BROWSER_UI_WEBUI_COMMERCE_PRICE_INSIGHTS_HANDLER_H_
