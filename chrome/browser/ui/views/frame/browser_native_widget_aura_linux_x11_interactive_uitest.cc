// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdint>
#include <memory>
#include <vector>

#include "base/callback_list.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/views/frame/browser_native_widget.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/browser_widget.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/color/color_provider_key.h"
#include "ui/color/color_provider_manager.h"
#include "ui/gfx/x/connection.h"
#include "ui/gfx/x/xproto.h"
#include "ui/ozone/public/ozone_platform.h"

namespace {

void VerifyColorsForFrameType(BrowserWindowInterface* browser,
                              bool use_custom_frame) {
  ThemeService* const theme_service =
      ThemeServiceFactory::GetForProfile(browser->GetProfile());
  EXPECT_EQ(use_custom_frame, theme_service->ShouldUseCustomFrame());

  ui::ColorProviderManager::ResetForTesting();

  bool initialized_color_provider_for_custom_frame = false;
  ui::ColorProviderManager::GetForTesting().AppendColorProviderInitializer(
      base::BindLambdaForTesting(
          [&initialized_color_provider_for_custom_frame](
              ui::ColorProvider* provider, const ui::ColorProviderKey& key) {
            initialized_color_provider_for_custom_frame =
                key.frame_type == ui::ColorProviderKey::FrameType::kChromium;
          }));
  ASSERT_NE(nullptr,
            BrowserView::GetBrowserViewForBrowser(browser)->GetColorProvider());
  EXPECT_EQ(use_custom_frame, initialized_color_provider_for_custom_frame);

  ui::ColorProviderManager::ResetForTesting();
}

class BrowserNativeWidgetAuraLinuxColdWmTest : public InProcessBrowserTest {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    InProcessBrowserTest::SetUpInProcessBrowserTestFixture();
    if (!ui::OzonePlatform::RunningOnX11ForTest()) {
      return;
    }
    wm_conn_ = std::make_unique<x11::Connection>();
    if (!wm_conn_->Ready()) {
      wm_conn_.reset();
      return;
    }
    const x11::Window root = wm_conn_->default_root();
    const x11::Atom check_atom = wm_conn_->GetAtom("_NET_SUPPORTING_WM_CHECK");
    const x11::Atom supported_atom = wm_conn_->GetAtom("_NET_SUPPORTED");
    had_wm_window_ =
        wm_conn_->GetPropertyAs(root, check_atom, &saved_wm_window_);
    had_supported_ =
        wm_conn_->GetArrayProperty(root, supported_atom, &saved_supported_);
    wm_conn_->DeleteProperty(root, check_atom);
    wm_conn_->DeleteProperty(root, supported_atom);
    wm_conn_->Sync();
  }

  void TearDownInProcessBrowserTestFixture() override {
    if (wm_conn_) {
      const x11::Window root = wm_conn_->default_root();
      const x11::Atom check_atom =
          wm_conn_->GetAtom("_NET_SUPPORTING_WM_CHECK");
      const x11::Atom supported_atom = wm_conn_->GetAtom("_NET_SUPPORTED");
      if (had_wm_window_) {
        wm_conn_->SetProperty(root, check_atom, x11::Atom::WINDOW,
                              saved_wm_window_);
      } else {
        wm_conn_->DeleteProperty(root, check_atom);
      }
      if (had_supported_) {
        wm_conn_->SetArrayProperty(root, supported_atom, x11::Atom::ATOM,
                                   saved_supported_);
      } else {
        wm_conn_->DeleteProperty(root, supported_atom);
      }
      if (dummy_wm_ != x11::Window::None) {
        wm_conn_->DestroyWindow({dummy_wm_});
      }
      wm_conn_->Sync();
      wm_conn_.reset();
    }
    InProcessBrowserTest::TearDownInProcessBrowserTestFixture();
  }

 protected:
  std::unique_ptr<x11::Connection> wm_conn_;
  x11::Window dummy_wm_ = x11::Window::None;

 private:
  bool had_wm_window_ = false;
  x11::Window saved_wm_window_ = x11::Window::None;
  bool had_supported_ = false;
  std::vector<x11::Atom> saved_supported_;
};

// Regression test for crbug.com/40221199: when Chrome starts on cold on-demand
// XWayland before the window manager publishes EWMH properties, the initial
// browser window starts with native decorations and switches to the custom
// frame once the stacking window manager finishes starting up (unless the user
// explicitly configured the preference).
IN_PROC_BROWSER_TEST_F(BrowserNativeWidgetAuraLinuxColdWmTest,
                       SwitchesToCustomFrameWhenWindowManagerStarts) {
  if (!ui::OzonePlatform::RunningOnX11ForTest() || !wm_conn_) {
    GTEST_SKIP();
  }

  BrowserView* const browser_view =
      BrowserView::GetBrowserViewForBrowser(browser());
  const BrowserNativeWidget* const native_widget =
      browser_view->browser_widget()->browser_native_widget();
  PrefService* const pref_service = browser()->GetProfile()->GetPrefs();

  ASSERT_FALSE(native_widget->UseCustomFrame());
  VerifyColorsForFrameType(browser(), false);

  PrefChangeRegistrar registrar;
  registrar.Init(pref_service);
  base::RunLoop pref_changed_loop;
  registrar.Add(prefs::kUseCustomChromeFrame, pref_changed_loop.QuitClosure());

  const x11::Window root = wm_conn_->default_root();
  const x11::Atom check_atom = wm_conn_->GetAtom("_NET_SUPPORTING_WM_CHECK");
  const x11::Atom supported_atom = wm_conn_->GetAtom("_NET_SUPPORTED");
  dummy_wm_ = wm_conn_->CreateDummyWindow("Test WM");
  wm_conn_->SetProperty(dummy_wm_, check_atom, x11::Atom::WINDOW, dummy_wm_);
  wm_conn_->SetStringProperty(dummy_wm_, wm_conn_->GetAtom("_NET_WM_NAME"),
                              wm_conn_->GetAtom("UTF8_STRING"), "GNOME Shell");
  wm_conn_->SetArrayProperty(
      root, supported_atom, x11::Atom::ATOM,
      std::vector<x11::Atom>{wm_conn_->GetAtom("_NET_WM_MOVERESIZE"),
                             wm_conn_->GetAtom("_GTK_FRAME_EXTENTS")});
  wm_conn_->SetProperty(root, check_atom, x11::Atom::WINDOW, dummy_wm_);
  wm_conn_->Sync();

  pref_changed_loop.Run();

  EXPECT_TRUE(native_widget->UseCustomFrame());
  VerifyColorsForFrameType(browser(), true);

  x11::Connection::Get()->Sync();
  const x11::Window xwindow =
      static_cast<x11::Window>(browser_view->GetWidget()
                                   ->GetNativeWindow()
                                   ->GetHost()
                                   ->GetAcceleratedWidget());
  uint32_t hide_titlebar = 0;
  ASSERT_TRUE(wm_conn_->GetPropertyAs(
      xwindow, wm_conn_->GetAtom("_GTK_HIDE_TITLEBAR_WHEN_MAXIMIZED"),
      &hide_titlebar));
  EXPECT_EQ(1u, hide_titlebar);
  std::vector<uint32_t> frame_extents;
  ASSERT_TRUE(wm_conn_->GetArrayProperty(
      xwindow, wm_conn_->GetAtom("_GTK_FRAME_EXTENTS"), &frame_extents));
  EXPECT_EQ(4u, frame_extents.size());

  // An explicit user preference overrides the platform default across WM
  // restarts.
  pref_service->SetBoolean(prefs::kUseCustomChromeFrame, false);
  EXPECT_FALSE(native_widget->UseCustomFrame());

  base::RunLoop default_false_loop;
  base::CallbackListSubscription sub1 =
      ui::OzonePlatform::GetInstance()
          ->RegisterCustomFramePrefDefaultChangedCallback(
              default_false_loop.QuitClosure());
  wm_conn_->DeleteProperty(root, check_atom);
  wm_conn_->Sync();
  default_false_loop.Run();
  sub1 = {};

  base::RunLoop default_true_loop;
  base::CallbackListSubscription sub2 =
      ui::OzonePlatform::GetInstance()
          ->RegisterCustomFramePrefDefaultChangedCallback(
              default_true_loop.QuitClosure());
  wm_conn_->SetProperty(root, check_atom, x11::Atom::WINDOW, dummy_wm_);
  wm_conn_->Sync();
  default_true_loop.Run();
  sub2 = {};

  EXPECT_FALSE(native_widget->UseCustomFrame());
}

}  // namespace
