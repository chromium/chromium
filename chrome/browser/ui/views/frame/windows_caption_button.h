// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_WINDOWS_CAPTION_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_WINDOWS_CAPTION_BUTTON_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/frame/windows_icon_painter.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/canvas.h"
#include "ui/views/controls/button/button.h"

class BrowserFrameViewWin;

class WindowsCaptionButton : public views::Button {
  METADATA_HEADER(WindowsCaptionButton, views::Button)

 public:
  WindowsCaptionButton(PressedCallback callback,
                       BrowserFrameViewWin* frame_view,
                       ViewID button_type,
                       const std::u16string& accessible_name);
  WindowsCaptionButton(const WindowsCaptionButton&) = delete;
  WindowsCaptionButton& operator=(const WindowsCaptionButton&) = delete;
  ~WindowsCaptionButton() override;

  // views::Button:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;
  void OnPaintBackground(gfx::Canvas* canvas) override;
  void PaintButtonContents(gfx::Canvas* canvas) override;

 private:
  std::unique_ptr<Windows10IconPainter> CreateIconPainter();

  // Returns the amount we should visually reserve on the left (right in RTL)
  // for spacing between buttons. We do this instead of repositioning the
  // buttons to avoid the sliver of deadspace that would result.
  int GetBetweenButtonSpacing() const;

  // Returns the order in which this button will be displayed (with 0 being
  // drawn farthest to the left, and larger indices being drawn to the right of
  // smaller indices).
  int GetButtonDisplayOrderIndex() const;

  // The base color to use for the button symbols and background blending. Uses
  // the more readable of black and white.
  SkColor GetBaseForegroundColor() const;

  // Paints the minimize/maximize/restore/close icon for the button.
  void PaintSymbol(gfx::Canvas* canvas);

  raw_ptr<BrowserFrameViewWin> frame_view_;
  std::unique_ptr<Windows10IconPainter> icon_painter_;
  ViewID button_type_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_WINDOWS_CAPTION_BUTTON_H_
