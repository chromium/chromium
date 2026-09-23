// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_CUSTOM_CORNERS_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_CUSTOM_CORNERS_H_

#include <array>
#include <variant>

#include "base/callback_list.h"
#include "base/memory/raw_ref.h"
#include "base/scoped_observation.h"
#include "base/types/strong_alias.h"
#include "ui/color/color_id.h"
#include "ui/views/view.h"
#include "ui/views/view_observer.h"

class BrowserView;

// Specifies which corner something refers to.
enum class CornerOrientation {
  kTopLeading,
  kTopTrailing,
  kBottomLeading,
  kBottomTrailing,
};

// Shared base class for custom corners in the UI.
class CustomCorners : public views::ViewObserver {
 public:
  // Represents a mapping from corner enum `C` to some data `T`.
  template <typename C, typename T>
  class CornerMapT {
   public:
    CornerMapT() = default;
    CornerMapT(const CornerMapT&) = default;
    CornerMapT& operator=(const CornerMapT&) = default;
    ~CornerMapT() = default;

    // Dereference by corner.
    T& operator[](C selector) {
      return corners_[static_cast<size_t>(selector)];
    }
    const T& operator[](C selector) const {
      return corners_[static_cast<size_t>(selector)];
    }

    // Comparison operator to detect changes.
    bool operator==(const CornerMapT& other) {
      for (size_t i = 0; i < corners_.size(); ++i) {
        if (corners_[i] != other.corners_[i]) {
          return false;
        }
      }
      return true;
    }

   private:
    std::array<T, 4> corners_;
  };

  // Designates that the current frame color (either active or inactive) should
  // be used. If a theme is present, then that takes precedence. If you want to
  // force e.g. active color, use `kColorFrameActive` instead.
  using FrameTheme = base::StrongAlias<class FrameThemeTag, std::monostate>;

  // Designates that the toolbar theme background should be used.
  using ToolbarTheme = base::StrongAlias<class ToolbarThemeTag, std::monostate>;

  // Specifies which color to be used for the background.
  using ColorChoice = std::variant<FrameTheme, ToolbarTheme, ui::ColorId>;

  // Represents a color choice with optional alpha. Special-case logic will be
  // used to optimize fully opaque and fully transparent surfaces.
  struct ColorChoiceWithAlpha {
    ColorChoiceWithAlpha() = default;
    inline explicit ColorChoiceWithAlpha(ColorChoice color_,
                                         float opacity_ = 1.0f)
        : color(color_), opacity(opacity_) {}
    ColorChoiceWithAlpha(const ColorChoiceWithAlpha& other) = default;
    ColorChoiceWithAlpha& operator=(const ColorChoiceWithAlpha& other) =
        default;

    friend bool operator==(const ColorChoiceWithAlpha& left,
                           const ColorChoiceWithAlpha& right) = default;

    // Specifies the color or theme to be used to render the surface.
    ColorChoice color;

    // Specifies the opacity to be used for the surface, from 0 (fully
    // transparent) to 1 (fully opaque).
    float opacity = 1.0f;

    inline bool is_opaque() const { return opacity == 1.0; }
    inline bool is_visible() const { return opacity > 0.0; }
  };

  // A color ID plus an alpha value.
  struct ColorWithAlpha {
    // The color ID.
    ui::ColorId color = ui::kColorSeparator;

    // The opacity. Special handling may be applied if this value is 0 (fully
    // transparent) or 1 (full opaque).
    float opacity = 1.0f;

    friend bool operator==(const ColorWithAlpha& left,
                           const ColorWithAlpha& right) = default;

    inline bool is_opaque() const { return opacity == 1.0; }
    inline bool is_visible() const { return opacity > 0.0; }
  };

  CustomCorners(const CustomCorners&) = delete;
  void operator=(const CustomCorners&) = delete;
  ~CustomCorners() override;

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
                 ColorChoiceWithAlpha color_choice,
                 bool anti_alias) const;

  // This represents which corner something is visually, which is different from
  // CornerOrientation because the latter is expressed as leading/trailing,
  // which can change between LtR/RtL.
  //
  // Use ToVisualCorner to convert from CornerOrientation to VisualCorner.
  enum class VisualCorner { kTopLeft, kTopRight, kBottomRight, kBottomLeft };

  // Possibly mirrors a corner for RtL.
  static VisualCorner ToVisualCorner(CornerOrientation orientation);

  // Gets the outline path for a corner.
  //
  // The entire shape will be drawn `in_bounds`, with the actual curve of the
  // corner drawn in `in_bounds` - `insets`. (Flat edges will be extended out to
  // the edge of `in_bounds`).
  static SkPath GetCornerPath(VisualCorner corner,
                              const gfx::Rect& in_bounds,
                              const gfx::Insets& insets);

 private:
  // views::ViewObserver:
  void OnViewAddedToWidget(views::View*) override;

  const raw_ref<const BrowserView> browser_view_;
  base::ScopedObservation<views::View, views::ViewObserver>
      browser_view_observation_{this};
  base::CallbackListSubscription browser_paint_as_active_subscription_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_CUSTOM_CORNERS_H_
