// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_EXTENSIONS_ICON_WITH_BADGE_IMAGE_SOURCE_H_
#define CHROME_BROWSER_UI_EXTENSIONS_ICON_WITH_BADGE_IMAGE_SOURCE_H_

#include <string>

#include "base/macros.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/image/canvas_image_source.h"
#include "ui/gfx/image/image.h"

namespace gfx {
class RenderText;
class Size;
}

// CanvasImageSource for creating extension icon with a badge.
class IconWithBadgeImageSource : public gfx::CanvasImageSource {
 public:
  // The data representing a badge to be painted over the base image.
  struct Badge {
    Badge(const std::string& text,
          SkColor text_color,
          SkColor background_color);
    ~Badge();

    std::string text;
    SkColor text_color;
    SkColor background_color;

   private:
    DISALLOW_COPY_AND_ASSIGN(Badge);
  };

  explicit IconWithBadgeImageSource(const gfx::Size& size);
  ~IconWithBadgeImageSource() override;

  void SetIcon(const gfx::Image& icon);
  void SetBadge(std::unique_ptr<Badge> badge);
  void set_grayscale(bool grayscale) { grayscale_ = grayscale; }
  void set_paint_page_action_decoration(bool should_paint) {
    paint_page_action_decoration_ = should_paint;
  }
  void set_paint_blocked_actions_decoration(bool should_paint) {
    paint_blocked_actions_decoration_ = should_paint;
  }
  bool grayscale() const { return grayscale_; }
  bool paint_page_action_decoration() const {
    return paint_page_action_decoration_;
  }
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

  // Whether or not to paint a decoration over the base icon to indicate the
  // represented action wants to run.
  bool paint_page_action_decoration_ = false;

  // Whether or not to paint a decoration to indicate that the extension has
  // had actions blocked.
  bool paint_blocked_actions_decoration_ = false;

  DISALLOW_COPY_AND_ASSIGN(IconWithBadgeImageSource);
};

#endif  // CHROME_BROWSER_UI_EXTENSIONS_ICON_WITH_BADGE_IMAGE_SOURCE_H_
