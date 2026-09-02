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
#include "chrome/browser/ui/omnibox/omnibox_controller.h"
#include "chrome/browser/ui/omnibox/omnibox_next_features.h"
#include "chrome/browser/ui/omnibox/omnibox_tab_helper.h"
#include "chrome/browser/ui/permission_bubble/permission_prompt.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_aim_popup_webui_content.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_aim_presenter.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_closer.h"
#include "chrome/browser/ui/views/omnibox/omnibox_view_views.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/permissions/permission_request_manager_test_api.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/omnibox/browser/aim_eligibility_service.h"
#include "components/omnibox/browser/mock_aim_eligibility_service.h"
#include "components/permissions/permission_request_manager.h"
#include "components/permissions/test/mock_permission_prompt_factory.h"
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
                                   {features::kWebUILocationBar});
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

  void DeactivatePresenterAndVerifyState(
      OmniboxPopupAimPresenter* presenter,
      OmniboxPopupState expected_state = OmniboxPopupState::kNone) {
    static_cast<views::WidgetObserver*>(presenter)->OnWidgetActivationChanged(
        nullptr, /*active=*/false);
    ASSERT_TRUE(base::test::RunUntil(
        [&]() { return !location_bar()->in_popup_state_transition(); }));
    EXPECT_EQ(location_bar()
                  ->GetOmniboxController()
                  ->popup_state_manager()
                  ->popup_state(),
              expected_state);
  }

  void VerifyFocusRestoredToWebUIInput(OmniboxPopupAimPresenter* presenter,
                                       OmniboxAimPopupWebUIContent* content) {
    // Wait until focus is redirected from the location bar to the popup's
    // WebUI content view in Views.
    ASSERT_TRUE(base::test::RunUntil(
        [&]() { return presenter->GetWebUIContent()->HasFocus(); }));

    // Wait until the Mojo IPC reaches the WebUI DOM and focuses the input
    // element inside the shadow root.
    ASSERT_TRUE(base::test::RunUntil([&]() {
      return content::EvalJs(
                 content->GetWebContents(),
                 "(function() {"
                 "  const app = document.querySelector('omnibox-aim-app');"
                 "  const box = app && app.shadowRoot && "
                 "              "
                 "app.shadowRoot.querySelector('cr-omnibox-composebox');"
                 "  return !!box && box.isFocusInInput();"
                 "})()")
          .ExtractBool();
    }));
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

  // Wait for the WebContents to finish loading and popup state transition to
  // finish.
  content::WaitForLoadStop(content->GetWebContents());
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return !location_bar()->in_popup_state_transition(); }));

  auto* permission_manager =
      permissions::PermissionRequestManager::FromWebContents(
          content->GetWebContents());
  ASSERT_TRUE(permission_manager);

  permissions::MockPermissionPromptFactory prompt_factory(permission_manager);

  // Add a mock permission request for a standard origin to set
  // IsRequestInProgress() to true without auto-approving. This avoids needing
  // a page navigation while allowing for a website to request a permission.
  // Cannot be about:blank due to DCheck.
  auto request = std::make_unique<permissions::MockPermissionRequest>(
      GURL("https://example.com"), permissions::RequestType::kMicStream,
      permissions::PermissionRequestGestureType::GESTURE);
  permission_manager->AddRequest(
      content->GetWebContents()->GetPrimaryMainFrame(), std::move(request));
  prompt_factory.WaitForPermissionBubble();

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

// Verify that permission prompt added notification prevents omnibox popup
// close.
IN_PROC_BROWSER_TEST_F(OmniboxAimPopupBrowserTest,
                       PermissionPromptAddedPreventsPopupClose) {
  location_bar()->GetOmniboxController()->popup_state_manager()->SetPopupState(
      OmniboxPopupState::kAim);

  auto* presenter = location_bar()->GetOmniboxPopupAimPresenter();
  ASSERT_TRUE(presenter);
  presenter->Show();

  // Simulate `OnPromptAdded` notification (as if permission prompt is shown).
  static_cast<permissions::PermissionRequestManager::Observer*>(presenter)
      ->OnPromptAdded();

  // Simulate loss of widget activation while permission prompt is showing.
  static_cast<views::WidgetObserver*>(presenter)->OnWidgetActivationChanged(
      nullptr, /*active=*/false);

  // Verify popup state is still kAim (omnibox did not close due to activation
  // loss).
  EXPECT_EQ(location_bar()
                ->GetOmniboxController()
                ->popup_state_manager()
                ->popup_state(),
            OmniboxPopupState::kAim);
}

class OmniboxAimPopupKeepOpenBrowserTest
    : public OmniboxAimPopupBrowserTest,
      public ::testing::WithParamInterface<bool> {
 public:
  OmniboxAimPopupKeepOpenBrowserTest() {
    feature_list_.InitWithFeatureState(omnibox::kOmniboxKeepOpenOnFileSelection,
                                       IsKeepOpenEnabled());
  }

  bool IsKeepOpenEnabled() const { return GetParam(); }

 private:
  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         OmniboxAimPopupKeepOpenBrowserTest,
                         ::testing::Bool());

// Verify that active deactivation blockers prevent the omnibox popup from
// closing, and that destroying the blockers allows it to close.
IN_PROC_BROWSER_TEST_P(OmniboxAimPopupKeepOpenBrowserTest,
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
  if (IsKeepOpenEnabled()) {
    ASSERT_TRUE(blocker);
    EXPECT_TRUE(presenter->has_active_blockers());

    // Simulate deactivation (loss of widget activation).
    static_cast<views::WidgetObserver*>(presenter)->OnWidgetActivationChanged(
        nullptr, /*active=*/false);
    ASSERT_TRUE(base::test::RunUntil(
        [&]() { return !location_bar()->in_popup_state_transition(); }));

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
    ASSERT_TRUE(base::test::RunUntil(
        [&]() { return !location_bar()->in_popup_state_transition(); }));

    // Verify the popup has now closed (transitions to kNone).
    EXPECT_EQ(location_bar()
                  ->GetOmniboxController()
                  ->popup_state_manager()
                  ->popup_state(),
              OmniboxPopupState::kNone);
  } else {
    EXPECT_FALSE(blocker);
    EXPECT_FALSE(presenter->has_active_blockers());

    // Simulate deactivation (loss of widget activation).
    static_cast<views::WidgetObserver*>(presenter)->OnWidgetActivationChanged(
        nullptr, /*active=*/false);
    ASSERT_TRUE(base::test::RunUntil(
        [&]() { return !location_bar()->in_popup_state_transition(); }));

    // Verify the popup closes immediately when feature is disabled.
    EXPECT_EQ(location_bar()
                  ->GetOmniboxController()
                  ->popup_state_manager()
                  ->popup_state(),
              OmniboxPopupState::kNone);
  }
}

// Verifies that opening a file chooser creates a deactivation blocker that
// prevents the AIM popup from closing on activation loss, and that cancelling
// the file selection proxies FileSelectionCanceled and releases the blocker.
IN_PROC_BROWSER_TEST_P(OmniboxAimPopupKeepOpenBrowserTest,
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

  if (IsKeepOpenEnabled()) {
    // Verify a deactivation blocker was created by `RunFileChooser`.
    EXPECT_TRUE(presenter->has_active_blockers());

    // Simulate loss of widget activation while file dialog is open.
    static_cast<views::WidgetObserver*>(presenter)->OnWidgetActivationChanged(
        nullptr, /*active=*/false);
    ASSERT_TRUE(base::test::RunUntil(
        [&]() { return !location_bar()->in_popup_state_transition(); }));

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
    // closed. Because focus restoration is pending, this deactivation is
    // blocked.
    static_cast<views::WidgetObserver*>(presenter)->OnWidgetActivationChanged(
        nullptr, /*active=*/false);
    ASSERT_TRUE(base::test::RunUntil(
        [&]() { return !location_bar()->in_popup_state_transition(); }));

    // Verify the popup remains open (blocked by focus restoration).
    EXPECT_EQ(location_bar()
                  ->GetOmniboxController()
                  ->popup_state_manager()
                  ->popup_state(),
              OmniboxPopupState::kAim);

    // Simulate focus restoring to the Omnibox.
    auto* fm = location_bar()->GetWidget()->GetFocusManager();
    ASSERT_TRUE(fm);
    EXPECT_TRUE(presenter->is_restoring_focus_after_file_selection());
    fm->ClearFocus();
    fm->SetFocusedViewWithReason(
        location_bar()->omnibox_view(),
        views::FocusManager::FocusChangeReason::kFocusRestore);
    ASSERT_TRUE(base::test::RunUntil([&]() {
      return !presenter->is_restoring_focus_after_file_selection();
    }));

    // Simulate loss of widget activation now that focus restoration is reset.
    static_cast<views::WidgetObserver*>(presenter)->OnWidgetActivationChanged(
        nullptr, /*active=*/false);
    ASSERT_TRUE(base::test::RunUntil(
        [&]() { return !location_bar()->in_popup_state_transition(); }));

    // Verify the popup now closes (transitions to `kNone`).
    EXPECT_EQ(location_bar()
                  ->GetOmniboxController()
                  ->popup_state_manager()
                  ->popup_state(),
              OmniboxPopupState::kNone);
  } else {
    // Verify no deactivation blocker was created.
    EXPECT_FALSE(presenter->has_active_blockers());

    // Simulate loss of widget activation while file dialog is open.
    static_cast<views::WidgetObserver*>(presenter)->OnWidgetActivationChanged(
        nullptr, /*active=*/false);
    ASSERT_TRUE(base::test::RunUntil(
        [&]() { return !location_bar()->in_popup_state_transition(); }));

    // Verify the popup closes when feature is disabled.
    EXPECT_EQ(location_bar()
                  ->GetOmniboxController()
                  ->popup_state_manager()
                  ->popup_state(),
              OmniboxPopupState::kNone);

    // Simulate cancelling the file chooser dialog.
    ui::FakeSelectFileDialog* dialog = factory->GetLastDialog();
    ASSERT_TRUE(dialog);
    dialog->CallFileSelectionCanceled();

    // Verify cancellation was still proxied to our listener.
    EXPECT_TRUE(test_listener->canceled());
  }
}

// Verifies that selecting a file in the file chooser proxies `FileSelected`
// and releases the deactivation blocker via `OnFileChooserClosed`.
IN_PROC_BROWSER_TEST_P(OmniboxAimPopupKeepOpenBrowserTest,
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

  if (IsKeepOpenEnabled()) {
    // Verify a deactivation blocker is active.
    EXPECT_TRUE(presenter->has_active_blockers());
  } else {
    EXPECT_FALSE(presenter->has_active_blockers());
  }

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
IN_PROC_BROWSER_TEST_P(OmniboxAimPopupKeepOpenBrowserTest,
                       FileSelectionFocusRestorationPreventsPopupClose) {
  auto* content = ShowPopupAndGetWebUIContent();
  ASSERT_TRUE(content);
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

  if (IsKeepOpenEnabled()) {
    // Deactivate the widget. Because the restoration flag is active,
    // this deactivation should be blocked.
    DeactivatePresenterAndVerifyState(presenter, OmniboxPopupState::kAim);

    // Clear focus first so SetFocusedViewWithReason triggers a focus change
    // event.
    EXPECT_TRUE(presenter->is_restoring_focus_after_file_selection());
    fm->ClearFocus();
    fm->SetFocusedViewWithReason(
        location_bar()->omnibox_view(),
        views::FocusManager::FocusChangeReason::kFocusRestore);
    ASSERT_TRUE(base::test::RunUntil([&]() {
      return !presenter->is_restoring_focus_after_file_selection();
    }));

    VerifyFocusRestoredToWebUIInput(presenter, content);

    // Deactivate the widget again. Because the restoration flag has been reset,
    // this deactivation should now close the popup.
    DeactivatePresenterAndVerifyState(presenter, OmniboxPopupState::kNone);
  } else {
    // When feature is disabled, deactivation closes the popup.
    DeactivatePresenterAndVerifyState(presenter, OmniboxPopupState::kNone);
  }
}

// Verifies that a widget activation sequence (active=true followed by
// active=false during window reactivation) after closing file selection does
// not prematurely reset focus restoration state or close the AIM popup when
// kOmniboxKeepOpenOnFileSelection is enabled.
IN_PROC_BROWSER_TEST_P(
    OmniboxAimPopupKeepOpenBrowserTest,
    ActivationChangedAfterFileSelectionClosedPreservesPopup) {
  auto* content = ShowPopupAndGetWebUIContent();
  ASSERT_TRUE(content);
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

  // Simulate file selection dialog closure.
  presenter->OnFileSelectionClosed();

  if (IsKeepOpenEnabled()) {
    // Simulate window reactivation when file chooser closes.
    static_cast<views::WidgetObserver*>(presenter)->OnWidgetActivationChanged(
        nullptr, /*active=*/true);
    // Simulate subsequent focus transition/deactivation event.
    static_cast<views::WidgetObserver*>(presenter)->OnWidgetActivationChanged(
        nullptr, /*active=*/false);
    // Wait for any popup state transition to finish before verifying state.
    ASSERT_TRUE(base::test::RunUntil(
        [&]() { return !location_bar()->in_popup_state_transition(); }));
    // Verify the popup remains open (blocked by focus restoration).
    EXPECT_EQ(location_bar()
                  ->GetOmniboxController()
                  ->popup_state_manager()
                  ->popup_state(),
              OmniboxPopupState::kAim);

    // Simulate focus restoration to the omnibox textfield after the activation
    // sequence. If the activation toggle prematurely reset the restoration
    // state, this will fail to redirect focus away from the omnibox_view.
    auto* fm = location_bar()->GetWidget()->GetFocusManager();
    ASSERT_TRUE(fm);
    EXPECT_TRUE(presenter->is_restoring_focus_after_file_selection());
    fm->ClearFocus();
    fm->SetFocusedViewWithReason(
        location_bar()->omnibox_view(),
        views::FocusManager::FocusChangeReason::kFocusRestore);
    ASSERT_TRUE(base::test::RunUntil([&]() {
      return !presenter->is_restoring_focus_after_file_selection();
    }));

    VerifyFocusRestoredToWebUIInput(presenter, content);
  } else {
    // When feature is disabled, deactivation closes the popup.
    DeactivatePresenterAndVerifyState(presenter, OmniboxPopupState::kNone);
  }
}

// Verifies that if focus moves to a view other than the omnibox text field
// while file selection restoration is pending, the restoration state is
// cancelled (`else if (focused_now)` branch) and subsequent deactivations close
// the popup.
IN_PROC_BROWSER_TEST_P(OmniboxAimPopupKeepOpenBrowserTest,
                       FocusMovingElsewhereCancelsRestorationState) {
  ASSERT_TRUE(ShowPopupAndGetWebUIContent());
  auto* presenter = location_bar()->GetOmniboxPopupAimPresenter();
  ASSERT_TRUE(presenter);
  EXPECT_EQ(location_bar()
                ->GetOmniboxController()
                ->popup_state_manager()
                ->popup_state(),
            OmniboxPopupState::kAim);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return !location_bar()->in_popup_state_transition(); }));

  presenter->OnFileSelectionClosed();

  if (IsKeepOpenEnabled()) {
    auto* fm = location_bar()->GetWidget()->GetFocusManager();
    ASSERT_TRUE(fm);

    // Simulate focus moving to a different focusable view (e.g.,
    // location_icon_view()) rather than omnibox_view().
    auto* icon_view = location_bar()->location_icon_view();
    ASSERT_TRUE(icon_view);
    fm->ClearFocus();
    fm->SetFocusedView(icon_view);

    // Verify focus stays on that view and is not redirected to the popup.
    ASSERT_TRUE(base::test::RunUntil(
        [&]() { return fm->GetFocusedView() == icon_view; }));

    // Because OnDidChangeFocus hit `else if (focused_now)` and called
    // ResetFocusRestorationState(), deactivating the widget should now close
    // the popup.
    DeactivatePresenterAndVerifyState(presenter, OmniboxPopupState::kNone);
  } else {
    // When feature is disabled, deactivation closes the popup.
    DeactivatePresenterAndVerifyState(presenter, OmniboxPopupState::kNone);
  }
}

namespace {

class TestPermissionPromptDelegate
    : public permissions::PermissionPrompt::Delegate {
 public:
  explicit TestPermissionPromptDelegate(content::WebContents* web_contents)
      : web_contents_(web_contents) {
    request_list_.push_back(
        std::make_unique<permissions::MockPermissionRequest>(
            permissions::RequestType::kMicStream,
            permissions::PermissionRequestGestureType::GESTURE));
  }

  const std::vector<std::unique_ptr<permissions::PermissionRequest>>& Requests()
      const override {
    return request_list_;
  }
  GURL GetRequestingOrigin() const override {
    return GURL(permissions::MockPermissionRequest::kDefaultOrigin);
  }
  GURL GetEmbeddingOrigin() const override {
    return GURL(permissions::MockPermissionRequest::kDefaultOrigin);
  }
  void Accept(const PromptOptions& prompt_options) override {}
  void AcceptThisTime(const PromptOptions& prompt_options) override {}
  void Deny(const PromptOptions& prompt_options) override {}
  void Dismiss(const PromptOptions& prompt_options) override {}
  void Ignore(const PromptOptions& prompt_options) override {}
  void SwitchToLoudPrompt() override {}
  GeolocationAccuracy GetInitialGeolocationAccuracySelection() const override {
    return GeolocationAccuracy::kPrecise;
  }
  std::optional<permissions::GeolocationPromptType> GetGeolocationPromptType()
      const override {
    return std::nullopt;
  }
  void FinalizeCurrentRequests() override {}
  void OpenHelpCenterLink(const ui::Event&) override {}
  void PreIgnoreQuietPrompt() override {}
  void SetManageClicked() override {}
  void SetLearnMoreClicked() override {}
  void SetHatsShownCallback(base::OnceCallback<void()> callback) override {}
  std::optional<permissions::PermissionUiSelector::QuietUiReason>
  ReasonForUsingQuietUi() const override {
    return std::nullopt;
  }
  bool ShouldCurrentRequestUseQuietUI() const override { return false; }
  void set_should_drop(bool drop) { should_drop_ = drop; }
  bool ShouldDropCurrentRequestIfCannotShowQuietly() const override {
    return should_drop_;
  }
  bool WasCurrentRequestAlreadyDisplayed() override { return false; }
  void SetDismissOnTabClose() override {}
  void SetPromptShown() override {}
  void SetDecisionTime() override {}
  bool RecreateView() override { return false; }
  const permissions::PermissionPrompt* GetCurrentPrompt() const override {
    return nullptr;
  }
  base::WeakPtr<permissions::PermissionPrompt::Delegate> GetWeakPtr() override {
    return weak_factory_.GetWeakPtr();
  }
  content::WebContents* GetAssociatedWebContents() override {
    return web_contents_;
  }

 private:
  bool should_drop_ = false;
  raw_ptr<content::WebContents> web_contents_;
  std::vector<std::unique_ptr<permissions::PermissionRequest>> request_list_;
  base::WeakPtrFactory<TestPermissionPromptDelegate> weak_factory_{this};
};

}  // namespace

// Verifies that when the Omnibox popup is open, creating a microphone
// permission prompt via `PermissionPromptFactory` notifies the presenter
// synchronously and prevents the popup from closing on deactivation.
IN_PROC_BROWSER_TEST_F(OmniboxAimPopupBrowserTest,
                       PermissionPromptCreationPreventsPopupCloseWhenOpen) {
  ASSERT_TRUE(ShowPopupAndGetWebUIContent());
  auto* presenter = location_bar()->GetOmniboxPopupAimPresenter();
  ASSERT_TRUE(presenter);
  EXPECT_TRUE(presenter->IsShown());

  // Initially, before `PermissionPromptFactory` runs, presenter is NOT locked.
  // (Verifies `PermissionRequestManager` did NOT set it).
  EXPECT_FALSE(presenter->IsPermissionPromptPreventingClose());

  auto* web_contents = browser()->GetTabStripModel()->GetActiveWebContents();
  TestPermissionPromptDelegate test_delegate(web_contents);

  // Directly call `PermissionPromptFactory::CreatePermissionPrompt`
  // synchronously.
  CreatePermissionPrompt(web_contents, &test_delegate);

  // Presenter MUST be locked synchronously by `PermissionPromptFactory`.
  EXPECT_TRUE(presenter->IsPermissionPromptPreventingClose());

  // Deactivating the widget via `OnWidgetActivationChanged` while permission
  // prompt is active should NOT close the popup.
  static_cast<views::WidgetObserver*>(presenter)->OnWidgetActivationChanged(
      nullptr, /*active=*/false);
  EXPECT_EQ(location_bar()
                ->GetOmniboxController()
                ->popup_state_manager()
                ->popup_state(),
            OmniboxPopupState::kAim);

  // `OmniboxPopupCloser::CloseWithReason(kBlur)` close event must
  // also be ignored while permission prompt is showing.
  if (auto* popup_closer = location_bar()
                               ->GetOmniboxController()
                               ->client()
                               ->GetOmniboxPopupCloser()) {
    popup_closer->CloseWithReason(omnibox::PopupCloseReason::kBlur);
    EXPECT_EQ(location_bar()
                  ->GetOmniboxController()
                  ->popup_state_manager()
                  ->popup_state(),
              OmniboxPopupState::kAim);
  }
}

// Verifies that when the Omnibox popup is closed, creating a permission prompt
// does NOT set permission prompt showing on the presenter.
IN_PROC_BROWSER_TEST_F(OmniboxAimPopupBrowserTest,
                       PermissionPromptCreationDoesNotLockPresenterWhenClosed) {
  auto* presenter = location_bar()->GetOmniboxPopupAimPresenter();
  ASSERT_TRUE(presenter);
  EXPECT_FALSE(presenter->IsShown());

  auto* web_contents = browser()->GetTabStripModel()->GetActiveWebContents();
  TestPermissionPromptDelegate test_delegate(web_contents);

  // Directly call PermissionPromptFactory::CreatePermissionPrompt synchronously
  // when closed.
  CreatePermissionPrompt(web_contents, &test_delegate);

  // When presenter is not shown, PermissionPromptFactory does NOT lock
  // presenter.
  EXPECT_FALSE(presenter->IsPermissionPromptPreventingClose());
}

// Verifies that if prompt creation returns `nullptr`, the presenter flag is
// reset.
IN_PROC_BROWSER_TEST_F(
    OmniboxAimPopupBrowserTest,
    PermissionPromptCreationResetsLockIfPromptCreationFails) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/empty.html")));

  ASSERT_TRUE(ShowPopupAndGetWebUIContent());
  auto* presenter = location_bar()->GetOmniboxPopupAimPresenter();
  ASSERT_TRUE(presenter);
  EXPECT_TRUE(presenter->IsShown());

  // Allow prompt to drop if it cannot show quietly.
  auto* web_contents = browser()->GetTabStripModel()->GetActiveWebContents();
  TestPermissionPromptDelegate test_delegate(web_contents);
  test_delegate.set_should_drop(true);

  // Set full screen so prompt will fail to render once. Because the prompt
  // is allowed to drop, the prompt will not show at all.
  ui_test_utils::ToggleFullscreenModeAndWait(browser());

  // Directly call `PermissionPromptFactory::CreatePermissionPrompt`
  // synchronously.
  auto prompt = CreatePermissionPrompt(web_contents, &test_delegate);
  EXPECT_EQ(nullptr, prompt);

  // Presenter MUST NOT be locked if prompt creation returned `nullptr`.
  EXPECT_FALSE(presenter->IsPermissionPromptPreventingClose());
}

// Verifies that when `kWebUIOmniboxFullPopup` is enabled, `OmniboxPopupCloser`
// transitions `kFull` popup state to `kNone` on `CloseWithReason(kRevertAll)`.
IN_PROC_BROWSER_TEST_F(OmniboxAimPopupBrowserTest,
                       PopupCloserTransitionsFullPopupStateToNone) {
  auto* state_manager =
      location_bar()->GetOmniboxController()->popup_state_manager();
  ASSERT_TRUE(state_manager);

  state_manager->SetPopupState(OmniboxPopupState::kFull);
  EXPECT_EQ(state_manager->popup_state(), OmniboxPopupState::kFull);

  auto* popup_closer =
      location_bar()->GetOmniboxController()->client()->GetOmniboxPopupCloser();
  ASSERT_TRUE(popup_closer);
  popup_closer->CloseWithReason(omnibox::PopupCloseReason::kRevertAll);

  EXPECT_EQ(state_manager->popup_state(), OmniboxPopupState::kNone);
}
