// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/profile_picker_test_base.h"

#include "base/callback.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/profile_picker.h"
#include "chrome/browser/ui/ui_features.h"
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

// Waits until a first non empty paint for given committed `url`.
class FirstVisuallyNonEmptyPaintObserver : public content::WebContentsObserver {
 public:
  explicit FirstVisuallyNonEmptyPaintObserver(content::WebContents* contents,
                                              const GURL& url)
      : content::WebContentsObserver(contents), url_(url) {}

  // Waits for the first paint.
  void Wait() {
    if (IsExitConditionSatisfied()) {
      return;
    }
    run_loop_.Run();
    EXPECT_TRUE(IsExitConditionSatisfied())
        << web_contents()->GetLastCommittedURL() << " != " << url_;
  }

 private:
  // WebContentsObserver:
  void DidFirstVisuallyNonEmptyPaint() override {
    if (IsExitConditionSatisfied())
      run_loop_.Quit();
  }

  void NavigationEntryCommitted(
      const content::LoadCommittedDetails& load_details) override {
    if (IsExitConditionSatisfied())
      run_loop_.Quit();
  }

  bool IsExitConditionSatisfied() {
    return (web_contents()->GetLastCommittedURL() == url_ &&
            web_contents()->CompletedFirstVisuallyNonEmptyPaint());
  }

  base::RunLoop run_loop_{base::RunLoop::Type::kNestableTasksAllowed};
  GURL url_;
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

ProfilePickerTestBase::ProfilePickerTestBase() {
  feature_list_.InitAndEnableFeature(features::kNewProfilePicker);
}

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

void ProfilePickerTestBase::WaitForFirstPaint(content::WebContents* contents,
                                              const GURL& url) {
  DCHECK(contents);
  FirstVisuallyNonEmptyPaintObserver(contents, url).Wait();
}

void ProfilePickerTestBase::WaitForPickerClosed() {
  if (!ProfilePicker::IsOpen())
    return;
  ViewDeletedWaiter(view()).Wait();
  ASSERT_FALSE(ProfilePicker::IsOpen());
}

content::WebContents* ProfilePickerTestBase::web_contents() {
  if (!web_view())
    return nullptr;
  return web_view()->GetWebContents();
}
