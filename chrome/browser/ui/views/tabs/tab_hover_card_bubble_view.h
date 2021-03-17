// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_TAB_HOVER_CARD_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_TAB_HOVER_CARD_BUBBLE_VIEW_H_

#include <memory>

#include "base/callback_list.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/ui/tabs/tab_utils.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/metadata/metadata_header_macros.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/public/cpp/metrics_util.h"
#include "base/optional.h"
#endif

namespace gfx {
class ImageSkia;
}

namespace views {
class ImageView;
class Label;
}  // namespace views

class Tab;

// Dialog that displays an informational hover card containing page information.
class TabHoverCardBubbleView : public views::BubbleDialogDelegateView {
 public:
  METADATA_HEADER(TabHoverCardBubbleView);
  explicit TabHoverCardBubbleView(Tab* tab);
  TabHoverCardBubbleView(const TabHoverCardBubbleView&) = delete;
  TabHoverCardBubbleView& operator=(const TabHoverCardBubbleView&) = delete;
  ~TabHoverCardBubbleView() override;

  // Updates and formats title, alert state, domain, and preview image.
  void UpdateCardContent(const Tab* tab);

  // Update the text fade to the given percent, which should be between 0 and 1.
  void SetTextFade(double percent);

  void ClearPreviewImage();
  void SetPreviewImage(gfx::ImageSkia preview_image);

 private:
  friend class TabHoverCardBubbleViewBrowserTest;
  friend class TabHoverCardBubbleViewInteractiveUiTest;
  class FadeLabel;

  // views::BubbleDialogDelegateView:
  ax::mojom::Role GetAccessibleWindowRole() override;
  void Layout() override;
  gfx::Size CalculatePreferredSize() const override;
  void OnThemeChanged() override;

  views::Label* title_label_ = nullptr;
  FadeLabel* title_fade_label_ = nullptr;
  base::Optional<TabAlertState> alert_state_;
  views::Label* domain_label_ = nullptr;
  FadeLabel* domain_fade_label_ = nullptr;
  views::ImageView* preview_image_ = nullptr;

  const bool using_rounded_corners_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_TAB_HOVER_CARD_BUBBLE_VIEW_H_
