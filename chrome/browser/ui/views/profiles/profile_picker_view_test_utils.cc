// Copyright 2022 The Chromium Authors. All rights reserved.
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
#include "chrome/browser/ui/views/profiles/profile_picker_view.h"
#include "chrome/browser/ui/webui/signin/enterprise_profile_welcome_handler.h"
#include "chrome/browser/ui/webui/signin/enterprise_profile_welcome_ui.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/web_contents.h"
#include "ui/views/controls/webview/webview.h"

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

namespace profiles::testing {

namespace {

// Waits until a view gets attached to its widget.
class WidgetAttachedWaiter : public views::ViewObserver {
 public:
  explicit WidgetAttachedWaiter(views::View* view) : view_(view) {}
  ~WidgetAttachedWaiter() override = default;

  void Wait() {
    if (view_->GetWidget())
      return;
    observation_.Observe(view_.get());
    run_loop_.Run();
  }

 private:
  // ViewObserver:
  void OnViewAddedToWidget(views::View* observed_view) override {
    if (observed_view == view_)
      run_loop_.Quit();
  }

  base::RunLoop run_loop_;
  const raw_ptr<views::View> view_;
  base::ScopedObservation<views::View, views::ViewObserver> observation_{this};
};

// Waits until a view is deleted.
class ViewDeletedWaiter : public ::views::ViewObserver {
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

content::WebContents* GetPickerWebContents() {
  if (!ProfilePicker::GetWebViewForTesting())
    return nullptr;
  return ProfilePicker::GetWebViewForTesting()->GetWebContents();
}

}  // namespace

void WaitForPickerWidgetCreated() {
  WidgetAttachedWaiter(ProfilePicker::GetViewForTesting()).Wait();
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
  Profile* profile = profiles::testing::CreateProfileSync(
      profile_manager, profile_manager->GetPrimaryUserProfilePath());

  WaitForPickerWidgetCreated();
  WaitForPickerLoadStop(GURL("chrome://enterprise-profile-welcome/"));

  ASSERT_TRUE(ProfilePicker::IsLacrosFirstRunOpen());
  EXPECT_EQ(0u, BrowserList::GetInstance()->size());

  EnterpriseProfileWelcomeHandler* handler = ExpectPickerWelcomeScreenType(
      EnterpriseProfileWelcomeUI::ScreenType::kLacrosConsumerWelcome);
  handler->HandleProceedForTesting(/*should_link_data=*/false);
  WaitForPickerLoadStop(AppendSyncConfirmationQueryParams(
      GURL("chrome://sync-confirmation/"), SyncConfirmationStyle::kWindow));

  if (result == LoginUIService::UI_CLOSED) {
    // `UI_CLOSED` is not provided via webui handlers. Instead, it gets sent
    // when the profile picker gets closed by some external source. If we only
    // send the result notification like for other types, the view will stay
    // open.
    ProfilePicker::Hide();
  } else {
    LoginUIServiceFactory::GetForProfile(profile)->SyncConfirmationUIClosed(
        result);
  }
}
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

}  // namespace profiles::testing
