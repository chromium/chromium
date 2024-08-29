// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_TAB_HOVER_CARD_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_TAB_HOVER_CARD_BUBBLE_VIEW_H_

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_utils.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/tabs/fade_footer_view.h"
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
class TabStyle;
class FadeLabelView;

// Dialog that displays an informational hover card containing page information.
class TabHoverCardBubbleView : public views::BubbleDialogDelegateView {
  METADATA_HEADER(TabHoverCardBubbleView, views::BubbleDialogDelegateView)

 public:
  static constexpr base::TimeDelta kHoverCardSlideDuration =
      base::Milliseconds(200);

  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kHoverCardBubbleElementId);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kHoverCardDomainLabelElementId);

  struct InitParams {
    // Becomes false with certain accessibility options and in tests
    bool use_animation = true;
    // Becomes false for ChromeOS tabbed system apps (e.g. Terminal)
    bool show_domain = true;
    // Becomes false when image preview setting is disabled
    bool show_image_preview = true;
    // Becomes false for ChromeOS system apps or when disabled in settings
    bool show_memory_usage = true;
  };

  explicit TabHoverCardBubbleView(Tab* tab, const InitParams& params);
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
  views::View* GetThumbnailViewForTesting();
  FooterView* GetFooterViewForTesting();

  // Returns the percentage complete during transition animations when a
  // pre-emptive crossfade to a placeholder should start if a new image is not
  // available, or `std::nullopt` to disable crossfades entirely.
  static std::optional<double> GetPreviewImageCrossfadeStart();

 private:
  FRIEND_TEST_ALL_PREFIXES(TabHoverCardFadeFooterInteractiveUiTest,
                           BackgroundTabHoverCardContentsHaveCorrectDimensions);
  class ThumbnailView;

  // views::BubbleDialogDelegateView:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;

  raw_ptr<FadeLabelView> title_label_ = nullptr;
  raw_ptr<FadeLabelView> domain_label_ = nullptr;
  raw_ptr<ThumbnailView> thumbnail_view_ = nullptr;
  raw_ptr<FooterView> footer_view_ = nullptr;
  std::optional<TabAlertState> alert_state_;
  const raw_ptr<const TabStyle> tab_style_;

  const InitParams bubble_params_;
  int corner_radius_ = ChromeLayoutProvider::Get()->GetCornerRadiusMetric(
      views::Emphasis::kHigh);
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_TAB_HOVER_CARD_BUBBLE_VIEW_H_
