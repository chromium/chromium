// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/context_hub/memory_banks_palette_bubble_view.h"

#include <memory>
#include <string>
#include <utility>

#include "base/check.h"
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
constexpr int kPopupWidth = 440;
constexpr int kPopupHeight = 520;
}  // namespace

MemoryBanksPaletteBubbleView::MemoryBanksPaletteBubbleView(
    views::BubbleAnchor anchor,
    Profile* profile)
    : views::BubbleDialogDelegate(anchor, views::BubbleBorder::TOP_RIGHT) {
  CHECK(profile);
  set_close_on_deactivate(true);
  SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));
  SetShowCloseButton(false);
  SetShowTitle(false);
  set_corner_radius(ChromeLayoutProvider::Get()->GetCornerRadiusMetric(
      views::Emphasis::kHigh));
  set_margins(gfx::Insets());
  InitLayout(profile);
}

MemoryBanksPaletteBubbleView::~MemoryBanksPaletteBubbleView() = default;

std::u16string MemoryBanksPaletteBubbleView::GetWindowTitle() const {
  // TODO(crbug.com/577076699): Replace with
  // IDS_CONTENT_CONTEXT_BROWSE_MEMORY_BANKS when the context menu entry is
  // added.
  return l10n_util::GetStringUTF16(IDS_CONTENT_CONTEXT_SAVE_TO_MEMORY_BANKS);
}

void MemoryBanksPaletteBubbleView::CloseContents(content::WebContents* source) {
  if (GetWidget()) {
    GetWidget()->CloseWithReason(
        views::Widget::ClosedReason::kCloseButtonClicked);
  }
}

views::WebView* MemoryBanksPaletteBubbleView::web_view_for_testing() {
  return static_cast<views::WebView*>(web_view_tracker_.view());
}

void MemoryBanksPaletteBubbleView::InitLayout(Profile* profile) {
  auto web_view = std::make_unique<views::WebView>(profile);
  web_view->SetPreferredSize(gfx::Size(kPopupWidth, kPopupHeight));

  std::string url_str = base::StrCat(
      {"chrome://", chrome::kChromeUIContextHubHost, "/memory_banks_palette"});

  web_view->LoadInitialURL(GURL(url_str));
  web_view->GetWebContents()->SetDelegate(this);
  web_view_tracker_.SetView(web_view.get());
  SetContentsView(std::move(web_view));
}
