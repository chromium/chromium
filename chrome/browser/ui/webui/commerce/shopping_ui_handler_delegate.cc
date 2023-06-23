// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/commerce/shopping_ui_handler_delegate.h"

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/webui/commerce/shopping_insights_side_panel_ui.h"
#include "components/commerce/core/webui/shopping_list_handler.h"
#include "content/public/browser/web_ui.h"

namespace commerce {

ShoppingUiHandlerDelegate::ShoppingUiHandlerDelegate(
    ShoppingInsightsSidePanelUI* insights_side_panel_ui)
    : insights_side_panel_ui_(insights_side_panel_ui) {}

ShoppingUiHandlerDelegate::~ShoppingUiHandlerDelegate() = default;

absl::optional<GURL> ShoppingUiHandlerDelegate::GetCurrentTabUrl() {
  auto* browser = chrome::FindLastActive();
  if (!browser) {
    return absl::nullopt;
  }

  content::WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  if (!web_contents) {
    return absl::nullopt;
  }
  return absl::make_optional<GURL>(web_contents->GetLastCommittedURL());
}

void ShoppingUiHandlerDelegate::ShowInsightsSidePanelUI() {
  if (insights_side_panel_ui_) {
    auto embedder = insights_side_panel_ui_->embedder();
    if (embedder) {
      embedder->ShowUI();
    }
  }
}

}  // namespace commerce
