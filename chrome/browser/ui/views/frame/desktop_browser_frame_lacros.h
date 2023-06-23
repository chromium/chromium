// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_DESKTOP_BROWSER_FRAME_LACROS_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_DESKTOP_BROWSER_FRAME_LACROS_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/frame/desktop_browser_frame_aura.h"

class BrowserFrame;
class BrowserView;
class BrowserDesktopWindowTreeHostLacros;

// Provides the window frame for the Chrome browser window on Lacros.
class DesktopBrowserFrameLacros : public DesktopBrowserFrameAura {
 public:
  DesktopBrowserFrameLacros(BrowserFrame* browser_frame,
                            BrowserView* browser_view);

  DesktopBrowserFrameLacros(const DesktopBrowserFrameLacros&) = delete;
  DesktopBrowserFrameLacros& operator=(const DesktopBrowserFrameLacros&) =
      delete;

  void set_host(BrowserDesktopWindowTreeHostLacros* host) { host_ = host; }

 protected:
  ~DesktopBrowserFrameLacros() override;

  // Overridden from NativeBrowserFrame:
  views::Widget::InitParams GetWidgetParams() override;
  void TabDraggingKindChanged(TabDragKind tab_drag_kind) override;

 private:
  raw_ptr<BrowserDesktopWindowTreeHostLacros> host_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_DESKTOP_BROWSER_FRAME_LACROS_H_
