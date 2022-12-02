// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_DESKTOP_BROWSER_FRAME_AURA_LINUX_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_DESKTOP_BROWSER_FRAME_AURA_LINUX_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/frame/desktop_browser_frame_aura.h"

#include "base/gtest_prod_util.h"
#include "components/prefs/pref_member.h"

class BrowserDesktopWindowTreeHostLinux;

// Provides the window frame for the Chrome browser window on Desktop Linux/X11.
class DesktopBrowserFrameAuraLinux : public DesktopBrowserFrameAura {
 public:
  DesktopBrowserFrameAuraLinux(BrowserFrame* browser_frame,
                               BrowserView* browser_view);

  DesktopBrowserFrameAuraLinux(const DesktopBrowserFrameAuraLinux&) = delete;
  DesktopBrowserFrameAuraLinux& operator=(const DesktopBrowserFrameAuraLinux&) =
      delete;

  bool ShouldDrawRestoredFrameShadow() const;

  void set_host(BrowserDesktopWindowTreeHostLinux* host) { host_ = host; }

 protected:
  ~DesktopBrowserFrameAuraLinux() override;

  // NativeBrowserFrame:
  views::Widget::InitParams GetWidgetParams() override;
  bool UseCustomFrame() const override;
  void TabDraggingKindChanged(TabDragKind tab_drag_kind) override;

 private:
  FRIEND_TEST_ALL_PREFIXES(DesktopBrowserFrameAuraLinuxTest, UseCustomFrame);

  // Called when the preference changes.
  void OnUseCustomChromeFrameChanged();

  // Whether the custom Chrome frame preference is set.
  BooleanPrefMember use_custom_frame_pref_;

  raw_ptr<BrowserDesktopWindowTreeHostLinux> host_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_DESKTOP_BROWSER_FRAME_AURA_LINUX_H_
