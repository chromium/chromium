// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/status_icons/status_tray_win.h"

#include <commctrl.h>
#include <stddef.h>

#include <utility>

#include "chrome/browser/status_icons/status_icon_menu_model.h"
#include "chrome/browser/status_icons/status_icon_observer.h"
#include "chrome/browser/ui/views/status_icons/status_icon_win.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_unittest_util.h"

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

StatusIconWin* CreateStatusIcon(StatusTray* tray) {
  return static_cast<StatusIconWin*>(tray->CreateStatusIcon(
      StatusTray::OTHER_ICON, gfx::test::CreateImageSkia(16, 16),
      std::u16string()));
}

}  // namespace

TEST(StatusTrayWinTest, CreateTray) {
  // Just tests creation/destruction.
  StatusTrayWin tray;
}

TEST(StatusTrayWinTest, CreateIconAndMenu) {
  // Create an icon, set the images, tooltip, and context menu, then shut it
  // down.
  StatusTrayWin tray;
  StatusIcon* icon = CreateStatusIcon(&tray);
  std::unique_ptr<StatusIconMenuModel> menu(new StatusIconMenuModel(NULL));
  menu->AddItem(0, u"foo");
  icon->SetContextMenu(std::move(menu));
}

#if !defined(USE_AURA)  // http://crbug.com/156370
TEST(StatusTrayWinTest, ClickOnIcon) {
  // Create an icon, send a fake click event, make sure observer is called.
  StatusTrayWin tray;

  StatusIconWin* icon = CreateStatusIcon(&tray);
  FakeStatusIconObserver observer;
  icon->AddObserver(&observer);
  // Mimic a click.
  tray.WndProc(NULL, icon->message_id(), icon->icon_id(), WM_LBUTTONDOWN);
  // Mimic a right-click - observer should not be called.
  tray.WndProc(NULL, icon->message_id(), icon->icon_id(), WM_RBUTTONDOWN);
  EXPECT_EQ(1, observer.status_icon_click_count());
  icon->RemoveObserver(&observer);
}

TEST(StatusTrayWinTest, ClickOnBalloon) {
  // Create an icon, send a fake click event, make sure observer is called.
  StatusTrayWin tray;
  StatusIconWin* icon = CreateStatusIcon(&tray);
  FakeStatusIconObserver observer;
  icon->AddObserver(&observer);
  // Mimic a click.
  tray.WndProc(
      NULL, icon->message_id(), icon->icon_id(), TB_INDETERMINATE);
  EXPECT_TRUE(observer.balloon_clicked());
  icon->RemoveObserver(&observer);
}

TEST(StatusTrayWinTest, HandleOldIconId) {
  StatusTrayWin tray;
  StatusIconWin* icon = CreateStatusIcon(&tray);
  UINT message_id = icon->message_id();
  UINT icon_id = icon->icon_id();

  tray.RemoveStatusIcon(icon);
  tray.WndProc(NULL, message_id, icon_id, WM_LBUTTONDOWN);
}
#endif  // !defined(USE_AURA)

TEST(StatusTrayWinTest, EnsureVisibleTest) {
  StatusTrayWin tray;

  FakeStatusTrayStateChangerProxy* proxy =
      new FakeStatusTrayStateChangerProxy();
  tray.SetStatusTrayStateChangerProxyForTest(
      std::unique_ptr<StatusTrayStateChangerProxy>(proxy));

  StatusIconWin* icon = CreateStatusIcon(&tray);

  icon->ForceVisible();
  // |proxy| is owned by |tray|, and |tray| lives to the end of the scope,
  // so calling methods on |proxy| is safe.
  EXPECT_TRUE(proxy->enqueue_called());
  EXPECT_EQ(proxy->window(), icon->window());
  EXPECT_EQ(proxy->icon_id(), icon->icon_id());
}
