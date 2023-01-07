// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_WINDOWS_ICON_PAINTER_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_WINDOWS_ICON_PAINTER_H_

#include "base/memory/raw_ptr.h"
#include "ui/gfx/canvas.h"

class Windows10IconPainter {
 public:
  Windows10IconPainter();
  virtual ~Windows10IconPainter();

  Windows10IconPainter(const Windows10IconPainter&) = delete;
  Windows10IconPainter& operator=(const Windows10IconPainter&) = delete;

 public:
  // Paints the minimize icon for the button
  virtual void PaintMinimizeIcon(gfx::Canvas* canvas,
                                 gfx::Rect& symbol_rect,
                                 cc::PaintFlags& flags);

  // Paints the maximize icon for the button
  virtual void PaintMaximizeIcon(gfx::Canvas* canvas,
                                 gfx::Rect& symbol_rect,
                                 cc::PaintFlags& flags);

  // Paints the restore icon for the button
  virtual void PaintRestoreIcon(gfx::Canvas* canvas,
                                gfx::Rect& symbol_rect,
                                cc::PaintFlags& flags);

  // Paints the close icon for the button
  virtual void PaintCloseIcon(gfx::Canvas* canvas,
                              gfx::Rect& symbol_rect,
                              cc::PaintFlags& flags);

  // Paints the tab search icon for the button
  virtual void PaintTabSearchIcon(gfx::Canvas* canvas,
                                  gfx::Rect& symbol_rect,
                                  cc::PaintFlags& flags);
};

class Windows11IconPainter : public Windows10IconPainter {
 public:
  Windows11IconPainter();
  ~Windows11IconPainter() override;

  Windows11IconPainter(const Windows11IconPainter&) = delete;
  Windows11IconPainter& operator=(const Windows11IconPainter&) = delete;

 public:
  // Paints the maximize icon for the button
  void PaintMaximizeIcon(gfx::Canvas* canvas,
                         gfx::Rect& symbol_rect,
                         cc::PaintFlags& flags) override;

  // Paints the restore icon for the button
  void PaintRestoreIcon(gfx::Canvas* canvas,
                        gfx::Rect& symbol_rect,
                        cc::PaintFlags& flags) override;
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_WINDOWS_ICON_PAINTER_H_
