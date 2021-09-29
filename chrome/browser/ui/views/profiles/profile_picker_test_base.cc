// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/profile_picker_test_base.h"

#include "base/callback.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/profile_picker.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/view.h"
#include "ui/views/view_observer.h"
#include "url/gurl.h"

namespace {

// Waits until a view's visibility has the expected value.
class ViewVisibilityChangedWaiter : public views::ViewObserver {
 public:
  ViewVisibilityChangedWaiter(views::View* view, bool expect_toolbar_visible)
      : view_(view), expect_toolbar_visible_(expect_toolbar_visible) {}
  ~ViewVisibilityChangedWaiter() override = default;

  void Wait() {
    if (view_->GetVisible() == expect_toolbar_visible_)
      return;
    observation_.Observe(view_);
    run_loop_.Run();
  }

 private:
  // ViewObserver:
  void OnViewVisibilityChanged(views::View* observed_view,
                               views::View* starting_view) override {
    if (observed_view == starting_view &&
        starting_view->GetVisible() == expect_toolbar_visible_) {
      run_loop_.Quit();
    }
  }

  base::RunLoop run_loop_;
  views::View* const view_;
  bool expect_toolbar_visible_;
  base::ScopedObservation<views::View, views::ViewObserver> observation_{this};
};

// Waits until a view is deleted.
class ViewDeletedWaiter : public views::ViewObserver {
 public:
  explicit ViewDeletedWaiter(views::View* view) {
    DCHECK(view);
    observation_.Observe(view);
  }
  ~ViewDeletedWaiter() override = default;

  // Waits until the view is deleted.
  void Wait() { run_loop_.Run(); }

 private:
  // ViewObserver:
  void OnViewIsDeleting(views::View* observed_view) override {
    // Reset the observation before the view is actually deleted.
    observation_.Reset();
    run_loop_.Quit();
  }

  base::RunLoop run_loop_;
  base::ScopedObservation<views::View, views::ViewObserver> observation_{this};
};

}  // namespace

ProfilePickerTestBase::ProfilePickerTestBase() = default;

ProfilePickerTestBase::~ProfilePickerTestBase() = default;

views::View* ProfilePickerTestBase::view() {
  return ProfilePicker::GetViewForTesting();
}

views::Widget* ProfilePickerTestBase::widget() {
  return view() ? view()->GetWidget() : nullptr;
}

views::WebView* ProfilePickerTestBase::web_view() {
  return ProfilePicker::GetWebViewForTesting();
}

void ProfilePickerTestBase::WaitForLayoutWithToolbar() {
  ViewVisibilityChangedWaiter(ProfilePicker::GetToolbarForTesting(),
                              /*expect_toolbar_visible=*/true)
      .Wait();
}

void ProfilePickerTestBase::WaitForLayoutWithoutToolbar() {
  ViewVisibilityChangedWaiter(ProfilePicker::GetToolbarForTesting(),
                              /*expect_toolbar_visible=*/false)
      .Wait();
}

void ProfilePickerTestBase::WaitForLoadStop(content::WebContents* contents,
                                            const GURL& url) {
  DCHECK(contents);
  if (contents->GetLastCommittedURL() == url && !contents->IsLoading())
    return;

  ui_test_utils::UrlLoadObserver url_observer(
      url, content::NotificationService::AllSources());
  url_observer.Wait();
  EXPECT_EQ(contents->GetLastCommittedURL(), url);
}

void ProfilePickerTestBase::WaitForPickerClosed() {
  if (!ProfilePicker::IsOpen())
    return;
  ViewDeletedWaiter(view()).Wait();
  ASSERT_FALSE(ProfilePicker::IsOpen());
}

void ProfilePickerTestBase::WaitForPickerClosedAndReopenedImmediately() {
  ASSERT_TRUE(ProfilePicker::IsOpen());
  ViewDeletedWaiter(view()).Wait();
  EXPECT_TRUE(ProfilePicker::IsOpen());
}

content::WebContents* ProfilePickerTestBase::web_contents() {
  if (!web_view())
    return nullptr;
  return web_view()->GetWebContents();
}
