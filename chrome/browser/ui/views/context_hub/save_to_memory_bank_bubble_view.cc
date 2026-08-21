// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/context_hub/save_to_memory_bank_bubble_view.h"

#include <memory>
#include <string>
#include <utility>

#include "base/strings/strcat.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

namespace {
constexpr int kPopupWidth = 320;
constexpr int kPopupHeight = 510;
}  // namespace

SaveToMemoryBankBubbleView::SaveToMemoryBankBubbleView(views::View* anchor_view,
                                                       Profile* profile)
    : views::BubbleDialogDelegate(anchor_view, views::BubbleBorder::TOP_RIGHT) {
  set_close_on_deactivate(true);
  SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));
  SetShowCloseButton(false);
  SetShowTitle(false);
  set_corner_radius(ChromeLayoutProvider::Get()->GetCornerRadiusMetric(
      views::Emphasis::kHigh));
  set_margins(gfx::Insets());
  InitLayout(profile);
}

SaveToMemoryBankBubbleView::~SaveToMemoryBankBubbleView() = default;

std::u16string SaveToMemoryBankBubbleView::GetWindowTitle() const {
  return l10n_util::GetStringUTF16(IDS_CONTENT_CONTEXT_SAVE_TO_MEMORY_BANKS);
}

void SaveToMemoryBankBubbleView::CloseContents(content::WebContents* source) {
  if (GetWidget()) {
    GetWidget()->CloseWithReason(
        views::Widget::ClosedReason::kCloseButtonClicked);
  }
}

views::WebView* SaveToMemoryBankBubbleView::web_view_for_testing() {
  return static_cast<views::WebView*>(web_view_tracker_.view());
}

void SaveToMemoryBankBubbleView::InitLayout(Profile* profile) {
  auto web_view = std::make_unique<views::WebView>(profile);
  web_view->SetPreferredSize(gfx::Size(kPopupWidth, kPopupHeight));

  std::string url_str = base::StrCat(
      {"chrome://", chrome::kChromeUIContextHubHost, "/save_to_memory_bank"});

  web_view->LoadInitialURL(GURL(url_str));
  web_view->GetWebContents()->SetDelegate(this);
  web_view_tracker_.SetView(web_view.get());
  SetContentsView(std::move(web_view));
}
