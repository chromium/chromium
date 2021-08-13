// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_TAB_HOVER_CARD_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_TAB_HOVER_CARD_BUBBLE_VIEW_H_

#include <string>
#include "base/callback_list.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/ui/tabs/tab_utils.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/animation/linear_animation.h"
#include "ui/views/animation/animation_delegate_views.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/public/cpp/metrics_util.h"
#endif

namespace gfx {
class ImageSkia;
}

class Tab;

// Dialog that displays an informational hover card containing page information.
class TabHoverCardBubbleView : public views::BubbleDialogDelegateView {
 public:
  static constexpr base::TimeDelta kHoverCardSlideDuration =
      base::TimeDelta::FromMilliseconds(200);

  METADATA_HEADER(TabHoverCardBubbleView);
  explicit TabHoverCardBubbleView(Tab* tab);
  TabHoverCardBubbleView(const TabHoverCardBubbleView&) = delete;
  TabHoverCardBubbleView& operator=(const TabHoverCardBubbleView&) = delete;
  ~TabHoverCardBubbleView() override;

  // Updates and formats title, alert state, domain, and preview image.
  void UpdateCardContent(const Tab* tab);

  // Update the text fade to the given percent, which should be between 0 and 1.
  void SetTextFade(double percent);

  // Set the preview image to use for the target tab.
  void SetTargetTabImage(gfx::ImageSkia preview_image);

  // Specifies that the hover card should display a placeholder image
  // specifying that no preview for the tab is available (yet).
  void SetPlaceholderImage();

  // Accessors used by tests.
  std::u16string GetTitleTextForTesting() const;
  std::u16string GetDomainTextForTesting() const;

  // Returns the percentage complete during transition animations when a
  // pre-emptive crossfade to a placeholder should start if a new image is not
  // available, or `absl::nullopt` to disable crossfades entirely.
  static absl::optional<double> GetPreviewImageCrossfadeStart();

 private:
  class FadeLabel;
  class ThumbnailView;

  bool using_rounded_corners() const { return corner_radius_.has_value(); }

  // views::BubbleDialogDelegateView:
  ax::mojom::Role GetAccessibleWindowRole() override;
  gfx::Size CalculatePreferredSize() const override;
  void OnThemeChanged() override;

  FadeLabel* title_label_ = nullptr;
  FadeLabel* domain_label_ = nullptr;
  ThumbnailView* thumbnail_view_ = nullptr;
  absl::optional<TabAlertState> alert_state_;

  absl::optional<int> corner_radius_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_TAB_HOVER_CARD_BUBBLE_VIEW_H_
