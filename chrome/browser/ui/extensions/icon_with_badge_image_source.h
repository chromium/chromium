// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_EXTENSIONS_ICON_WITH_BADGE_IMAGE_SOURCE_H_
#define CHROME_BROWSER_UI_EXTENSIONS_ICON_WITH_BADGE_IMAGE_SOURCE_H_

#include <string>

#include "base/functional/callback.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/image/canvas_image_source.h"
#include "ui/gfx/image/image.h"

namespace gfx {
class RenderText;
class Size;
}

namespace ui {
class ColorProvider;
}

// CanvasImageSource for creating extension icon with a badge.
class IconWithBadgeImageSource : public gfx::CanvasImageSource {
 public:
  // The data representing a badge to be painted over the base image.
  struct Badge {
    Badge(const std::string& text,
          SkColor text_color,
          SkColor background_color);

    Badge(const Badge&) = delete;
    Badge& operator=(const Badge&) = delete;

    ~Badge();

    std::string text;
    SkColor text_color;
    SkColor background_color;
  };

  using GetColorProviderCallback =
      base::RepeatingCallback<const ui::ColorProvider*()>;
  IconWithBadgeImageSource(
      const gfx::Size& size,
      GetColorProviderCallback get_color_provider_callback);

  IconWithBadgeImageSource(const IconWithBadgeImageSource&) = delete;
  IconWithBadgeImageSource& operator=(const IconWithBadgeImageSource&) = delete;

  ~IconWithBadgeImageSource() override;

  void SetIcon(const gfx::Image& icon);
  void SetBadge(std::unique_ptr<Badge> badge);
  void set_grayscale(bool grayscale) { grayscale_ = grayscale; }
  void set_paint_blocked_actions_decoration(bool should_paint) {
    paint_blocked_actions_decoration_ = should_paint;
  }
  bool grayscale() const { return grayscale_; }
  bool paint_blocked_actions_decoration() const {
    return paint_blocked_actions_decoration_;
  }

 private:
  // gfx::CanvasImageSource:
  void Draw(gfx::Canvas* canvas) override;

  // Paints |badge_|, if any, on |canvas|.
  void PaintBadge(gfx::Canvas* canvas);

  // Paints a decoration over the base icon to indicate that the action wants to
  // run.
  void PaintPageActionDecoration(gfx::Canvas* canvas);

  // Paints a decoration over the base icon to indicate that the extension has
  // a blocked action that wants to run.
  void PaintBlockedActionDecoration(gfx::Canvas* canvas);

  // The toolbar action view may have different values of paddings depending on
  // the current material design mode (See ToolbarActionsBar::GetViewSize()). In
  // all cases, our badges and decorations should be positions at the corners of
  // the area where the icon exists (ignoring all the paddings).
  // https://crbug.com/831946.
  gfx::Rect GetIconAreaRect() const;

  GetColorProviderCallback get_color_provider_callback_;

  // The base icon to draw.
  gfx::Image icon_;

  // An optional badge to draw over the base icon.
  std::unique_ptr<Badge> badge_;

  // The badge text to draw if a badge exists.
  std::unique_ptr<gfx::RenderText> badge_text_;

  // The badge's background display rectangle area.
  gfx::Rect badge_background_rect_;

  // Whether or not the icon should be grayscaled (e.g., to show it is
  // disabled).
  bool grayscale_ = false;

  // Whether or not to paint a decoration to indicate that the extension has
  // had actions blocked.
  // TODO(crbug.com/40857680): Remove once kExtensionsMenuAccessControl is
  // rolled out.
  bool paint_blocked_actions_decoration_ = false;
};

#endif  // CHROME_BROWSER_UI_EXTENSIONS_ICON_WITH_BADGE_IMAGE_SOURCE_H_
