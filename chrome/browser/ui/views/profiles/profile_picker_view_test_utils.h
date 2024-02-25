// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_PICKER_VIEW_TEST_UTILS_H_
#define CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_PICKER_VIEW_TEST_UTILS_H_

#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/profiles/profile_picker.h"
#include "chrome/browser/ui/views/profiles/profile_management_flow_controller.h"
#include "chrome/browser/ui/views/profiles/profile_picker_view.h"
#include "chrome/browser/ui/views/profiles/profile_picker_web_contents_host.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/views/view_observer.h"

class Profile;
class ProfileManagementStepController;

namespace views {
class View;
}

// Waits until a view gets attached to its widget.
class ViewAddedWaiter : public views::ViewObserver {
 public:
  explicit ViewAddedWaiter(views::View* view);
  ~ViewAddedWaiter() override;

  void Wait();

 private:
  // ViewObserver:
  void OnViewAddedToWidget(views::View* observed_view) override;

  base::RunLoop run_loop_;
  const raw_ptr<views::View> view_;
  base::ScopedObservation<views::View, views::ViewObserver> observation_{this};
};

// Waits until a view is deleted.
class ViewDeletedWaiter : public ::views::ViewObserver {
 public:
  explicit ViewDeletedWaiter(views::View* view);
  ~ViewDeletedWaiter() override;

  // Waits until the view is deleted.
  void Wait();

 private:
  // ViewObserver:
  void OnViewIsDeleting(views::View* observed_view) override;

  base::RunLoop run_loop_;
  base::ScopedObservation<views::View, views::ViewObserver> observation_{this};
};

// Waits until `expected_url` is loaded in `web_view`. Supports the case when
// the page load happens in a different `content::WebContents` than the one
// displayed at the moment we start waiting.
class PickerLoadStopWaiter : public content::WebContentsObserver {
 public:
  enum class Mode {
    kWaitUntilUrlLoaded,
    kCheckUrlAtNextLoad,
  };

  PickerLoadStopWaiter(views::WebView* web_view,
                       const GURL& expected_url,
                       Mode wait_mode = Mode::kWaitUntilUrlLoaded);

  // content::WebContentsObserver:
  void DidStopLoading() override;

  // Waits until the navigation is completed.
  void Wait();

 private:
  void OnWebContentsAttached(views::WebView* web_view);
  bool ShouldKeepWaiting() const;

  const raw_ref<views::WebView> web_view_;
  const GURL expected_url_;
  const Mode wait_mode_;

  base::RunLoop run_loop_;
  base::CallbackListSubscription web_contents_attached_subscription_;
};

// View intended to be used in pixel tests to render a specific step in the
// Profile Picker view.
// Unlike the classic `ProfilePickerView`, this one does not interact with the
// global state nor try to reuse previously existing views. It does not
// guarantee that some unsupported state can't be reached, so user discretion
// is advised.
class ProfileManagementStepTestView : public ProfilePickerView {
 public:
  using StepControllerFactory =
      base::RepeatingCallback<std::unique_ptr<ProfileManagementStepController>(
          ProfilePickerWebContentsHost* host)>;

  explicit ProfileManagementStepTestView(
      ProfilePicker::Params&& params,
      ProfileManagementFlowController::Step step,
      StepControllerFactory step_controller_factory);
  ~ProfileManagementStepTestView() override;

  // Returns when the content of the step reached the first non-empty paint.
  void ShowAndWait(std::optional<gfx::Size> view_size = std::nullopt);

 protected:
  std::unique_ptr<ProfileManagementFlowController> CreateFlowController(
      Profile* picker_profile,
      ClearHostClosure clear_host_callback) override;

 private:
  const ProfileManagementFlowController::Step step_;
  StepControllerFactory step_controller_factory_;

  base::RunLoop run_loop_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_PICKER_VIEW_TEST_UTILS_H_
