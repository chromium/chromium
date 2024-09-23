// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/web_modal/web_contents_modal_dialog_manager.h"

#include <map>
#include <memory>

#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "components/web_modal/single_web_contents_dialog_manager.h"
#include "components/web_modal/test_web_contents_modal_dialog_manager_delegate.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace web_modal {

class MockCloseOnNavigationObserver
    : public WebContentsModalDialogManager::CloseOnNavigationObserver {
 public:
  MockCloseOnNavigationObserver() = default;
  ~MockCloseOnNavigationObserver() override = default;

  MOCK_METHOD(void, OnWillClose, (), (override));
};

// Tracks persistent state changes of the native WC-modal dialog manager.
class NativeManagerTracker {
 public:
  enum DialogState {
    UNKNOWN,
    NOT_SHOWN,
    SHOWN,
    HIDDEN,
    CLOSED
  };

  NativeManagerTracker() = default;

  void SetState(DialogState state) {
    state_ = state;
    if (state_ == SHOWN) {
      ++shown_times_;
    }
  }

  bool was_shown() { return shown_times_ > 0; }

  DialogState state_ = DialogState::UNKNOWN;
  int shown_times_ = 0;
};

NativeManagerTracker unused_tracker;

class TestNativeWebContentsModalDialogManager
    : public SingleWebContentsDialogManager {
 public:
  TestNativeWebContentsModalDialogManager(
      gfx::NativeWindow dialog,
      SingleWebContentsDialogManagerDelegate* delegate,
      NativeManagerTracker* tracker)
      : delegate_(delegate),
        dialog_(dialog),
        tracker_(tracker) {
    if (tracker_)
      tracker_->SetState(NativeManagerTracker::NOT_SHOWN);
  }

  TestNativeWebContentsModalDialogManager(
      const TestNativeWebContentsModalDialogManager&) = delete;
  TestNativeWebContentsModalDialogManager& operator=(
      const TestNativeWebContentsModalDialogManager&) = delete;

  void Show() override {
    if (tracker_)
      tracker_->SetState(NativeManagerTracker::SHOWN);
  }
  void Hide() override {
    if (tracker_)
      tracker_->SetState(NativeManagerTracker::HIDDEN);
  }
  void Close() override {
    if (tracker_)
      tracker_->SetState(NativeManagerTracker::CLOSED);
    delegate_->WillClose(dialog_);
  }
  void Focus() override {}
  void Pulse() override {}
  void HostChanged(WebContentsModalDialogHost* new_host) override {}
  gfx::NativeWindow dialog() override { return dialog_; }
  bool IsActive() const override { return is_active_; }

  void StopTracking() { tracker_ = nullptr; }
  void SetIsActive(bool is_active) { is_active_ = is_active; }

 private:
  raw_ptr<SingleWebContentsDialogManagerDelegate> delegate_;
  gfx::NativeWindow dialog_;
  raw_ptr<NativeManagerTracker> tracker_;
  bool is_active_;
};

class WebContentsModalDialogManagerTest
    : public content::RenderViewHostTestHarness {
 public:
  WebContentsModalDialogManagerTest() : next_dialog_id(1), manager(nullptr) {}

  WebContentsModalDialogManagerTest(const WebContentsModalDialogManagerTest&) =
      delete;
  WebContentsModalDialogManagerTest& operator=(
      const WebContentsModalDialogManagerTest&) = delete;

  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();

    delegate = std::make_unique<TestWebContentsModalDialogManagerDelegate>();
    WebContentsModalDialogManager::CreateForWebContents(web_contents());
    manager = WebContentsModalDialogManager::FromWebContents(web_contents());
    manager->SetDelegate(delegate.get());
    test_api =
        std::make_unique<WebContentsModalDialogManager::TestApi>(manager);
  }

  void TearDown() override {
    manager = nullptr;
    test_api.reset();
    content::RenderViewHostTestHarness::TearDown();
  }

 protected:
  gfx::NativeWindow MakeFakeDialog() {
    // WebContentsModalDialogManager treats the dialog window as an opaque
    // type, so creating fake dialog windows using reinterpret_cast is valid.
#if BUILDFLAG(IS_APPLE)
    NSWindow* window = reinterpret_cast<NSWindow*>(next_dialog_id++);
    return gfx::NativeWindow(window);
#else
    return reinterpret_cast<gfx::NativeWindow>(next_dialog_id++);
#endif
  }

  int next_dialog_id;
  std::unique_ptr<TestWebContentsModalDialogManagerDelegate> delegate;
  raw_ptr<WebContentsModalDialogManager> manager;
  std::unique_ptr<WebContentsModalDialogManager::TestApi> test_api;
};

// Test that the dialog is shown immediately when the delegate indicates the web
// contents is visible.
TEST_F(WebContentsModalDialogManagerTest, WebContentsVisible) {
  // Dialog should be shown while WebContents is visible.
  const gfx::NativeWindow dialog = MakeFakeDialog();

  NativeManagerTracker tracker;
  TestNativeWebContentsModalDialogManager* native_manager =
      new TestNativeWebContentsModalDialogManager(dialog, manager, &tracker);
  manager->ShowDialogWithManager(dialog, base::WrapUnique(native_manager));

  EXPECT_EQ(NativeManagerTracker::SHOWN, tracker.state_);
  EXPECT_TRUE(manager->IsDialogActive());
  EXPECT_TRUE(delegate->web_contents_blocked());
  EXPECT_TRUE(tracker.was_shown());

  native_manager->StopTracking();
}

// Test that the dialog is not shown immediately when the delegate indicates the
// web contents is not visible.
TEST_F(WebContentsModalDialogManagerTest, WebContentsNotVisible) {
  // Dialog should not be shown while WebContents is not visible.
  delegate->set_web_contents_visible(false);

  const gfx::NativeWindow dialog = MakeFakeDialog();

  NativeManagerTracker tracker;
  TestNativeWebContentsModalDialogManager* native_manager =
      new TestNativeWebContentsModalDialogManager(dialog, manager, &tracker);
  manager->ShowDialogWithManager(dialog, base::WrapUnique(native_manager));

  EXPECT_EQ(NativeManagerTracker::NOT_SHOWN, tracker.state_);
  EXPECT_TRUE(manager->IsDialogActive());
  EXPECT_TRUE(delegate->web_contents_blocked());
  EXPECT_FALSE(tracker.was_shown());

  native_manager->StopTracking();
}

// Test that only the first of multiple dialogs is shown.
TEST_F(WebContentsModalDialogManagerTest, ShowDialogs) {
  const gfx::NativeWindow dialog1 = MakeFakeDialog();
  const gfx::NativeWindow dialog2 = MakeFakeDialog();
  const gfx::NativeWindow dialog3 = MakeFakeDialog();

  NativeManagerTracker tracker1;
  NativeManagerTracker tracker2;
  NativeManagerTracker tracker3;
  TestNativeWebContentsModalDialogManager* native_manager1 =
      new TestNativeWebContentsModalDialogManager(dialog1, manager, &tracker1);
  TestNativeWebContentsModalDialogManager* native_manager2 =
      new TestNativeWebContentsModalDialogManager(dialog2, manager, &tracker2);
  TestNativeWebContentsModalDialogManager* native_manager3 =
      new TestNativeWebContentsModalDialogManager(dialog3, manager, &tracker3);
  manager->ShowDialogWithManager(dialog1, base::WrapUnique(native_manager1));
  manager->ShowDialogWithManager(dialog2, base::WrapUnique(native_manager2));
  manager->ShowDialogWithManager(dialog3, base::WrapUnique(native_manager3));

  EXPECT_TRUE(delegate->web_contents_blocked());
  EXPECT_EQ(NativeManagerTracker::SHOWN, tracker1.state_);
  EXPECT_EQ(NativeManagerTracker::NOT_SHOWN, tracker2.state_);
  EXPECT_EQ(NativeManagerTracker::NOT_SHOWN, tracker3.state_);

  native_manager1->StopTracking();
  native_manager2->StopTracking();
  native_manager3->StopTracking();
}

// Test that the dialog is shown/hidden when the WebContents is shown/hidden.
TEST_F(WebContentsModalDialogManagerTest, VisibilityObservation) {
  const gfx::NativeWindow dialog = MakeFakeDialog();

  NativeManagerTracker tracker;
  TestNativeWebContentsModalDialogManager* native_manager =
      new TestNativeWebContentsModalDialogManager(dialog, manager, &tracker);
  manager->ShowDialogWithManager(dialog, base::WrapUnique(native_manager));

  EXPECT_TRUE(manager->IsDialogActive());
  EXPECT_TRUE(delegate->web_contents_blocked());
  EXPECT_EQ(NativeManagerTracker::SHOWN, tracker.state_);

  test_api->WebContentsVisibilityChanged(content::Visibility::HIDDEN);

  EXPECT_TRUE(manager->IsDialogActive());
  EXPECT_TRUE(delegate->web_contents_blocked());
  EXPECT_EQ(NativeManagerTracker::HIDDEN, tracker.state_);

  test_api->WebContentsVisibilityChanged(content::Visibility::VISIBLE);

  EXPECT_TRUE(manager->IsDialogActive());
  EXPECT_TRUE(delegate->web_contents_blocked());
  EXPECT_EQ(NativeManagerTracker::SHOWN, tracker.state_);

  native_manager->StopTracking();
}

// Tests that the dialog shows when switching from occluded to visible.
TEST_F(WebContentsModalDialogManagerTest, OccludedToVisible) {
  const gfx::NativeWindow dialog = MakeFakeDialog();

  delegate->set_web_contents_visible(false);
  test_api->WebContentsVisibilityChanged(content::Visibility::OCCLUDED);

  NativeManagerTracker tracker;
  TestNativeWebContentsModalDialogManager* native_manager =
      new TestNativeWebContentsModalDialogManager(dialog, manager, &tracker);
  native_manager->SetIsActive(false);

  manager->ShowDialogWithManager(dialog, base::WrapUnique(native_manager));

  EXPECT_TRUE(manager->IsDialogActive());
  EXPECT_TRUE(delegate->web_contents_blocked());
  EXPECT_EQ(NativeManagerTracker::NOT_SHOWN, tracker.state_);

  test_api->WebContentsVisibilityChanged(content::Visibility::VISIBLE);

  EXPECT_TRUE(manager->IsDialogActive());
  EXPECT_TRUE(delegate->web_contents_blocked());
  EXPECT_EQ(NativeManagerTracker::SHOWN, tracker.state_);

  native_manager->StopTracking();
}

// Tests that the dialog is not shown when switching from visible to occluded.
// Regression test for crbug.com/350745485.
TEST_F(WebContentsModalDialogManagerTest, VisibleToOccluded) {
  const gfx::NativeWindow dialog = MakeFakeDialog();

  delegate->set_web_contents_visible(true);
  test_api->WebContentsVisibilityChanged(content::Visibility::VISIBLE);

  NativeManagerTracker tracker;
  TestNativeWebContentsModalDialogManager* native_manager =
      new TestNativeWebContentsModalDialogManager(dialog, manager, &tracker);

  manager->ShowDialogWithManager(dialog, base::WrapUnique(native_manager));

  EXPECT_TRUE(manager->IsDialogActive());
  EXPECT_TRUE(delegate->web_contents_blocked());
  EXPECT_EQ(NativeManagerTracker::SHOWN, tracker.state_);
  EXPECT_EQ(tracker.shown_times_, 1);

  // Simulate e.g. the user clicking on another window.
  native_manager->SetIsActive(false);
  test_api->WebContentsVisibilityChanged(content::Visibility::OCCLUDED);

  // The dialog should still be shown, but Show() should not have been called
  // again.
  EXPECT_TRUE(manager->IsDialogActive());
  EXPECT_TRUE(delegate->web_contents_blocked());
  EXPECT_EQ(NativeManagerTracker::SHOWN, tracker.state_);
  EXPECT_EQ(tracker.shown_times_, 1);

  native_manager->StopTracking();
}

// Test that the first dialog is always shown, regardless of the order in which
// dialogs are closed.
TEST_F(WebContentsModalDialogManagerTest, CloseDialogs) {
  // The front dialog is always shown regardless of dialog close order.
  const gfx::NativeWindow dialog1 = MakeFakeDialog();
  const gfx::NativeWindow dialog2 = MakeFakeDialog();
  const gfx::NativeWindow dialog3 = MakeFakeDialog();
  const gfx::NativeWindow dialog4 = MakeFakeDialog();

  NativeManagerTracker tracker1;
  NativeManagerTracker tracker2;
  NativeManagerTracker tracker3;
  NativeManagerTracker tracker4;
  TestNativeWebContentsModalDialogManager* native_manager1 =
      new TestNativeWebContentsModalDialogManager(dialog1, manager, &tracker1);
  TestNativeWebContentsModalDialogManager* native_manager2 =
      new TestNativeWebContentsModalDialogManager(dialog2, manager, &tracker2);
  TestNativeWebContentsModalDialogManager* native_manager3 =
      new TestNativeWebContentsModalDialogManager(dialog3, manager, &tracker3);
  TestNativeWebContentsModalDialogManager* native_manager4 =
      new TestNativeWebContentsModalDialogManager(dialog4, manager, &tracker4);
  manager->ShowDialogWithManager(dialog1, base::WrapUnique(native_manager1));
  manager->ShowDialogWithManager(dialog2, base::WrapUnique(native_manager2));
  manager->ShowDialogWithManager(dialog3, base::WrapUnique(native_manager3));
  manager->ShowDialogWithManager(dialog4, base::WrapUnique(native_manager4));

  native_manager1->Close();

  EXPECT_TRUE(manager->IsDialogActive());
  EXPECT_TRUE(delegate->web_contents_blocked());
  EXPECT_EQ(NativeManagerTracker::CLOSED, tracker1.state_);
  EXPECT_EQ(NativeManagerTracker::SHOWN, tracker2.state_);
  EXPECT_EQ(NativeManagerTracker::NOT_SHOWN, tracker3.state_);
  EXPECT_EQ(NativeManagerTracker::NOT_SHOWN, tracker4.state_);

  native_manager3->Close();

  EXPECT_TRUE(manager->IsDialogActive());
  EXPECT_TRUE(delegate->web_contents_blocked());
  EXPECT_EQ(NativeManagerTracker::CLOSED, tracker1.state_);
  EXPECT_EQ(NativeManagerTracker::SHOWN, tracker2.state_);
  EXPECT_EQ(NativeManagerTracker::CLOSED, tracker3.state_);
  EXPECT_EQ(NativeManagerTracker::NOT_SHOWN, tracker4.state_);
  EXPECT_FALSE(tracker3.was_shown());

  native_manager2->Close();

  EXPECT_TRUE(manager->IsDialogActive());
  EXPECT_TRUE(delegate->web_contents_blocked());
  EXPECT_EQ(NativeManagerTracker::CLOSED, tracker1.state_);
  EXPECT_EQ(NativeManagerTracker::CLOSED, tracker2.state_);
  EXPECT_EQ(NativeManagerTracker::CLOSED, tracker3.state_);
  EXPECT_EQ(NativeManagerTracker::SHOWN, tracker4.state_);
  EXPECT_FALSE(tracker3.was_shown());

  native_manager4->Close();

  EXPECT_FALSE(manager->IsDialogActive());
  EXPECT_FALSE(delegate->web_contents_blocked());
  EXPECT_EQ(NativeManagerTracker::CLOSED, tracker1.state_);
  EXPECT_EQ(NativeManagerTracker::CLOSED, tracker2.state_);
  EXPECT_EQ(NativeManagerTracker::CLOSED, tracker3.state_);
  EXPECT_EQ(NativeManagerTracker::CLOSED, tracker4.state_);
  EXPECT_TRUE(tracker1.was_shown());
  EXPECT_TRUE(tracker2.was_shown());
  EXPECT_FALSE(tracker3.was_shown());
  EXPECT_TRUE(tracker4.was_shown());
}

// Test that CloseAllDialogs does what it says.
TEST_F(WebContentsModalDialogManagerTest, CloseAllDialogs) {
  const int kWindowCount = 4;
  NativeManagerTracker trackers[kWindowCount];
  TestNativeWebContentsModalDialogManager* native_managers[kWindowCount];
  for (int i = 0; i < kWindowCount; i++) {
    const gfx::NativeWindow dialog = MakeFakeDialog();
    native_managers[i] =
        new TestNativeWebContentsModalDialogManager(
            dialog, manager, &(trackers[i]));
    manager->ShowDialogWithManager(dialog,
                                   base::WrapUnique(native_managers[i]));
  }

  for (int i = 0; i < kWindowCount; i++)
    EXPECT_NE(NativeManagerTracker::CLOSED, trackers[i].state_);

  test_api->CloseAllDialogs();

  EXPECT_FALSE(delegate->web_contents_blocked());
  EXPECT_FALSE(manager->IsDialogActive());
  for (int i = 0; i < kWindowCount; i++)
    EXPECT_EQ(NativeManagerTracker::CLOSED, trackers[i].state_);
}

// Test that dialogs are closed on WebContents navigation.
TEST_F(WebContentsModalDialogManagerTest, CloseOnNavigation) {
  MockCloseOnNavigationObserver observer;
  EXPECT_CALL(observer, OnWillClose());

  const gfx::NativeWindow dialog = MakeFakeDialog();
  NativeManagerTracker tracker;
  TestNativeWebContentsModalDialogManager* native_manager =
      new TestNativeWebContentsModalDialogManager(dialog, manager, &tracker);
  manager->ShowDialogWithManager(dialog, base::WrapUnique(native_manager));

  manager->AddCloseOnNavigationObserver(&observer);

  NavigateAndCommit(GURL("https://example.com/"));
  EXPECT_EQ(NativeManagerTracker::CLOSED, tracker.state_);
}

// Test that the CloseOnNavigation observer is not triggered if the dialog is
// closed for another reason.
TEST_F(WebContentsModalDialogManagerTest,
       ObserverNotNotifiedOfNonNavigationClose) {
  MockCloseOnNavigationObserver observer;
  EXPECT_CALL(observer, OnWillClose()).Times(0);

  const gfx::NativeWindow dialog = MakeFakeDialog();
  NativeManagerTracker tracker;
  TestNativeWebContentsModalDialogManager* native_manager =
      new TestNativeWebContentsModalDialogManager(dialog, manager, &tracker);
  manager->ShowDialogWithManager(dialog, base::WrapUnique(native_manager));

  manager->AddCloseOnNavigationObserver(&observer);

  native_manager->Close();
  EXPECT_EQ(NativeManagerTracker::CLOSED, tracker.state_);
}

}  // namespace web_modal
