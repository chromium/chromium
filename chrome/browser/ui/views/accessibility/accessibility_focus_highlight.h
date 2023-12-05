// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_ACCESSIBILITY_ACCESSIBILITY_FOCUS_HIGHLIGHT_H_
#define CHROME_BROWSER_UI_VIEWS_ACCESSIBILITY_ACCESSIBILITY_FOCUS_HIGHLIGHT_H_

#include <memory>

#include "base/callback_list.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "components/prefs/pref_change_registrar.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/compositor/compositor_animation_observer.h"
#include "ui/compositor/layer_delegate.h"
#include "ui/gfx/geometry/rect.h"

class BrowserView;

namespace content {
struct FocusedNodeDetails;
}  // namespace content

namespace ui {
class Compositor;
class Layer;
}  // namespace ui

// AccessibilityFocusHighlight manages an always-on-top layer used to
// highlight the focused UI element for accessibility.
class AccessibilityFocusHighlight : public ui::LayerDelegate,
                                    public ui::CompositorAnimationObserver,
                                    public TabStripModelObserver {
 public:
  explicit AccessibilityFocusHighlight(BrowserView* browser_view);
  ~AccessibilityFocusHighlight() override;

  // Disallow copy and assign.
  AccessibilityFocusHighlight(const AccessibilityFocusHighlight&) = delete;
  AccessibilityFocusHighlight& operator=(const AccessibilityFocusHighlight&) =
      delete;

 private:
  FRIEND_TEST_ALL_PREFIXES(AccessibilityFocusHighlightBrowserTest,
                           DrawsHighlight);
  FRIEND_TEST_ALL_PREFIXES(AccessibilityFocusHighlightBrowserTest,
                           FocusAppearance);
  // For testing.
  static void SetNoFadeForTesting();
  static void SkipActivationCheckForTesting();
  static void UseDefaultColorForTesting();
  ui::Layer* GetLayerForTesting();

  // Create the layer if needed, and set node_bounds_
  void CreateOrUpdateLayer(gfx::Rect node_bounds);

  // Get rid of the layer and stop animation.
  void RemoveLayer();

  // Handle preference changes by adding or removing observers as necessary.
  void AddOrRemoveObservers();

  // Handle focus change notifications.
  void OnFocusChangedInPage(const content::FocusedNodeDetails& details);

  // ui::LayerDelegate overrides:
  void OnPaintLayer(const ui::PaintContext& context) override;
  void OnDeviceScaleFactorChanged(float old_device_scale_factor,
                                  float new_device_scale_factor) override {}

  // CompositorAnimationObserver overrides:
  void OnAnimationStep(base::TimeTicks timestamp) override;
  void OnCompositingShuttingDown(ui::Compositor* compositor) override;

  // TabStripModelObserver
  void OnTabStripModelChanged(TabStripModel*,
                              const TabStripModelChange&,
                              const TabStripSelectionChange&) override;

  // Compute the highlight color based on theme colors and defaults.
  SkColor GetHighlightColor();

  // Compute the opacity based on the fade in and fade out times.
  // TODO(aboxhall): figure out how to use cubic beziers
  float ComputeOpacity(base::TimeDelta time_since_layer_create,
                       base::TimeDelta time_since_focus_move);

  // The layer, if visible.
  std::unique_ptr<ui::Layer> layer_;

  // The compositor associated with this layer.
  raw_ptr<ui::Compositor> compositor_ = nullptr;

  // The bounding rectangle of the focused object, relative to the layer.
  gfx::Rect node_bounds_;

  // Owns this.
  raw_ptr<BrowserView> browser_view_;

  // The time the layer was created and started fading in.
  base::TimeTicks layer_created_time_;

  // The most recent time the layer was updated because focus moved.
  base::TimeTicks focus_last_changed_time_;

  // Whether to skip fade in/fade out for testing.
  static bool no_fade_for_testing_;

  // The amount of time it should take for the highlight to fade in.
  static base::TimeDelta fade_in_time_;

  // The amount of time the highlight should persist between fading in and
  // fading out.
  static base::TimeDelta persist_time_;

  // The amount of time it should take for the highlight to fade out.
  static base::TimeDelta fade_out_time_;

  // If set, draws the highlight even if the widget is not active.
  static bool skip_activation_check_for_testing_;

  // If set, don't check the system theme color.
  static bool use_default_color_for_testing_;

  // For observing browser preference notifications.
  PrefChangeRegistrar profile_pref_registrar_;

  // For observing focus notifications.
  std::optional<base::CallbackListSubscription> focus_changed_subscription_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_ACCESSIBILITY_ACCESSIBILITY_FOCUS_HIGHLIGHT_H_
