// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/bind.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/autocomplete/aim_eligibility_service_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/omnibox/omnibox_next_features.h"
#include "chrome/browser/ui/omnibox/omnibox_tab_helper.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_aim_popup_webui_content.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_aim_presenter.h"
#include "chrome/browser/ui/views/omnibox/omnibox_view_views.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/omnibox/browser/aim_eligibility_service.h"
#include "components/omnibox/browser/mock_aim_eligibility_service.h"
#include "components/permissions/permission_request_manager.h"
#include "components/permissions/test/mock_permission_request.h"
#include "components/permissions/test/permission_request_observer.h"
#include "components/variations/service/variations_service.h"
#include "content/public/browser/file_select_listener.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/mojom/choosers/file_chooser.mojom.h"
#include "ui/shell_dialogs/fake_select_file_dialog.h"
#include "url/gurl.h"

namespace {

std::unique_ptr<KeyedService> BuildMockAimEligibilityService(
    content::BrowserContext* context) {
  auto service = std::make_unique<testing::NiceMock<MockAimEligibilityService>>(
      *Profile::FromBrowserContext(context)->GetPrefs(),
      /*template_url_service=*/nullptr,
      /*url_loader_factory=*/nullptr, /*identity_manager=*/nullptr,
      AimEligibilityService::Configuration{});
  ON_CALL(*service, IsAimEligible()).WillByDefault(testing::Return(true));
  ON_CALL(*service, GetLocaleImpl()).WillByDefault(testing::Return("en-US"));
  return service;
}

class TestFileSelectListener : public content::FileSelectListener {
 public:
  TestFileSelectListener() = default;

  bool file_selected() const { return file_selected_; }
  bool canceled() const { return canceled_; }

  void FileSelected(std::vector<blink::mojom::FileChooserFileInfoPtr> files,
                    const base::FilePath& base_dir,
                    blink::mojom::FileChooserParams::Mode mode) override {
    file_selected_ = true;
  }

  void FileSelectionCanceled() override { canceled_ = true; }

 protected:
  ~TestFileSelectListener() override = default;

 private:
  bool file_selected_ = false;
  bool canceled_ = false;
};

}  // namespace

class OmniboxAimPopupBrowserTest : public InProcessBrowserTest {
 public:
  OmniboxAimPopupBrowserTest() {
    feature_list_.InitWithFeatures({omnibox::internal::kWebUIOmniboxAimPopup,
                                    omnibox::internal::kWebUIOmniboxPopup},
                                   {});
  }

  void TriggerMenuClosed(OmniboxPopupWebUIBaseContent* content) {
    content->OnMenuClosed();
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    g_browser_process->variations_service()->OverrideStoredPermanentCountry(
        "us");
  }

  void TearDownOnMainThread() override {
    ui::SelectFileDialog::SetFactory(nullptr);
    InProcessBrowserTest::TearDownOnMainThread();
  }

  void SetUpInProcessBrowserTestFixture() override {
    create_services_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(base::BindRepeating(
                &OmniboxAimPopupBrowserTest::OnWillCreateBrowserContextServices,
                base::Unretained(this)));
  }

  void OnWillCreateBrowserContextServices(content::BrowserContext* context) {
    AimEligibilityServiceFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating(&BuildMockAimEligibilityService));
  }

 protected:
  LocationBarView* location_bar() {
    return BrowserView::GetBrowserViewForBrowser(browser())
        ->toolbar()
        ->location_bar_view();
  }

  OmniboxAimPopupWebUIContent* ShowPopupAndGetWebUIContent() {
    location_bar()
        ->GetOmniboxController()
        ->popup_state_manager()
        ->SetPopupState(OmniboxPopupState::kAim);
    auto* presenter = location_bar()->GetOmniboxPopupAimPresenter();
    EXPECT_TRUE(presenter);
    if (!presenter) {
      return nullptr;
    }
    presenter->Show();
    auto* content =
        static_cast<OmniboxAimPopupWebUIContent*>(presenter->GetWebUIContent());
    EXPECT_TRUE(content);
    return content;
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  base::CallbackListSubscription create_services_subscription_;
};

IN_PROC_BROWSER_TEST_F(OmniboxAimPopupBrowserTest, ClearEventuallyDetaches) {
  auto* content = ShowPopupAndGetWebUIContent();
  ASSERT_TRUE(content);

  // Wait for the WebContents to be ready so the handler can be created.
  content::WaitForLoadStop(content->GetWebContents());

  base::test::TestFuture<void> future;
  auto subscription =
      content->AddWebContentsDetachedCallback(base::BindLambdaForTesting(
          [&](views::WebView* view) { future.SetValue(); }));

  // Trigger `OmniboxPopupAimPresenter::Hide()` by setting state to `kNone`.
  // This ensures `in_popup_state_transition_` is handled correctly by
  // `LocationBarView` and the state matches widget visibility.
  location_bar()->GetOmniboxController()->popup_state_manager()->SetPopupState(
      OmniboxPopupState::kNone);

  // This should eventually call `OmniboxPopupWebUIBaseContent::Detach`,
  // either immediately if there's no handler, or after the Mojo callback if
  // there is.
  EXPECT_TRUE(future.Wait());
}

IN_PROC_BROWSER_TEST_F(OmniboxAimPopupBrowserTest,
                       DraftTextPreservedOnTabSwitch) {
  // Ensure we are in AIM state so presenter is created.
  auto* content = ShowPopupAndGetWebUIContent();
  ASSERT_TRUE(content);

  // Get original tab. Ensure we capture the weak pointer before adding a
  // new tab.
  content::WebContents* original_tab = location_bar()->GetWebContents();
  base::WeakPtr<content::WebContents> original_tab_ptr =
      original_tab->GetWeakPtr();

  // Ensure original tab has an OmniboxState (normally created on blur).
  static_cast<OmniboxViewViews*>(location_bar()->GetOmniboxView())
      ->SaveStateToTab(original_tab);
  ASSERT_NE(nullptr, original_tab->GetUserData(OmniboxTabHelper::kOmniboxStateKey));

  // 2. Simulate opening a new tab (Ctrl+T scenario).
  // We don't actually need to press Ctrl+T, just adding a new tab to the
  // browser is enough to shift focus.
  chrome::AddTabAt(browser(), GURL("about:blank"), -1, true);
  ASSERT_NE(original_tab, location_bar()->GetWebContents());

  // 3. Simulate the Mojo callback returning with draft text.
  // We manually call the private `OnClearCallback`.
  const std::string draft_text = "typed in composebox";
  content->OnClearCallback(original_tab_ptr, draft_text);

  // 4. Verify the text was injected into the ORIGINAL tab's state.
  auto* state = static_cast<OmniboxState*>(
      original_tab->GetUserData(OmniboxTabHelper::kOmniboxStateKey));
  ASSERT_TRUE(state);
  EXPECT_EQ(base::UTF8ToUTF16(draft_text), state->model_state.user_text);
  EXPECT_TRUE(state->model_state.user_input_in_progress);

  // 5. Verify the NEW tab's Omnibox DOES NOT contain the draft text.
  EXPECT_NE(base::UTF8ToUTF16(draft_text),
            location_bar()->GetOmniboxView()->GetText());
}

IN_PROC_BROWSER_TEST_F(OmniboxAimPopupBrowserTest,
                       SynthesizesMouseLeaveEventOnMenuClosed) {
  auto* content = ShowPopupAndGetWebUIContent();
  ASSERT_TRUE(content);

  auto* rwh = content->GetWebContents()->GetPrimaryMainFrame()->GetRenderWidgetHost();
  ASSERT_TRUE(rwh);

  // 2. Set up the mouse event monitor to spy on the RenderWidgetHost.
  content::RenderWidgetHostMouseEventMonitor monitor(rwh);

  // 3. Trigger menu close.
  TriggerMenuClosed(content);

  // 4. Verify the kMouseLeave event was successfully forwarded to the host.
  EXPECT_TRUE(monitor.EventWasReceived());
  EXPECT_EQ(monitor.event().GetType(), blink::WebInputEvent::Type::kMouseLeave);
}

IN_PROC_BROWSER_TEST_F(OmniboxAimPopupBrowserTest,
                       PermissionPromptRemovalPreventsPopupClose) {
  ASSERT_TRUE(ShowPopupAndGetWebUIContent());
  auto* presenter = location_bar()->GetOmniboxPopupAimPresenter();
  ASSERT_TRUE(presenter);

  EXPECT_EQ(location_bar()
                ->GetOmniboxController()
                ->popup_state_manager()
                ->popup_state(),
            OmniboxPopupState::kAim);

  // Wait for the popup state transition delayed task in
  // `location_bar_view` (100ms) to finish.
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return !location_bar()->in_popup_state_transition(); }));

  // Simulate `OnPromptRemoved` notification (as if permission prompt was
  // dismissed).
  static_cast<permissions::PermissionRequestManager::Observer*>(presenter)
      ->OnPromptRemoved();

  // Simulate loss of widget activation while in permission prompt transition.
  static_cast<views::WidgetObserver*>(presenter)->OnWidgetActivationChanged(
      nullptr, /*active=*/false);

  // Verify popup state is still kAim (omnibox did not close due to activation
  // change) due to the protective flag preventing omnibox from closing until it
  // receives focus as the permission prompt asked.
  EXPECT_EQ(location_bar()
                ->GetOmniboxController()
                ->popup_state_manager()
                ->popup_state(),
            OmniboxPopupState::kAim);

  // Simulate widget activation returning to the omnibox popup (`active` is
  // true). The protective flag preventing omnibox from closing is now removed.
  static_cast<views::WidgetObserver*>(presenter)->OnWidgetActivationChanged(
      nullptr, /*active=*/true);

  // Now simulate subsequent widget activation loss after focus transition
  // complete (omnibox is back in focus, but user clicks outside of omnibox).
  static_cast<views::WidgetObserver*>(presenter)->OnWidgetActivationChanged(
      nullptr, /*active=*/false);

  // Verify popup state is now kNone (omnibox closed) due to no more protective
  // flag.
  EXPECT_EQ(location_bar()
                ->GetOmniboxController()
                ->popup_state_manager()
                ->popup_state(),
            OmniboxPopupState::kNone);
}

// Verify that active permission prompt taking focus prevents omnibox
// popup close.
IN_PROC_BROWSER_TEST_F(OmniboxAimPopupBrowserTest,
                       ActivePermissionPromptPreventsPopupClose) {
  auto* content = ShowPopupAndGetWebUIContent();
  ASSERT_TRUE(content);
  auto* presenter = location_bar()->GetOmniboxPopupAimPresenter();
  ASSERT_TRUE(presenter);

  auto* permission_manager =
      permissions::PermissionRequestManager::FromWebContents(
          content->GetWebContents());
  ASSERT_TRUE(permission_manager);

  // Add a mock permission request for a standard origin to set
  // IsRequestInProgress() to true without auto-approving. This avoids needing
  // a page navigation while allowing for a website to request a permission.
  // Cannot be about:blank due to DCheck.
  permissions::PermissionRequestObserver observer(content->GetWebContents());
  auto request = std::make_unique<permissions::MockPermissionRequest>(
      GURL("https://example.com"), permissions::RequestType::kMicStream);
  permission_manager->AddRequest(
      content->GetWebContents()->GetPrimaryMainFrame(), std::move(request));
  observer.Wait();

  EXPECT_TRUE(permission_manager->IsRequestInProgress());

  // Simulate widget activation loss while permission request is in progress.
  static_cast<views::WidgetObserver*>(presenter)->OnWidgetActivationChanged(
      nullptr, /*active=*/false);

  // Verify popup state is STILL kAim (omnibox did not close).
  EXPECT_EQ(location_bar()
                ->GetOmniboxController()
                ->popup_state_manager()
                ->popup_state(),
            OmniboxPopupState::kAim);
}

// Verify that active deactivation blockers prevent the omnibox popup from
// closing, and that destroying the blockers allows it to close.
IN_PROC_BROWSER_TEST_F(OmniboxAimPopupBrowserTest,
                       DeactivationBlockersPreventPopupClose) {
  ASSERT_TRUE(ShowPopupAndGetWebUIContent());
  auto* presenter = location_bar()->GetOmniboxPopupAimPresenter();
  ASSERT_TRUE(presenter);
  EXPECT_EQ(location_bar()
                ->GetOmniboxController()
                ->popup_state_manager()
                ->popup_state(),
            OmniboxPopupState::kAim);

  // Wait for popup state transition delayed task to finish.
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return !location_bar()->in_popup_state_transition(); }));

  // Create a deactivation blocker.
  auto blocker = presenter->CreateDeactivationBlocker();
  ASSERT_TRUE(blocker);
  EXPECT_TRUE(presenter->has_active_blockers());

  // Simulate deactivation (loss of widget activation).
  static_cast<views::WidgetObserver*>(presenter)->OnWidgetActivationChanged(
      nullptr, /*active=*/false);

  // Verify the popup did not close (remains kAim).
  EXPECT_EQ(location_bar()
                ->GetOmniboxController()
                ->popup_state_manager()
                ->popup_state(),
            OmniboxPopupState::kAim);

  // Destroy the blocker.
  blocker.reset();
  EXPECT_FALSE(presenter->has_active_blockers());

  // Simulate deactivation again (loss of widget activation).
  static_cast<views::WidgetObserver*>(presenter)->OnWidgetActivationChanged(
      nullptr, /*active=*/false);

  // Verify the popup has now closed (transitions to kNone).
  EXPECT_EQ(location_bar()
                ->GetOmniboxController()
                ->popup_state_manager()
                ->popup_state(),
            OmniboxPopupState::kNone);
}

class OmniboxAimPopupKeepOpenBrowserTest : public OmniboxAimPopupBrowserTest {
 public:
  OmniboxAimPopupKeepOpenBrowserTest() {
    feature_list_.InitAndEnableFeature(
        omnibox::kOmniboxKeepOpenOnFileSelection);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Verifies that opening a file chooser creates a deactivation blocker that
// prevents the AIM popup from closing on activation loss, and that cancelling
// the file selection proxies FileSelectionCanceled and releases the blocker.
IN_PROC_BROWSER_TEST_F(OmniboxAimPopupKeepOpenBrowserTest,
                       FileChooserCancellationReleasesDeactivationBlocker) {
  auto* content = ShowPopupAndGetWebUIContent();
  ASSERT_TRUE(content);
  auto* presenter = location_bar()->GetOmniboxPopupAimPresenter();
  ASSERT_TRUE(presenter);
  // Wait for popup state transition delayed task to finish.
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return !location_bar()->in_popup_state_transition(); }));
  // Register FakeSelectFileDialog factory so FileSelectHelper uses it.
  ui::FakeSelectFileDialog::Factory* factory =
      ui::FakeSelectFileDialog::RegisterFactory();
  bool dialog_opened = false;
  factory->SetOpenCallback(
      base::BindLambdaForTesting([&]() { dialog_opened = true; }));

  // Run the file chooser.
  auto test_listener = base::MakeRefCounted<TestFileSelectListener>();
  blink::mojom::FileChooserParams params;
  params.mode = blink::mojom::FileChooserParams::Mode::kOpen;
  content->GetWebContents()->GetDelegate()->RunFileChooser(
      content->GetWebContents()->GetPrimaryMainFrame(), test_listener, params);
  ASSERT_TRUE(base::test::RunUntil([&]() { return dialog_opened; }));

  // Verify a deactivation blocker was created by `RunFileChooser`.
  EXPECT_TRUE(presenter->has_active_blockers());

  // Simulate loss of widget activation while file dialog is open.
  static_cast<views::WidgetObserver*>(presenter)->OnWidgetActivationChanged(
      nullptr, /*active=*/false);

  // Verify the popup did not close (remains kAim).
  EXPECT_EQ(location_bar()
                ->GetOmniboxController()
                ->popup_state_manager()
                ->popup_state(),
            OmniboxPopupState::kAim);

  // Simulate cancelling the file chooser dialog.
  ui::FakeSelectFileDialog* dialog = factory->GetLastDialog();
  ASSERT_TRUE(dialog);
  dialog->CallFileSelectionCanceled();

  // Verify cancellation was proxied to our listener and the blocker was
  // released via `OnFileChooserClosed`.
  EXPECT_TRUE(test_listener->canceled());
  EXPECT_FALSE(presenter->has_active_blockers());

  // Simulate loss of widget activation immediately after the file dialog has
  // closed. Because focus restoration is pending, this deactivation is blocked.
  static_cast<views::WidgetObserver*>(presenter)->OnWidgetActivationChanged(
      nullptr, /*active=*/false);

  // Verify the popup remains open (blocked by focus restoration).
  EXPECT_EQ(location_bar()
                ->GetOmniboxController()
                ->popup_state_manager()
                ->popup_state(),
            OmniboxPopupState::kAim);

  // Simulate focus restoring to the Omnibox (kFocusRestore reason).
  auto* fm = location_bar()->GetWidget()->GetFocusManager();
  ASSERT_TRUE(fm);
  fm->SetFocusedViewWithReason(
      location_bar()->omnibox_view(),
      views::FocusManager::FocusChangeReason::kFocusRestore);

  // Simulate loss of widget activation now that focus restoration is reset.
  static_cast<views::WidgetObserver*>(presenter)->OnWidgetActivationChanged(
      nullptr, /*active=*/false);

  // Verify the popup now closes (transitions to `kNone`).
  EXPECT_EQ(location_bar()
                ->GetOmniboxController()
                ->popup_state_manager()
                ->popup_state(),
            OmniboxPopupState::kNone);
}

// Verifies that selecting a file in the file chooser proxies `FileSelected`
// and releases the deactivation blocker via `OnFileChooserClosed`.
IN_PROC_BROWSER_TEST_F(OmniboxAimPopupKeepOpenBrowserTest,
                       FileChooserSelectionReleasesDeactivationBlocker) {
  auto* content = ShowPopupAndGetWebUIContent();
  ASSERT_TRUE(content);
  auto* presenter = location_bar()->GetOmniboxPopupAimPresenter();
  ASSERT_TRUE(presenter);

  // Wait for popup state transition delayed task to finish.
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return !location_bar()->in_popup_state_transition(); }));

  // Register FakeSelectFileDialog factory so FileSelectHelper uses it.
  ui::FakeSelectFileDialog::Factory* factory =
      ui::FakeSelectFileDialog::RegisterFactory();
  bool dialog_opened = false;
  factory->SetOpenCallback(
      base::BindLambdaForTesting([&]() { dialog_opened = true; }));

  // Run the file chooser via `WebContentsDelegate` (invokes `RunFileChooser`).
  auto test_listener = base::MakeRefCounted<TestFileSelectListener>();
  blink::mojom::FileChooserParams params;
  params.mode = blink::mojom::FileChooserParams::Mode::kOpen;
  params.accept_types.push_back(u".txt");

  content->GetWebContents()->GetDelegate()->RunFileChooser(
      content->GetWebContents()->GetPrimaryMainFrame(), test_listener, params);
  ASSERT_TRUE(base::test::RunUntil([&]() { return dialog_opened; }));

  // Verify a deactivation blocker is active.
  EXPECT_TRUE(presenter->has_active_blockers());

  // Simulate selecting a file in the dialog.
  ui::FakeSelectFileDialog* dialog = factory->GetLastDialog();
  ASSERT_TRUE(dialog);
  ASSERT_TRUE(dialog->CallFileSelected(
      base::FilePath(FILE_PATH_LITERAL("test.txt")), "txt"));

  // Verify the selection event was proxied to our listener and the
  // deactivation blocker was released (`OnFileChooserClosed` called).
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return test_listener->file_selected(); }));
  EXPECT_FALSE(presenter->has_active_blockers());
}

// Verifies that closing a file selection dialog triggers focus restoration
// handling, preventing a subsequent deactivation event from closing the popup,
// and that it is cleaned up when focus is restored to the Omnibox.
IN_PROC_BROWSER_TEST_F(OmniboxAimPopupKeepOpenBrowserTest,
                       FileSelectionFocusRestorationPreventsPopupClose) {
  ASSERT_TRUE(ShowPopupAndGetWebUIContent());
  auto* presenter = location_bar()->GetOmniboxPopupAimPresenter();
  ASSERT_TRUE(presenter);
  EXPECT_EQ(location_bar()
                ->GetOmniboxController()
                ->popup_state_manager()
                ->popup_state(),
            OmniboxPopupState::kAim);
  // Wait for the popup state transition delayed task to finish.
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return !location_bar()->in_popup_state_transition(); }));
  auto* fm = location_bar()->GetWidget()->GetFocusManager();
  ASSERT_TRUE(fm);

  // Close the file selection dialog.
  presenter->OnFileSelectionClosed();

  // Deactivate the widget. Because the restoration flag is active,
  // this deactivation should be blocked.
  static_cast<views::WidgetObserver*>(presenter)->OnWidgetActivationChanged(
      nullptr, /*active=*/false);

  // Verify the popup remains open.
  EXPECT_EQ(location_bar()
                ->GetOmniboxController()
                ->popup_state_manager()
                ->popup_state(),
            OmniboxPopupState::kAim);

  // Restore focus to the Omnibox. This consumes and resets the restoration
  // flag.
  fm->SetFocusedViewWithReason(
      location_bar()->omnibox_view(),
      views::FocusManager::FocusChangeReason::kFocusRestore);

  // Deactivate the widget again. Because the restoration flag has been reset,
  // this deactivation should now close the popup.
  static_cast<views::WidgetObserver*>(presenter)->OnWidgetActivationChanged(
      nullptr, /*active=*/false);

  // Verify the popup is now closed.
  EXPECT_EQ(location_bar()
                ->GetOmniboxController()
                ->popup_state_manager()
                ->popup_state(),
            OmniboxPopupState::kNone);
}
