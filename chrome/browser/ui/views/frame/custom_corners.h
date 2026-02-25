// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_CUSTOM_CORNERS_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_CUSTOM_CORNERS_H_

#include <optional>
#include <variant>

#include "base/callback_list.h"
#include "base/memory/raw_ref.h"
#include "base/scoped_observation.h"
#include "base/types/strong_alias.h"
#include "ui/color/color_id.h"
#include "ui/views/view.h"
#include "ui/views/view_observer.h"

class BrowserView;

// Shared base class for custom corners in the UI.
class CustomCorners : public views::ViewObserver {
 public:
  // Designates that the current frame color (either active or inactive) should
  // be used. If a theme is present, then that takes precedence. If you want to
  // force e.g. active color, use `kColorFrameActive` instead.
  using FrameTheme = base::StrongAlias<class FrameThemeTag, std::monostate>;

  // Designates that the toolbar theme background should be used.
  using ToolbarTheme = base::StrongAlias<class ToolbarThemeTag, std::monostate>;

  // Specifies which color to be used for the background.
  using ColorChoice = std::variant<FrameTheme, ToolbarTheme, ui::ColorId>;

  // Background to be overlaid on the corner's original background.
  struct FadeBackground {
    // Specifies which color to be used for the fade background.
    ColorChoice color;
    // Specifies the opacity to be used for the fade background, with 0.0 being
    // fully transparent and 1.0 being fully opaque.
    float opacity;

    bool operator==(const FadeBackground& other) const = default;
  };

  CustomCorners(const CustomCorners&) = delete;
  void operator=(const CustomCorners&) = delete;
  ~CustomCorners() override;

  // Fades the background of the region to `fade_background`. If
  // `fade_background` is nullopt, then the fade is removed.
  void SetFadeBackground(std::optional<FadeBackground> fade_background);

 protected:
  explicit CustomCorners(BrowserView&);

  const BrowserView& browser_view() const { return *browser_view_; }

  // Gets the target view to paint on.
  virtual const views::View& GetView() const = 0;

  // Handle the case where the browser's paint-as-active state changes.
  virtual void OnBrowserPaintAsActiveChanged() = 0;

  // Schedule a paint on the host view.
  virtual void SchedulePaintHost() = 0;

  // Paints the given `path` on `canvas` using `color_choice`.
  void PaintPath(gfx::Canvas* canvas,
                 const SkPath& path,
                 ColorChoice color_choice,
                 bool anti_alias) const;

 private:
  // views::ViewObserver:
  void OnViewAddedToWidget(views::View*) override;

  const raw_ref<const BrowserView> browser_view_;
  base::ScopedObservation<views::View, views::ViewObserver>
      browser_view_observation_{this};
  base::CallbackListSubscription browser_paint_as_active_subscription_;

  // Background to be overlaid on the corner's original background.
  std::optional<FadeBackground> fade_background_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_CUSTOM_CORNERS_H_
