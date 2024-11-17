// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_TAB_ICON_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_TAB_ICON_H_

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/ui/tabs/tab_network_state.h"
#include "components/performance_manager/public/features.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/gfx/animation/linear_animation.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/paint_throbber.h"
#include "ui/views/animation/animation_delegate_views.h"
#include "ui/views/view.h"

namespace base {
class TickClock;
}

struct TabRendererData;

DECLARE_CUSTOM_ELEMENT_EVENT_TYPE(kDiscardAnimationFinishes);

// View that displays the favicon, sad tab, throbber, and attention indicator
// in a tab.
//
// The icon will be drawn in the upper left (upper right for RTL). Normally you
// would lay this out so the top is where you want the icon to be positioned,
// the width is TabIcon::GetIdealWidth(), and the height goes down to the
// bottom of the enclosing view (this is so the crashed tab can animate out of
// the bottom).
class TabIcon : public views::View, public views::AnimationDelegateViews {
  METADATA_HEADER(TabIcon, views::View)

 public:
  // Attention indicator types (use as a bitmask). There is only one visual
  // representation, but the state of each of these is tracked separately and
  // the indicator is shown as long as one is enabled.
  enum class AttentionType {
    kBlockedWebContents = 1 << 0,       // The WebContents is marked as blocked.
    kTabWantsAttentionStatus = 1 << 1,  // Tab::SetTabNeedsAttention() called.
  };

  TabIcon();
  TabIcon(const TabIcon&) = delete;
  TabIcon& operator=(const TabIcon&) = delete;
  ~TabIcon() override;

  // Sets the tab data (network state, favicon, load progress, etc.) that are
  // used to render the tab icon.
  void SetData(const TabRendererData& data);

  // Sets whether this tab is currently active.
  void SetActiveState(bool is_active);

  // Enables or disables the given attention type. The attention indicator
  // will be shown as long as any of the types are enabled.
  void SetAttention(AttentionType type, bool enabled);

  bool GetShowingLoadingAnimation() const;
  bool GetShowingAttentionIndicator() const;
  bool GetShowingDiscardIndicator() const;

  // Sets whether this object can paint to a layer. When the loading animation
  // is running, painting to a layer saves painting overhead. But if the tab is
  // being painted to some other context than the window, the layered painting
  // won't work.
  void SetCanPaintToLayer(bool can_paint_to_layer);

  // The loading animation only steps when this function is called. The
  // |elapsed_time| parameter is expected to be the same among all tabs in a tab
  // strip in order to keep the throbbers in sync.
  void StepLoadingAnimation(const base::TimeDelta& elapsed_time);

  gfx::ImageSkia GetThemedIconForTesting() { return themed_favicon_; }
  bool GetActiveStateForTesting() { return is_active_tab_; }

  void EnlargeDiscardIndicatorRadius(int radius);
  void SetShouldShowDiscardIndicator(bool enabled);

 private:
  class CrashAnimation;
  friend CrashAnimation;
  friend class TabTest;
  FRIEND_TEST_ALL_PREFIXES(TabTest, DiscardIndicatorResponsiveness);

  // views::View:
  void OnPaint(gfx::Canvas* canvas) override;
  void OnThemeChanged() override;

  // views::AnimationDelegateViews:
  void AnimationProgressed(const gfx::Animation* animation) override;
  void AnimationEnded(const gfx::Animation* animation) override;

  // Paints the attention indicator and |favicon_| at the given location.
  void PaintAttentionIndicatorAndIcon(gfx::Canvas* canvas,
                                      const gfx::ImageSkia& icon,
                                      const gfx::Rect& bounds);

  // Paints a dimmed and shrunken favicon surrounded by the discard ring
  void PaintDiscardRingAndIcon(gfx::Canvas* canvas,
                               const gfx::ImageSkia& icon,
                               const gfx::Rect& bounds);

  // Paint either the indeterimate throbber or progress indicator according to
  // current tab state.
  void PaintLoadingAnimation(gfx::Canvas* canvas, gfx::Rect bounds);

  // Gets either the crashed icon or favicon to be rendered for the tab.
  gfx::ImageSkia GetIconToPaint();

  // Paint the favicon if it's available.
  void MaybePaintFavicon(gfx::Canvas* canvas,
                         const gfx::ImageSkia& icon,
                         const gfx::Rect& bounds);
  bool GetNonDefaultFavicon() const;

  // Sets the icon.
  void SetIcon(const ui::ImageModel& icon, bool should_themify_favicon);

  // Start or stops the favicon fade animation for discard tabs
  void SetDiscarded(bool show_discard_status);

  // For certain types of tabs the loading animation is not desired so the
  // caller can set inhibit_loading_animation to true. When false, the loading
  // animation state will be derived from the network state.
  void SetNetworkState(TabNetworkState network_state);

  // Sets whether the tab should paint as crashed or not.
  void SetCrashed(bool crashed);
  bool GetCrashed() const;

  // Creates or destroys the layer according to the current animation state and
  // whether a layer can be used.
  void RefreshLayer();

  gfx::ImageSkia ThemeFavicon(const gfx::ImageSkia& source);
  gfx::ImageSkia ThemeMonochromeFavicon(const gfx::ImageSkia& source);

  // Updates the themed favicon if necessary.
  void UpdateThemedFavicon();

  raw_ptr<const base::TickClock> clock_;

  ui::ImageModel favicon_;
  bool should_themify_favicon_ = false;
  TabNetworkState network_state_ = TabNetworkState::kNone;
  bool crashed_ = false;
  int attention_types_ = 0;  // Bitmask of AttentionType.

  // Value from last call to SetNetworkState. When true, the network loading
  // animation will not be shown.
  bool inhibit_loading_animation_ = false;

  // The point in time when the tab icon was first painted in the loading state.
  base::TimeTicks loading_animation_start_time_;

  // When the favicon_ has theming applied to it, the themed version will be
  // cached here. If this isNull(), then there is no theming and favicon_
  // should be used.
  gfx::ImageSkia themed_favicon_;

  // May be different than is_crashed when the crashed icon is animating in.
  bool should_display_crashed_favicon_ = false;

  // Drawn when should_display_crashed_favicon_ is set. Created lazily.
  gfx::ImageSkia crashed_icon_;

  // The fraction the icon is hidden by for the crashed tab animation.
  // When this is 0 it will be drawn at the normal location, and when this is 1
  // it will be drawn off the bottom.
  double hiding_fraction_ = 0.0;

  // Animation used when the favicon grows or shrinks in size. `Show` will
  // represent the favicon growing to full size, while `Hide` will represent the
  // favicon shrinking which happens when the loading spinner or discard
  // indicator is present.
  gfx::SlideAnimation favicon_size_animation_;

  // Animation used when a tab is discarded so the favicon will partially
  // fade out
  gfx::LinearAnimation tab_discard_animation_;

  // The discard indicator will be shown only if the tab is discarded and the
  // discard ring treatment pref is enabled. Keep track of both of the component
  // booleans, in order to determine if the discard indicator is shown/unshown
  // due to a change in the discard status or a change to the pref, because
  // we don't want to animate the discard ring in the latter case.
  bool is_discarded_ = false;
  bool should_show_discard_indicator_ = true;
  bool was_discard_indicator_shown_ = false;

  // Crash animation (in place of favicon). Lazily created since most of the
  // time it will be unneeded.
  std::unique_ptr<CrashAnimation> crash_animation_;

  bool can_paint_to_layer_ = false;

  bool has_tab_renderer_data_ = false;

  bool is_active_tab_ = false;

  bool is_monochrome_favicon_ = false;

  int increased_discard_indicator_radius_ = 0;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_TAB_ICON_H_
