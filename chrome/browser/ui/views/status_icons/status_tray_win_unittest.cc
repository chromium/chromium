// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/status_icons/status_tray_win.h"

#include <commctrl.h>
#include <stddef.h>

#include <memory>
#include <utility>

#include "chrome/browser/status_icons/status_icon_menu_model.h"
#include "chrome/browser/status_icons/status_icon_observer.h"
#include "chrome/browser/ui/views/status_icons/status_icon_win.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/views/test/views_test_base.h"

namespace {

class FakeStatusTrayStateChangerProxy : public StatusTrayStateChangerProxy {
 public:
  FakeStatusTrayStateChangerProxy()
      : enqueue_called_(false), icon_id_(0), window_(NULL) {}

  FakeStatusTrayStateChangerProxy(const FakeStatusTrayStateChangerProxy&) =
      delete;
  FakeStatusTrayStateChangerProxy& operator=(
      const FakeStatusTrayStateChangerProxy&) = delete;

  void EnqueueChange(UINT icon_id, HWND window) override {
    enqueue_called_ = true;
    icon_id_ = icon_id;
    window_ = window;
  }

  bool enqueue_called() { return enqueue_called_; }
  UINT icon_id() { return icon_id_; }
  HWND window() { return window_; }

 private:
  bool enqueue_called_;
  UINT icon_id_;
  HWND window_;
};

class FakeStatusIconObserver : public StatusIconObserver {
 public:
  FakeStatusIconObserver()
      : status_icon_click_count_(0), balloon_clicked_(false) {}
  void OnStatusIconClicked() override { ++status_icon_click_count_; }
  void OnBalloonClicked() override { balloon_clicked_ = true; }
  bool balloon_clicked() const { return balloon_clicked_; }
  size_t status_icon_click_count() const {
    return status_icon_click_count_;
  }

 private:
  size_t status_icon_click_count_;
  bool balloon_clicked_;
};

}  // namespace

class StatusTrayWinTest : public ChromeViewsTestBase {
 protected:
  void SetUp() override {
    set_native_widget_type(NativeWidgetType::kDesktop);
    ChromeViewsTestBase::SetUp();
  }
};

StatusIconWin* CreateStatusIcon(StatusTray& tray) {
  return static_cast<StatusIconWin*>(tray.CreateStatusIcon(
      StatusTray::OTHER_ICON, gfx::test::CreateImageSkia(16, 16),
      std::u16string()));
}

void CreateContextMenu(StatusIconWin& icon) {
  auto menu = std::make_unique<StatusIconMenuModel>(nullptr);
  menu->AddItem(0, u"foo");
  icon.SetContextMenu(std::move(menu));
}

TEST_F(StatusTrayWinTest, CreateTray) {
  // Just tests creation/destruction.
  StatusTrayWin tray;
}

TEST_F(StatusTrayWinTest, CreateIconAndMenu) {
  // Create an icon, set the context menu, then shut down.
  StatusTrayWin tray;
  StatusIconWin* icon = CreateStatusIcon(tray);
  CreateContextMenu(*icon);
}

TEST_F(StatusTrayWinTest, ClickOnIcon) {
  // Create an icon, send a fake click event, make sure observer is called.
  StatusTrayWin tray;

  StatusIconWin* icon = CreateStatusIcon(tray);
  FakeStatusIconObserver observer;
  icon->AddObserver(&observer);
  // Mimic a click.
  tray.WndProc(nullptr, icon->message_id(), icon->icon_id(), WM_LBUTTONDOWN);
  // Mimic a right-click - observer should not be called.
  tray.WndProc(nullptr, icon->message_id(), icon->icon_id(), WM_RBUTTONDOWN);
  EXPECT_EQ(1u, observer.status_icon_click_count());
  icon->RemoveObserver(&observer);
}

TEST_F(StatusTrayWinTest, ContextMenu) {
  // Create icon and context menu.
  StatusTrayWin tray;
  StatusIconWin* icon = CreateStatusIcon(tray);
  CreateContextMenu(*icon);

  // Trigger the context menu.
  tray.WndProc(nullptr, icon->message_id(), icon->icon_id(), WM_RBUTTONDOWN);
}

TEST_F(StatusTrayWinTest, ClickOnBalloon) {
  // Create an icon, send a fake click event, make sure observer is called.
  StatusTrayWin tray;
  StatusIconWin* icon = CreateStatusIcon(tray);
  FakeStatusIconObserver observer;
  icon->AddObserver(&observer);
  // Mimic a click.
  tray.WndProc(nullptr, icon->message_id(), icon->icon_id(), TB_INDETERMINATE);
  EXPECT_TRUE(observer.balloon_clicked());
  icon->RemoveObserver(&observer);
}

TEST_F(StatusTrayWinTest, HandleOldIconId) {
  StatusTrayWin tray;
  StatusIconWin* icon = CreateStatusIcon(tray);
  UINT message_id = icon->message_id();
  UINT icon_id = icon->icon_id();

  tray.RemoveStatusIcon(icon);
  tray.WndProc(nullptr, message_id, icon_id, WM_LBUTTONDOWN);
}

TEST_F(StatusTrayWinTest, EnsureVisibleTest) {
  StatusTrayWin tray;

  FakeStatusTrayStateChangerProxy* proxy =
      new FakeStatusTrayStateChangerProxy();
  tray.SetStatusTrayStateChangerProxyForTest(
      std::unique_ptr<StatusTrayStateChangerProxy>(proxy));

  StatusIconWin* icon = CreateStatusIcon(tray);

  icon->ForceVisible();
  // |proxy| is owned by |tray|, and |tray| lives to the end of the scope,
  // so calling methods on |proxy| is safe.
  EXPECT_TRUE(proxy->enqueue_called());
  EXPECT_EQ(proxy->window(), icon->window());
  EXPECT_EQ(proxy->icon_id(), icon->icon_id());
}
