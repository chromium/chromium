// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_WINDOWS_10_CAPTION_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_WINDOWS_10_CAPTION_BUTTON_H_

#include "chrome/browser/ui/view_ids.h"
#include "ui/gfx/canvas.h"
#include "ui/views/controls/button/button.h"

class GlassBrowserFrameView;

class Windows10CaptionButton : public views::Button {
 public:
  Windows10CaptionButton(PressedCallback callback,
                         GlassBrowserFrameView* frame_view,
                         ViewID button_type,
                         const base::string16& accessible_name);

  // views::Button:
  gfx::Size CalculatePreferredSize() const override;
  void OnPaintBackground(gfx::Canvas* canvas) override;
  void PaintButtonContents(gfx::Canvas* canvas) override;

 private:
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
  SkColor GetBaseColor() const;

  // Paints the minimize/maximize/restore/close icon for the button.
  void PaintSymbol(gfx::Canvas* canvas);

  GlassBrowserFrameView* frame_view_;
  ViewID button_type_;

  DISALLOW_COPY_AND_ASSIGN(Windows10CaptionButton);
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_WINDOWS_10_CAPTION_BUTTON_H_
