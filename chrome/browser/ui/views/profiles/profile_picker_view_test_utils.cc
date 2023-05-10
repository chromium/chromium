// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/profile_ui_test_utils.h"

#include "base/files/file_path.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/ui/profile_picker.h"
#include "chrome/browser/ui/views/profiles/profile_management_step_controller.h"
#include "chrome/browser/ui/views/profiles/profile_picker_view.h"
#include "chrome/browser/ui/views/profiles/profile_picker_view_test_utils.h"
#include "chrome/browser/ui/webui/signin/enterprise_profile_welcome_handler.h"
#include "chrome/browser/ui/webui/signin/enterprise_profile_welcome_ui.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/web_contents.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/view.h"

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/webui/signin/login_ui_service_factory.h"
#include "chrome/browser/ui/webui/signin/signin_url_utils.h"
#include "chrome/browser/ui/webui/signin/sync_confirmation_ui.h"
#endif

#if BUILDFLAG(IS_ANDROID)
#error This file should only be included on desktop.
#endif

namespace {

content::WebContents* GetPickerWebContents() {
  if (!ProfilePicker::GetWebViewForTesting())
    return nullptr;
  return ProfilePicker::GetWebViewForTesting()->GetWebContents();
}

class TestProfileManagementFlowController
    : public ProfileManagementFlowController,
      public content::WebContentsObserver {
 public:
  TestProfileManagementFlowController(
      ProfilePickerWebContentsHost* host,
      ClearHostClosure clear_host_callback,
      Step step,
      ProfileManagementStepTestView::StepControllerFactory factory,
      base::OnceClosure initial_step_load_finished_closure)
      : ProfileManagementFlowController(host, std::move(clear_host_callback)),
        step_(step),
        step_controller_factory_(std::move(factory)),
        initial_step_load_finished_closure_(
            std::move(initial_step_load_finished_closure)) {}

  void Init(StepSwitchFinishedCallback step_switch_finished_callback) override {
    RegisterStep(step_, step_controller_factory_.Run(host()));
    SwitchToStep(
        step_, /*reset_state=*/true,
        /*step_switch_finished_callback=*/
        base::BindOnce(
            &TestProfileManagementFlowController::OnInitialStepSwitchFinished,
            weak_ptr_factory_.GetWeakPtr(),
            std::move(step_switch_finished_callback)));
  }

  void OnInitialStepSwitchFinished(StepSwitchFinishedCallback original_callback,
                                   bool success) {
    if (original_callback) {
      std::move(original_callback).Run(success);
    }

    if (host()->GetPickerContents()->IsLoading()) {
      Observe(host()->GetPickerContents());
    } else {
      DCHECK(initial_step_load_finished_closure_);
      std::move(initial_step_load_finished_closure_).Run();
    }
  }

  void DidFirstVisuallyNonEmptyPaint() override {
    Observe(nullptr);
    DCHECK(initial_step_load_finished_closure_);
    std::move(initial_step_load_finished_closure_).Run();
  }

  void CancelPostSignInFlow() override { NOTREACHED_NORETURN(); }

  Step step_;
  ProfileManagementStepTestView::StepControllerFactory step_controller_factory_;
  base::OnceClosure initial_step_load_finished_closure_;
  base::WeakPtrFactory<TestProfileManagementFlowController> weak_ptr_factory_{
      this};
};

}  // namespace

// -- ViewAddedWaiter ----------------------------------------------------------

ViewAddedWaiter::ViewAddedWaiter(views::View* view) : view_(view) {}
ViewAddedWaiter::~ViewAddedWaiter() = default;

void ViewAddedWaiter::Wait() {
  if (view_->GetWidget())
    return;
  observation_.Observe(view_.get());
  run_loop_.Run();
}

void ViewAddedWaiter::OnViewAddedToWidget(views::View* observed_view) {
  if (observed_view == view_)
    run_loop_.Quit();
}

// -- ViewDeletedWaiter --------------------------------------------------------

ViewDeletedWaiter::ViewDeletedWaiter(views::View* view) {
  DCHECK(view);
  observation_.Observe(view);
}

ViewDeletedWaiter::~ViewDeletedWaiter() = default;

void ViewDeletedWaiter::Wait() {
  run_loop_.Run();
}

void ViewDeletedWaiter::OnViewIsDeleting(views::View* observed_view) {
  // Reset the observation before the view is actually deleted.
  observation_.Reset();
  run_loop_.Quit();
}

// -- ProfileManagementStepTestView --------------------------------------------

ProfileManagementStepTestView::ProfileManagementStepTestView(
    ProfilePicker::Params&& params,
    ProfileManagementFlowController::Step step,
    StepControllerFactory step_controller_factory)
    : ProfilePickerView(std::move(params)),
      step_(step),
      step_controller_factory_(std::move(step_controller_factory)) {}

ProfileManagementStepTestView::~ProfileManagementStepTestView() = default;

void ProfileManagementStepTestView::ShowAndWait(
    absl::optional<gfx::Size> view_size) {
  Display();

  // waits for the view to be shown to return. If we don't wait enough
  // and the test is flaky, try to poll the page to check the presence of some
  // UI elements to know when to stop waiting.
  run_loop_.Run();

  if (view_size.has_value())
    GetWidget()->SetSize(view_size.value());
}

std::unique_ptr<ProfileManagementFlowController>
ProfileManagementStepTestView::CreateFlowController(
    Profile* picker_profile,
    ClearHostClosure clear_host_callback) {
  return std::make_unique<TestProfileManagementFlowController>(
      this, std::move(clear_host_callback), step_, step_controller_factory_,
      run_loop_.QuitClosure());
}

// -- Other utils --------------------------------------------------------------
namespace profiles::testing {

void WaitForPickerWidgetCreated() {
  if (!ProfilePicker::IsOpen()) {
    base::RunLoop run_loop;
    ProfilePicker::AddOnProfilePickerOpenedCallbackForTesting(
        run_loop.QuitClosure());
    run_loop.Run();
  }
  ViewAddedWaiter(ProfilePicker::GetViewForTesting()).Wait();
}

void WaitForPickerLoadStop(const GURL& url) {
  content::WebContents* wc = GetPickerWebContents();
  if (wc && wc->GetLastCommittedURL() == url && !wc->IsLoading())
    return;

  ui_test_utils::UrlLoadObserver url_observer(
      url, content::NotificationService::AllSources());
  url_observer.Wait();

  // Update the pointer as the picker's WebContents could have changed in the
  // meantime.
  wc = GetPickerWebContents();
  EXPECT_EQ(wc->GetLastCommittedURL(), url);
}

void WaitForPickerClosed() {
  if (!ProfilePicker::IsOpen())
    return;
  ViewDeletedWaiter(ProfilePicker::GetViewForTesting()).Wait();
}

EnterpriseProfileWelcomeHandler* ExpectPickerWelcomeScreenType(
    EnterpriseProfileWelcomeUI::ScreenType expected_type) {
  content::WebContents* web_contents = GetPickerWebContents();
  EXPECT_TRUE(web_contents);
  EnterpriseProfileWelcomeHandler* handler =
      web_contents->GetWebUI()
          ->GetController()
          ->GetAs<EnterpriseProfileWelcomeUI>()
          ->GetHandlerForTesting();
  EXPECT_TRUE(handler);
  EXPECT_EQ(handler->GetTypeForTesting(), expected_type);
  return handler;
}

void ExpectPickerWelcomeScreenTypeAndProceed(
    EnterpriseProfileWelcomeUI::ScreenType expected_type,
    signin::SigninChoice choice) {
  EnterpriseProfileWelcomeHandler* handler =
      ExpectPickerWelcomeScreenType(expected_type);

  // Simulate clicking on the next button.
  handler->CallProceedCallbackForTesting(choice);
}

#if BUILDFLAG(IS_CHROMEOS_LACROS)
void CompleteLacrosFirstRun(
    LoginUIService::SyncConfirmationUIClosedResult result) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  Profile& profile = profiles::testing::CreateProfileSync(
      profile_manager, profile_manager->GetPrimaryUserProfilePath());

  WaitForPickerWidgetCreated();
  WaitForPickerLoadStop(GURL(chrome::kChromeUIIntroURL));

  ASSERT_TRUE(ProfilePicker::IsFirstRunOpen());
  EXPECT_EQ(0u, BrowserList::GetInstance()->size());

  base::Value::List args;
  GetPickerWebContents()->GetWebUI()->ProcessWebUIMessage(
      GetPickerWebContents()->GetURL(), "continueWithAccount", std::move(args));

  WaitForPickerLoadStop(AppendSyncConfirmationQueryParams(
      GURL("chrome://sync-confirmation/"), SyncConfirmationStyle::kWindow));

  if (result == LoginUIService::UI_CLOSED) {
    // `UI_CLOSED` is not provided via webui handlers. Instead, it gets sent
    // when the profile picker gets closed by some external source. If we only
    // send the result notification like for other types, the view will stay
    // open.
    ProfilePicker::Hide();
  } else {
    LoginUIServiceFactory::GetForProfile(&profile)->SyncConfirmationUIClosed(
        result);
  }
}
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

}  // namespace profiles::testing
