// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_NATIVE_WIDGET_AURA_LINUX_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_NATIVE_WIDGET_AURA_LINUX_H_

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/frame/browser_native_widget_aura.h"
#include "components/prefs/pref_member.h"

class BrowserDesktopWindowTreeHostLinux;

// Provides the window frame for the Chrome browser window on Desktop Linux/X11.
class BrowserNativeWidgetAuraLinux : public BrowserNativeWidgetAura {
 public:
  BrowserNativeWidgetAuraLinux(BrowserWidget* browser_widget,
                               BrowserView* browser_view);

  BrowserNativeWidgetAuraLinux(const BrowserNativeWidgetAuraLinux&) = delete;
  BrowserNativeWidgetAuraLinux& operator=(const BrowserNativeWidgetAuraLinux&) =
      delete;

  // BrowserNativeWidgetAura:
  void OnHostClosed() override;
  bool UseCustomFrame() const override;

  bool ShouldDrawRestoredFrameShadow() const;

  void set_host(BrowserDesktopWindowTreeHostLinux* host) { host_ = host; }

 protected:
  ~BrowserNativeWidgetAuraLinux() override;

  // BrowserNativeWidget:
  views::Widget::InitParams GetWidgetParams(
      views::Widget::InitParams::Ownership ownership) override;
  void TabDraggingKindChanged(TabDragKind tab_drag_kind) override;
  void ClientDestroyedWidget() override;

 private:
  FRIEND_TEST_ALL_PREFIXES(BrowserNativeWidgetAuraLinuxTest, UseCustomFrame);

  // Called when the preference changes.
  void OnUseCustomChromeFrameChanged();

  // Whether the custom Chrome frame preference is set.
  BooleanPrefMember use_custom_frame_pref_;

  raw_ptr<BrowserDesktopWindowTreeHostLinux> host_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_NATIVE_WIDGET_AURA_LINUX_H_
