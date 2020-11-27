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

// Waits until a web view is added as a child view of the given view.
class WebViewAddedWaiter : public views::ViewObserver {
 public:
  WebViewAddedWaiter(
      views::View* top_view,
      base::RepeatingCallback<views::WebView*()> current_web_view_getter)
      : current_web_view_getter_(current_web_view_getter) {
    observation_.Observe(top_view);
  }
  ~WebViewAddedWaiter() override = default;

  void Wait() { run_loop_.Run(); }

 private:
  // ViewObserver:
  void OnChildViewAdded(views::View* observed_view,
                        views::View* child) override {
    if (child == current_web_view_getter_.Run()) {
      ASSERT_TRUE(child);
      run_loop_.Quit();
    }
  }

  base::RunLoop run_loop_;
  base::RepeatingCallback<views::WebView*()> current_web_view_getter_;
  base::ScopedObservation<views::View, views::ViewObserver> observation_{this};
};

// Waits until a first non empty paint for given `url`.
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
        << web_contents()->GetVisibleURL() << " != " << url_;
  }

 private:
  // WebContentsObserver:
  void DidFirstVisuallyNonEmptyPaint() override {
    if (web_contents()->GetVisibleURL() == url_)
      run_loop_.Quit();
  }

  bool IsExitConditionSatisfied() {
    return (web_contents()->GetVisibleURL() == url_ &&
            web_contents()->CompletedFirstVisuallyNonEmptyPaint());
  }

  base::RunLoop run_loop_{base::RunLoop::Type::kNestableTasksAllowed};
  GURL url_;
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

void ProfilePickerTestBase::WaitForNewWebView() {
  ASSERT_TRUE(view());
  WebViewAddedWaiter(view(),
                     base::BindRepeating(&ProfilePickerTestBase::web_view,
                                         base::Unretained(this)))
      .Wait();
  EXPECT_TRUE(web_view());
}

void ProfilePickerTestBase::WaitForFirstPaint(content::WebContents* contents,
                                              const GURL& url) {
  DCHECK(contents);
  FirstVisuallyNonEmptyPaintObserver(contents, url).Wait();
}

content::WebContents* ProfilePickerTestBase::web_contents() {
  if (!web_view())
    return nullptr;
  return web_view()->GetWebContents();
}
