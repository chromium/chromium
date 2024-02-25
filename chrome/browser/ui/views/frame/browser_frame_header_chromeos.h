// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_FRAME_HEADER_CHROMEOS_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_FRAME_HEADER_CHROMEOS_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "chromeos/ui/frame/frame_header.h"
#include "ui/gfx/image/image_skia.h"

// Helper class for drawing a custom frame (such as for a themed Chrome Browser
// frame).
class BrowserFrameHeaderChromeOS : public chromeos::FrameHeader {
 public:
  class AppearanceProvider {
   public:
    virtual ~AppearanceProvider() = default;
    virtual SkColor GetTitleColor() = 0;
    virtual SkColor GetFrameHeaderColor(bool active) = 0;
    virtual gfx::ImageSkia GetFrameHeaderImage(bool active) = 0;
    virtual int GetFrameHeaderImageYInset() = 0;
    virtual gfx::ImageSkia GetFrameHeaderOverlayImage(bool active) = 0;
  };

  // BrowserFrameHeaderChromeOS does not take ownership of any of the
  // parameters. |target_widget| is the widget that the caption buttons act on.
  // |view| is the view into which |this| will paint.
  BrowserFrameHeaderChromeOS(
      views::Widget* target_widget,
      views::View* view,
      AppearanceProvider* appearance_provider,
      chromeos::FrameCaptionButtonContainerView* caption_button_container);

  BrowserFrameHeaderChromeOS(const BrowserFrameHeaderChromeOS&) = delete;
  BrowserFrameHeaderChromeOS& operator=(const BrowserFrameHeaderChromeOS&) =
      delete;

  ~BrowserFrameHeaderChromeOS() override;

  // FrameHeader:
  void UpdateFrameColors() override;
  SkPath GetWindowMaskForFrameHeader(const gfx::Size& size) override;

 protected:
  // FrameHeader:
  void DoPaintHeader(gfx::Canvas* canvas) override;
  views::CaptionButtonLayoutSize GetButtonLayoutSize() const override;
  SkColor GetTitleColor() const override;
  SkColor GetCurrentFrameColor() const override;

 private:
  // Paints the frame image.
  void PaintFrameImages(gfx::Canvas* canvas);

  raw_ptr<AppearanceProvider> appearance_provider_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_FRAME_HEADER_CHROMEOS_H_
