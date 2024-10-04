// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/memory/weak_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/accessibility/accessibility_state_utils.h"
#include "chrome/browser/ui/autofill/autofill_popup_controller_impl.h"
#include "chrome/browser/ui/autofill/autofill_popup_view.h"
#include "chrome/browser/ui/autofill/autofill_suggestion_controller_test_base.h"
#include "chrome/browser/ui/autofill/popup_controller_common.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/content/browser/content_autofill_driver_factory.h"
#include "components/autofill/content/browser/content_autofill_driver_factory_test_api.h"
#include "components/autofill/content/browser/test_autofill_client_injector.h"
#include "components/autofill/content/browser/test_autofill_driver_injector.h"
#include "components/autofill/content/browser/test_autofill_manager_injector.h"
#include "components/autofill/content/browser/test_content_autofill_client.h"
#include "components/autofill/core/browser/autofill_driver_router.h"
#include "components/autofill/core/browser/autofill_external_delegate.h"
#include "components/autofill/core/browser/autofill_manager.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/browser_autofill_manager_test_api.h"
#include "components/autofill/core/browser/ui/autofill_suggestion_delegate.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/autofill/core/browser/ui/suggestion_hiding_reason.h"
#include "components/autofill/core/browser/ui/suggestion_type.h"
#include "components/autofill/core/common/aliases.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/input/native_web_keyboard_event.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/video_picture_in_picture_window_controller.h"
#include "content/public/browser/weak_document_ptr.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/navigation_simulator.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/text_utils.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/autofill/test_autofill_keyboard_accessory_controller_autofill_client.h"
#else
#include "chrome/browser/ui/autofill/test_autofill_popup_controller_autofill_client.h"
#endif

namespace autofill {
namespace {

using base::ASCIIToUTF16;
using base::WeakPtr;
using ::testing::_;
using ::testing::AtLeast;
using ::testing::Field;
using ::testing::Invoke;
using ::testing::Mock;
using ::testing::Optional;
using ::testing::Return;

using TestAutofillSuggestionControllerAutofillClient =
#if BUILDFLAG(IS_ANDROID)
    TestAutofillKeyboardAccessoryControllerAutofillClient<>;
#else
    TestAutofillPopupControllerAutofillClient<>;
#endif

content::RenderFrameHost* CreateAndNavigateChildFrame(
    content::RenderFrameHost* parent,
    const GURL& url,
    std::string_view name) {
  content::RenderFrameHost* rfh =
      content::RenderFrameHostTester::For(parent)->AppendChild(
          std::string(name));
  // ContentAutofillDriverFactory::DidFinishNavigation() creates a driver for
  // subframes only if
  // `NavigationHandle::HasSubframeNavigationEntryCommitted()` is true. This
  // is not the case for the first navigation. (In non-unit-tests, the first
  // navigation creates a driver in
  // ContentAutofillDriverFactory::BindAutofillDriver().) Therefore,
  // we simulate *two* navigations here, and explicitly set the transition
  // type for the second navigation.
  std::unique_ptr<content::NavigationSimulator> simulator;
  // First navigation: `HasSubframeNavigationEntryCommitted() == false`.
  // Must be a different URL from the second navigation.
  GURL about_blank("about:blank");
  CHECK_NE(about_blank, url);
  simulator =
      content::NavigationSimulator::CreateRendererInitiated(about_blank, rfh);
  simulator->Commit();
  rfh = simulator->GetFinalRenderFrameHost();
  // Second navigation: `HasSubframeNavigationEntryCommitted() == true`.
  // Must set the transition type to ui::PAGE_TRANSITION_MANUAL_SUBFRAME.
  simulator = content::NavigationSimulator::CreateRendererInitiated(url, rfh);
  simulator->SetTransition(ui::PAGE_TRANSITION_MANUAL_SUBFRAME);
  simulator->Commit();
  return simulator->GetFinalRenderFrameHost();
}

content::RenderFrameHost* NavigateAndCommitFrame(content::RenderFrameHost* rfh,
                                                 const GURL& url) {
  std::unique_ptr<content::NavigationSimulator> simulator =
      content::NavigationSimulator::CreateRendererInitiated(url, rfh);
  simulator->Commit();
  return simulator->GetFinalRenderFrameHost();
}

using AutofillSuggestionControllerTest = AutofillSuggestionControllerTestBase<
    TestAutofillSuggestionControllerAutofillClient>;

// Regression test for (crbug.com/1513574): Showing an Autofill Compose
// suggestion twice does not crash.
TEST_F(AutofillSuggestionControllerTest, ShowTwice) {
  ShowSuggestions(manager(), {Suggestion(u"Help me write",
                                         SuggestionType::kComposeResumeNudge)});
  ShowSuggestions(manager(), {Suggestion(u"Help me write",
                                         SuggestionType::kComposeResumeNudge)});
}

// Tests that the AED is informed when suggestions were shown.
TEST_F(AutofillSuggestionControllerTest, ShowInformsDelegate) {
  EXPECT_CALL(manager().external_delegate(), OnSuggestionsShown);
  ShowSuggestions(manager(), {SuggestionType::kAddressEntry});
}

TEST_F(AutofillSuggestionControllerTest, UpdateDataListValues) {
  ShowSuggestions(manager(), {SuggestionType::kAddressEntry});
  std::vector<SelectOption> options = {
      {.value = u"data list value 1", .text = u"data list label 1"}};
  client().popup_controller(manager()).UpdateDataListValues(options);

  ASSERT_EQ(3, client().popup_controller(manager()).GetLineCount());

  Suggestion result0 = client().popup_controller(manager()).GetSuggestionAt(0);
  EXPECT_EQ(options[0].value, result0.main_text.value);
  EXPECT_EQ(
      options[0].value,
      client().popup_controller(manager()).GetSuggestionAt(0).main_text.value);
  ASSERT_EQ(1u, result0.labels.size());
  ASSERT_EQ(1u, result0.labels[0].size());
  EXPECT_EQ(options[0].text, result0.labels[0][0].value);
  EXPECT_EQ(std::u16string(), result0.additional_label);
  EXPECT_EQ(options[0].text, client()
                                 .popup_controller(manager())
                                 .GetSuggestionAt(0)
                                 .labels[0][0]
                                 .value);
  EXPECT_EQ(SuggestionType::kDatalistEntry, result0.type);

  Suggestion result1 = client().popup_controller(manager()).GetSuggestionAt(1);
  EXPECT_EQ(std::u16string(), result1.main_text.value);
  EXPECT_TRUE(result1.labels.empty());
  EXPECT_EQ(std::u16string(), result1.additional_label);
  EXPECT_EQ(SuggestionType::kSeparator, result1.type);

  Suggestion result2 = client().popup_controller(manager()).GetSuggestionAt(2);
  EXPECT_EQ(std::u16string(), result2.main_text.value);
  EXPECT_TRUE(result2.labels.empty());
  EXPECT_EQ(std::u16string(), result2.additional_label);
  EXPECT_EQ(SuggestionType::kAddressEntry, result2.type);

  // Add two data list entries (which should replace the current one).
  options.push_back(
      {.value = u"data list value 1", .text = u"data list label 1"});
  client().popup_controller(manager()).UpdateDataListValues(options);
  ASSERT_EQ(4, client().popup_controller(manager()).GetLineCount());

  // Original one first, followed by new one, then separator.
  EXPECT_EQ(
      options[0].value,
      client().popup_controller(manager()).GetSuggestionAt(0).main_text.value);
  EXPECT_EQ(
      options[0].value,
      client().popup_controller(manager()).GetSuggestionAt(0).main_text.value);
  ASSERT_EQ(
      1u,
      client().popup_controller(manager()).GetSuggestionAt(0).labels.size());
  ASSERT_EQ(
      1u,
      client().popup_controller(manager()).GetSuggestionAt(0).labels[0].size());
  EXPECT_EQ(options[0].text, client()
                                 .popup_controller(manager())
                                 .GetSuggestionAt(0)
                                 .labels[0][0]
                                 .value);
  EXPECT_EQ(
      std::u16string(),
      client().popup_controller(manager()).GetSuggestionAt(0).additional_label);
  EXPECT_EQ(
      options[1].value,
      client().popup_controller(manager()).GetSuggestionAt(1).main_text.value);
  EXPECT_EQ(
      options[1].value,
      client().popup_controller(manager()).GetSuggestionAt(1).main_text.value);
  ASSERT_EQ(
      1u,
      client().popup_controller(manager()).GetSuggestionAt(1).labels.size());
  ASSERT_EQ(
      1u,
      client().popup_controller(manager()).GetSuggestionAt(1).labels[0].size());
  EXPECT_EQ(options[1].text, client()
                                 .popup_controller(manager())
                                 .GetSuggestionAt(1)
                                 .labels[0][0]
                                 .value);
  EXPECT_EQ(
      std::u16string(),
      client().popup_controller(manager()).GetSuggestionAt(1).additional_label);
  EXPECT_EQ(SuggestionType::kSeparator,
            client().popup_controller(manager()).GetSuggestionAt(2).type);

  // Clear all data list values.
  options.clear();
  client().popup_controller(manager()).UpdateDataListValues(options);

  ASSERT_EQ(1, client().popup_controller(manager()).GetLineCount());
  EXPECT_EQ(SuggestionType::kAddressEntry,
            client().popup_controller(manager()).GetSuggestionAt(0).type);
}

TEST_F(AutofillSuggestionControllerTest, PopupsWithOnlyDataLists) {
  // Create the popup with a single datalist element.
  ShowSuggestions(manager(), {SuggestionType::kDatalistEntry});

  // Replace the datalist element with a new one.
  std::vector<SelectOption> options = {
      {.value = u"data list value 1", .text = u"data list label 1"}};
  client().popup_controller(manager()).UpdateDataListValues(options);

  ASSERT_EQ(1, client().popup_controller(manager()).GetLineCount());
  EXPECT_EQ(
      options[0].value,
      client().popup_controller(manager()).GetSuggestionAt(0).main_text.value);
  ASSERT_EQ(
      1u,
      client().popup_controller(manager()).GetSuggestionAt(0).labels.size());
  ASSERT_EQ(
      1u,
      client().popup_controller(manager()).GetSuggestionAt(0).labels[0].size());
  EXPECT_EQ(options[0].text, client()
                                 .popup_controller(manager())
                                 .GetSuggestionAt(0)
                                 .labels[0][0]
                                 .value);
  EXPECT_EQ(
      std::u16string(),
      client().popup_controller(manager()).GetSuggestionAt(0).additional_label);
  EXPECT_EQ(SuggestionType::kDatalistEntry,
            client().popup_controller(manager()).GetSuggestionAt(0).type);

  // Clear datalist values and check that the popup becomes hidden.
  EXPECT_CALL(client().popup_controller(manager()),
              Hide(SuggestionHidingReason::kNoSuggestions));
  options.clear();
  client().popup_controller(manager()).UpdateDataListValues(options);
}

TEST_F(AutofillSuggestionControllerTest, GetOrCreate) {
  auto create_controller = [&](gfx::RectF bounds) {
    return AutofillSuggestionController::GetOrCreate(
        client().popup_controller(manager()).GetWeakPtr(),
        manager().external_delegate().GetWeakPtrForTest(), nullptr,
        PopupControllerCommon(std::move(bounds), base::i18n::UNKNOWN_DIRECTION,
                              nullptr),
        /*form_control_ax_id=*/0);
  };
  WeakPtr<AutofillSuggestionController> controller = create_controller(gfx::RectF());
  EXPECT_TRUE(controller);

  controller->Hide(SuggestionHidingReason::kViewDestroyed);
  EXPECT_FALSE(controller);

  controller = create_controller(gfx::RectF());
  EXPECT_TRUE(controller);

  WeakPtr<AutofillSuggestionController> controller2 =
      create_controller(gfx::RectF());
  EXPECT_EQ(controller.get(), controller2.get());

  controller->Hide(SuggestionHidingReason::kViewDestroyed);
  EXPECT_FALSE(controller);
  EXPECT_FALSE(controller2);

  EXPECT_CALL(client().popup_controller(manager()),
              Hide(SuggestionHidingReason::kViewDestroyed));
  gfx::RectF bounds(0.f, 0.f, 1.f, 2.f);
  base::WeakPtr<AutofillSuggestionController> controller3 =
      create_controller(bounds);
  EXPECT_EQ(&client().popup_controller(manager()), controller3.get());
  EXPECT_EQ(bounds, static_cast<AutofillSuggestionController*>(controller3.get())
                        ->element_bounds());
  controller3->Hide(SuggestionHidingReason::kViewDestroyed);

  client().popup_controller(manager()).DoHide();

  const base::WeakPtr<AutofillSuggestionController> controller4 =
      create_controller(bounds);
  EXPECT_EQ(&client().popup_controller(manager()), controller4.get());
  EXPECT_EQ(bounds,
            static_cast<const AutofillSuggestionController*>(controller4.get())
                ->element_bounds());

  client().popup_controller(manager()).DoHide();
}

// Tests that the controller does not have a UI session id if it has no view.
TEST_F(AutofillSuggestionControllerTest, EmptyUiSessionIdAfterCreation) {
  test_api(client().popup_controller(manager())).SetView(nullptr);
  EXPECT_EQ(client().popup_controller(manager()).GetUiSessionId(),
            std::nullopt);
}

// Tests that the controller has a UI session id after `Show` is called.
TEST_F(AutofillSuggestionControllerTest, NonEmptyUiSessionIdAfterShow) {
  ShowSuggestions(manager(), {SuggestionType::kAutocompleteEntry,
                              SuggestionType::kAutocompleteEntry});
  EXPECT_TRUE(
      client().popup_controller(manager()).GetUiSessionId().has_value());
}

TEST_F(AutofillSuggestionControllerTest, ProperlyResetController) {
  ShowSuggestions(manager(), {SuggestionType::kAutocompleteEntry,
                              SuggestionType::kAutocompleteEntry});

  // Now show a new popup with the same controller, but with fewer items.
  base::WeakPtr<AutofillSuggestionController> controller =
      AutofillSuggestionController::GetOrCreate(
          client().popup_controller(manager()).GetWeakPtr(),
          manager().external_delegate().GetWeakPtrForTest(), nullptr,
          PopupControllerCommon(gfx::RectF(), base::i18n::UNKNOWN_DIRECTION,
                                nullptr),
          /*form_control_ax_id=*/0);
  EXPECT_EQ(0, controller->GetLineCount());
}

TEST_F(AutofillSuggestionControllerTest, HidingClearsPreview) {
  EXPECT_CALL(manager().external_delegate(), ClearPreviewedForm());
  EXPECT_CALL(manager().external_delegate(), OnSuggestionsHidden());
  client().popup_controller(manager()).DoHide();
}

TEST_F(AutofillSuggestionControllerTest, DontHideWhenWaitingForData) {
  client().popup_controller(manager()).PinView();
  EXPECT_CALL(*client().popup_view(), Hide).Times(0);

  // Hide() will not work for stale data or when focusing native UI.
  client().popup_controller(manager()).DoHide(
      SuggestionHidingReason::kStaleData);
  client().popup_controller(manager()).DoHide(
      SuggestionHidingReason::kEndEditing);

  // Check the expectations now since TearDown will perform a successful hide.
  Mock::VerifyAndClearExpectations(&manager().external_delegate());
  Mock::VerifyAndClearExpectations(client().popup_view());
}

TEST_F(AutofillSuggestionControllerTest, ShouldReportHidingPopupReason) {
  base::HistogramTester histogram_tester;
  ShowSuggestions(manager(), {Suggestion(u"Autocomplete entry",
                                         SuggestionType::kAutocompleteEntry)});
  client().popup_controller(manager()).DoHide(SuggestionHidingReason::kTabGone);
  ShowSuggestions(
      manager(), {Suggestion(u"Address entry", SuggestionType::kAddressEntry)});
  client().popup_controller(manager()).DoHide(SuggestionHidingReason::kTabGone);
  ShowSuggestions(manager(), {Suggestion(u"Credit Card entry",
                                         SuggestionType::kCreditCardEntry)});
  client().popup_controller(manager()).DoHide(SuggestionHidingReason::kTabGone);

  histogram_tester.ExpectTotalCount("Autofill.PopupHidingReason", 3);
  histogram_tester.ExpectBucketCount("Autofill.PopupHidingReason",
                                     SuggestionHidingReason::kTabGone, 3);
  histogram_tester.ExpectBucketCount("Autofill.PopupHidingReason.Autocomplete",
                                     SuggestionHidingReason::kTabGone, 1);
  histogram_tester.ExpectBucketCount("Autofill.PopupHidingReason.Address",
                                     SuggestionHidingReason::kTabGone, 1);
  histogram_tester.ExpectBucketCount("Autofill.PopupHidingReason.CreditCard",
                                     SuggestionHidingReason::kTabGone, 1);
}

// Tests that when a picture-in-picture window is initialized, there is a call
// to the popup view to check if the autofill popup bounds overlap with the
// picture-in-picture window.
// TODO(crbug.com/40280362): Implement PIP overlap checks on Android.
#if !BUILDFLAG(IS_ANDROID)
TEST_F(AutofillSuggestionControllerTest, CheckBoundsOverlapWithPictureInPicture) {
  ShowSuggestions(manager(), {SuggestionType::kAddressEntry});
  PictureInPictureWindowManager* picture_in_picture_window_manager =
      PictureInPictureWindowManager::GetInstance();
  EXPECT_CALL(*client().popup_view(), OverlapsWithPictureInPictureWindow);
  picture_in_picture_window_manager->NotifyObserversOnEnterPictureInPicture();
}
#endif

// Tests that a change to a text field does not hide a popup with an
// Autocomplete suggestion.
TEST_F(AutofillSuggestionControllerTest,
       DoeNotHideOnFieldChangeForNonComposeEntries) {
  ShowSuggestions(manager(), {SuggestionType::kAutocompleteEntry});
  EXPECT_CALL(client().popup_controller(manager()), Hide).Times(0);
  manager().NotifyObservers(
      &AutofillManager::Observer::OnBeforeTextFieldDidChange, FormGlobalId(),
      FieldGlobalId());
  Mock::VerifyAndClearExpectations(&client().popup_controller(manager()));
}

class AutofillSuggestionControllerTestHidingLogic
    : public AutofillSuggestionControllerTest {
 public:
  void SetUp() override {
    AutofillSuggestionControllerTest::SetUp();
    sub_frame_ = CreateAndNavigateChildFrame(
                     main_frame(), GURL("https://bar.com"), "sub_frame")
                     ->GetWeakDocumentPtr();
  }

  Manager& sub_manager() { return manager(sub_frame()); }

  content::RenderFrameHost* sub_frame() {
    return sub_frame_.AsRenderFrameHostIfValid();
  }

 private:
  content::WeakDocumentPtr sub_frame_;
};

// Tests that if the popup is shown in the *main frame*, destruction of the
// *sub frame* does not hide the popup.
TEST_F(AutofillSuggestionControllerTestHidingLogic,
       KeepOpenInMainFrameOnSubFrameDestruction) {
  ShowSuggestions(manager(), {SuggestionType::kAddressEntry});
  test::GenerateTestAutofillPopup(&manager().external_delegate());
  EXPECT_CALL(client().popup_controller(manager()), Hide).Times(0);
  content::RenderFrameHostTester::For(sub_frame())->Detach();
  // Verify and clear before TearDown() closes the popup.
  Mock::VerifyAndClearExpectations(&client().popup_controller(manager()));
}

// Tests that if the popup is shown in the *main frame*, a navigation in the
// *sub frame* does not hide the popup.
TEST_F(AutofillSuggestionControllerTestHidingLogic,
       KeepOpenInMainFrameOnSubFrameNavigation) {
  ShowSuggestions(manager(), {SuggestionType::kAddressEntry});
  test::GenerateTestAutofillPopup(&manager().external_delegate());
  EXPECT_CALL(client().popup_controller(manager()), Hide).Times(0);
  NavigateAndCommitFrame(sub_frame(), GURL("https://bar.com/"));
  // Verify and clear before TearDown() closes the popup.
  Mock::VerifyAndClearExpectations(&client().popup_controller(manager()));
}

// Tests that if the popup is shown, destruction of the WebContents hides the
// popup.
TEST_F(AutofillSuggestionControllerTestHidingLogic, HideOnWebContentsDestroyed) {
  ShowSuggestions(manager(), {SuggestionType::kAddressEntry});
  test::GenerateTestAutofillPopup(&manager().external_delegate());
  EXPECT_CALL(client().popup_controller(manager()),
              Hide(SuggestionHidingReason::kRendererEvent));
  DeleteContents();
}

// Tests that if the popup is shown in the *main frame*, destruction of the
// *main frame* hides the popup.
TEST_F(AutofillSuggestionControllerTestHidingLogic, HideInMainFrameOnDestruction) {
  ShowSuggestions(manager(), {SuggestionType::kAddressEntry});
  test::GenerateTestAutofillPopup(&manager().external_delegate());
  EXPECT_CALL(client().popup_controller(manager()),
              Hide(SuggestionHidingReason::kRendererEvent));
}

// Tests that if the popup is shown in the *sub frame*, destruction of the
// *sub frame* hides the popup.
TEST_F(AutofillSuggestionControllerTestHidingLogic, HideInSubFrameOnDestruction) {
  ShowSuggestions(sub_manager(), {SuggestionType::kAddressEntry});
  test::GenerateTestAutofillPopup(&sub_manager().external_delegate());
  EXPECT_CALL(client().popup_controller(sub_manager()),
              Hide(SuggestionHidingReason::kRendererEvent));
  content::RenderFrameHostTester::For(sub_frame())->Detach();
  // Verify and clear before TearDown() closes the popup.
  Mock::VerifyAndClearExpectations(&client().popup_controller(sub_manager()));
}

// Tests that if the popup is shown in the *main frame*, a navigation in the
// *main frame* hides the popup.
TEST_F(AutofillSuggestionControllerTestHidingLogic,
       HideInMainFrameOnMainFrameNavigation) {
  ShowSuggestions(manager(), {SuggestionType::kAddressEntry});
  test::GenerateTestAutofillPopup(&manager().external_delegate());
  // The navigation generates a PrimaryMainFrameWasResized callback.
  EXPECT_CALL(client().popup_controller(manager()),
              Hide(SuggestionHidingReason::kWidgetChanged));
  NavigateAndCommitFrame(main_frame(), GURL("https://bar.com/"));
  // Verify and clear before TearDown() closes the popup.
  Mock::VerifyAndClearExpectations(&client().popup_controller(manager()));
}

// Tests that if the popup is shown in the *sub frame*, a navigation in the
// *sub frame* hides the popup.
TEST_F(AutofillSuggestionControllerTestHidingLogic,
       HideInSubFrameOnSubFrameNavigation) {
  ShowSuggestions(sub_manager(), {SuggestionType::kAddressEntry});
  test::GenerateTestAutofillPopup(&sub_manager().external_delegate());
  if (sub_frame()->ShouldChangeRenderFrameHostOnSameSiteNavigation()) {
    // If the RenderFrameHost changes, a RenderFrameDeleted will fire first.
    EXPECT_CALL(client().popup_controller(sub_manager()),
                Hide(SuggestionHidingReason::kRendererEvent));
  } else {
    EXPECT_CALL(client().popup_controller(sub_manager()),
                Hide(SuggestionHidingReason::kNavigation));
  }
  NavigateAndCommitFrame(sub_frame(), GURL("https://bar.com/"));
  // Verify and clear before TearDown() closes the popup.
  Mock::VerifyAndClearExpectations(&client().popup_controller(sub_manager()));
}

// Tests that if the popup is shown in the *sub frame*, a navigation in the
// *main frame* hides the popup.
//
// TODO(crbug.com/41492848): This test only makes little sense: with BFcache,
// the navigation doesn't destroy the `sub_frame()` and thus we wouldn't hide
// the popup. What hides the popup in reality is
// AutofillExternalDelegate::DidEndTextFieldEditing().
TEST_F(AutofillSuggestionControllerTestHidingLogic,
       HideInSubFrameOnMainFrameNavigation) {
  ShowSuggestions(sub_manager(), {SuggestionType::kAddressEntry});
  test::GenerateTestAutofillPopup(&sub_manager().external_delegate());
  EXPECT_CALL(client().popup_controller(sub_manager()),
              Hide(SuggestionHidingReason::kWidgetChanged));
  NavigateAndCommitFrame(main_frame(), GURL("https://bar.com/"));
}

}  // namespace
}  // namespace autofill
