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
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/ui/autofill/autofill_popup_controller_impl.h"
#include "chrome/browser/ui/autofill/autofill_popup_controller_test_base.h"
#include "chrome/browser/ui/autofill/autofill_popup_view.h"
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
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "components/autofill/core/browser/ui/autofill_popup_delegate.h"
#include "components/autofill/core/browser/ui/popup_hiding_reasons.h"
#include "components/autofill/core/browser/ui/popup_item_ids.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/autofill/core/common/aliases.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/video_picture_in_picture_window_controller.h"
#include "content/public/browser/weak_document_ptr.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/input/native_web_keyboard_event.h"
#include "content/public/test/navigation_simulator.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/text_utils.h"

namespace autofill {
namespace {

using base::ASCIIToUTF16;
using base::WeakPtr;
using ::testing::_;
using ::testing::AtLeast;
using ::testing::Eq;
using ::testing::Field;
using ::testing::Invoke;
using ::testing::Mock;
using ::testing::Optional;
using ::testing::Return;

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

}  // namespace

using AutofillPopupControllerTest = AutofillPopupControllerTestBase<>;

TEST_F(AutofillPopupControllerTest, RemoveSuggestion) {
  ShowSuggestions(manager(),
                  {PopupItemId::kAddressEntry, PopupItemId::kAddressEntry,
                   PopupItemId::kAutofillOptions});

  // Generate a popup, so it can be hidden later. It doesn't matter what the
  // external_delegate thinks is being shown in the process, since we are just
  // testing the popup here.
  test::GenerateTestAutofillPopup(&manager().external_delegate());
  EXPECT_CALL(manager().external_delegate(),
              RemoveSuggestion(Field(&Suggestion::popup_item_id,
                                     PopupItemId::kAddressEntry)))
      .WillRepeatedly(Return(true));

  // Remove the first entry. The popup should be redrawn since its size has
  // changed.
  EXPECT_CALL(client().popup_view(), OnSuggestionsChanged());
  EXPECT_TRUE(client().popup_controller(manager()).RemoveSuggestion(
      0,
      AutofillMetrics::SingleEntryRemovalMethod::kKeyboardShiftDeletePressed));
  Mock::VerifyAndClearExpectations(&client().popup_view());

  // Remove the next entry. The popup should then be hidden since there are
  // no Autofill entries left.
  EXPECT_CALL(client().popup_controller(manager()),
              Hide(PopupHidingReason::kNoSuggestions));
  EXPECT_TRUE(client().popup_controller(manager()).RemoveSuggestion(
      0,
      AutofillMetrics::SingleEntryRemovalMethod::kKeyboardShiftDeletePressed));
}

// Regression test for (crbug.com/1513574): Showing an Autofill Compose
// suggestion twice does not crash.
TEST_F(AutofillPopupControllerTest, ShowTwice) {
  ShowSuggestions(manager(),
                  {Suggestion(u"Help me write", PopupItemId::kCompose)});
  ShowSuggestions(manager(),
                  {Suggestion(u"Help me write", PopupItemId::kCompose)});
}

TEST_F(AutofillPopupControllerTest, RemoveAutocompleteSuggestion_AnnounceText) {
  ShowSuggestions(manager(),
                  {Suggestion(u"main text", PopupItemId::kAutocompleteEntry)});
  test::GenerateTestAutofillPopup(&manager().external_delegate());

  EXPECT_CALL(manager().external_delegate(),
              RemoveSuggestion(Field(&Suggestion::popup_item_id,
                                     PopupItemId::kAutocompleteEntry)))
      .WillOnce(Return(true));
  EXPECT_CALL(client().popup_view(),
              AxAnnounce(Eq(u"Entry main text has been deleted")));
  EXPECT_TRUE(client().popup_controller(manager()).RemoveSuggestion(
      0,
      AutofillMetrics::SingleEntryRemovalMethod::kKeyboardShiftDeletePressed));
}

TEST_F(AutofillPopupControllerTest,
       RemoveAutocompleteSuggestion_IgnoresClickOutsideCheck) {
  ShowSuggestions(manager(), {PopupItemId::kAutocompleteEntry,
                              PopupItemId::kAutocompleteEntry});

  // Generate a popup, so it can be hidden later. It doesn't matter what the
  // external_delegate thinks is being shown in the process, since we are just
  // testing the popup here.
  test::GenerateTestAutofillPopup(&manager().external_delegate());

  EXPECT_CALL(manager().external_delegate(),
              RemoveSuggestion(Field(&Suggestion::popup_item_id,
                                     PopupItemId::kAutocompleteEntry)))
      .WillOnce(Return(true));
  // Remove the first entry. The popup should be redrawn since its size has
  // changed.
  EXPECT_CALL(client().popup_view(), OnSuggestionsChanged());
  EXPECT_TRUE(client().popup_controller(manager()).RemoveSuggestion(
      0,
      AutofillMetrics::SingleEntryRemovalMethod::kKeyboardShiftDeletePressed));
  Mock::VerifyAndClearExpectations(&client().popup_view());

  EXPECT_TRUE(client()
                  .popup_controller(manager())
                  .ShouldIgnoreMouseObservedOutsideItemBoundsCheck());
}

TEST_F(AutofillPopupControllerTest,
       RemoveAutocompleteSuggestion_NoMetricsEmittedOnFail) {
  base::HistogramTester histogram_tester;
  ShowSuggestions(manager(), {PopupItemId::kAutocompleteEntry});
  test::GenerateTestAutofillPopup(&manager().external_delegate());
  EXPECT_CALL(manager().external_delegate(),
              RemoveSuggestion(Field(&Suggestion::popup_item_id,
                                     PopupItemId::kAutocompleteEntry)))
      .WillOnce(Return(false));

  EXPECT_FALSE(client().popup_controller(manager()).RemoveSuggestion(
      0,
      AutofillMetrics::SingleEntryRemovalMethod::kKeyboardShiftDeletePressed));
  histogram_tester.ExpectUniqueSample(
      "Autofill.Autocomplete.SingleEntryRemovalMethod",
      AutofillMetrics::SingleEntryRemovalMethod::kKeyboardShiftDeletePressed,
      0);
  histogram_tester.ExpectUniqueSample(
      "Autocomplete.Events2",
      AutofillMetrics::AutocompleteEvent::AUTOCOMPLETE_SUGGESTION_DELETED, 0);
}

TEST_F(AutofillPopupControllerTest,
       RemoveAutocompleteSuggestion_MetricsEmittedOnSuccess) {
  base::HistogramTester histogram_tester;
  ShowSuggestions(manager(), {PopupItemId::kAutocompleteEntry});
  test::GenerateTestAutofillPopup(&manager().external_delegate());
  EXPECT_CALL(manager().external_delegate(),
              RemoveSuggestion(Field(&Suggestion::popup_item_id,
                                     PopupItemId::kAutocompleteEntry)))
      .WillOnce(Return(true));

  EXPECT_TRUE(client().popup_controller(manager()).RemoveSuggestion(
      0,
      AutofillMetrics::SingleEntryRemovalMethod::kKeyboardShiftDeletePressed));
  histogram_tester.ExpectUniqueSample(
      "Autofill.Autocomplete.SingleEntryRemovalMethod",
      AutofillMetrics::SingleEntryRemovalMethod::kKeyboardShiftDeletePressed,
      1);
  histogram_tester.ExpectUniqueSample(
      "Autocomplete.Events2",
      AutofillMetrics::AutocompleteEvent::AUTOCOMPLETE_SUGGESTION_DELETED, 1);
  // Also no autofill metrics are emitted.
  histogram_tester.ExpectUniqueSample("Autofill.ProfileDeleted.Popup", 1, 0);
  histogram_tester.ExpectUniqueSample(
      "Autofill.ProfileDeleted.KeyboardAccessory", 1, 0);
  histogram_tester.ExpectUniqueSample("Autofill.ProfileDeleted.Any", 1, 0);
}

TEST_F(AutofillPopupControllerTest,
       RemoveAddressSuggestion_ShiftDelete_NoMetricsEmittedOnFail) {
  base::HistogramTester histogram_tester;
  ShowSuggestions(manager(), {PopupItemId::kAddressEntry});
  test::GenerateTestAutofillPopup(&manager().external_delegate());
  EXPECT_CALL(manager().external_delegate(),
              RemoveSuggestion(Field(&Suggestion::popup_item_id,
                                     PopupItemId::kAddressEntry)))
      .WillOnce(Return(false));

  EXPECT_FALSE(client().popup_controller(manager()).RemoveSuggestion(
      0,
      AutofillMetrics::SingleEntryRemovalMethod::kKeyboardShiftDeletePressed));
  histogram_tester.ExpectUniqueSample("Autofill.ProfileDeleted.Popup", 1, 0);
  histogram_tester.ExpectUniqueSample(
      "Autofill.ProfileDeleted.KeyboardAccessory", 1, 0);
  histogram_tester.ExpectUniqueSample("Autofill.ProfileDeleted.Any", 1, 0);
}

TEST_F(AutofillPopupControllerTest,
       RemoveAddressSuggestion_ShiftDelete_MetricsEmittedOnSuccess) {
  base::HistogramTester histogram_tester;
  ShowSuggestions(manager(), {PopupItemId::kAddressEntry});
  test::GenerateTestAutofillPopup(&manager().external_delegate());
  EXPECT_CALL(manager().external_delegate(),
              RemoveSuggestion(Field(&Suggestion::popup_item_id,
                                     PopupItemId::kAddressEntry)))
      .WillOnce(Return(true));

  EXPECT_TRUE(client().popup_controller(manager()).RemoveSuggestion(
      0,
      AutofillMetrics::SingleEntryRemovalMethod::kKeyboardShiftDeletePressed));
  histogram_tester.ExpectUniqueSample("Autofill.ProfileDeleted.Popup", 1, 1);
  histogram_tester.ExpectUniqueSample("Autofill.ProfileDeleted.Any", 1, 1);
  // Also no autocomplete or keyboard accessory metrics are emitted.
  histogram_tester.ExpectUniqueSample(
      "Autofill.ProfileDeleted.KeyboardAccessory", 1, 0);
  histogram_tester.ExpectUniqueSample(
      "Autofill.Autocomplete.SingleEntryRemovalMethod",
      AutofillMetrics::SingleEntryRemovalMethod::kKeyboardShiftDeletePressed,
      0);
  histogram_tester.ExpectUniqueSample(
      "Autocomplete.Events2",
      AutofillMetrics::AutocompleteEvent::AUTOCOMPLETE_SUGGESTION_DELETED, 0);
}

TEST_F(AutofillPopupControllerTest,
       RemoveAddressSuggestion_KeyboardAccessory_NoMetricsEmittedOnFail) {
  base::HistogramTester histogram_tester;
  ShowSuggestions(manager(), {PopupItemId::kAddressEntry});
  test::GenerateTestAutofillPopup(&manager().external_delegate());
  EXPECT_CALL(manager().external_delegate(),
              RemoveSuggestion(Field(&Suggestion::popup_item_id,
                                     PopupItemId::kAddressEntry)))
      .WillOnce(Return(false));

  EXPECT_FALSE(client().popup_controller(manager()).RemoveSuggestion(
      0, AutofillMetrics::SingleEntryRemovalMethod::kKeyboardAccessory));
  histogram_tester.ExpectUniqueSample("Autofill.ProfileDeleted.Popup", 1, 0);
  histogram_tester.ExpectUniqueSample(
      "Autofill.ProfileDeleted.KeyboardAccessory", 1, 0);
  histogram_tester.ExpectUniqueSample("Autofill.ProfileDeleted.Any", 1, 0);
}

TEST_F(AutofillPopupControllerTest,
       RemoveAddressSuggestion_KeyboardAccessory_MetricsEmittedOnSuccess) {
  base::HistogramTester histogram_tester;
  ShowSuggestions(manager(), {PopupItemId::kAddressEntry});
  test::GenerateTestAutofillPopup(&manager().external_delegate());
  EXPECT_CALL(manager().external_delegate(),
              RemoveSuggestion(Field(&Suggestion::popup_item_id,
                                     PopupItemId::kAddressEntry)))
      .WillOnce(Return(true));

  EXPECT_TRUE(client().popup_controller(manager()).RemoveSuggestion(
      0, AutofillMetrics::SingleEntryRemovalMethod::kKeyboardAccessory));
  histogram_tester.ExpectUniqueSample(
      "Autofill.ProfileDeleted.KeyboardAccessory", 1, 1);
  histogram_tester.ExpectUniqueSample("Autofill.ProfileDeleted.Any", 1, 1);
  // Also no autocomplete or shift+delete metrics are emitted.
  histogram_tester.ExpectUniqueSample("Autofill.ProfileDeleted.Popup", 1, 0);
  histogram_tester.ExpectUniqueSample(
      "Autofill.Autocomplete.SingleEntryRemovalMethod",
      AutofillMetrics::SingleEntryRemovalMethod::kKeyboardShiftDeletePressed,
      0);
  histogram_tester.ExpectUniqueSample(
      "Autocomplete.Events2",
      AutofillMetrics::AutocompleteEvent::AUTOCOMPLETE_SUGGESTION_DELETED, 0);
}

TEST_F(AutofillPopupControllerTest,
       RemoveCreditCardSuggestion_NoMetricsEmitted) {
  base::HistogramTester histogram_tester;
  ShowSuggestions(manager(), {PopupItemId::kCreditCardEntry});
  test::GenerateTestAutofillPopup(&manager().external_delegate());
  EXPECT_CALL(manager().external_delegate(),
              RemoveSuggestion(Field(&Suggestion::popup_item_id,
                                     PopupItemId::kCreditCardEntry)))
      .WillOnce(Return(true));

  EXPECT_TRUE(client().popup_controller(manager()).RemoveSuggestion(
      0,
      AutofillMetrics::SingleEntryRemovalMethod::kKeyboardShiftDeletePressed));
  histogram_tester.ExpectUniqueSample(
      "Autofill.Autocomplete.SingleEntryRemovalMethod",
      AutofillMetrics::SingleEntryRemovalMethod::kKeyboardShiftDeletePressed,
      0);
  histogram_tester.ExpectUniqueSample(
      "Autocomplete.Events2",
      AutofillMetrics::AutocompleteEvent::AUTOCOMPLETE_SUGGESTION_DELETED, 0);
  histogram_tester.ExpectUniqueSample("Autofill.ProfileDeleted.Popup", 1, 0);
  histogram_tester.ExpectUniqueSample(
      "Autofill.ProfileDeleted.KeyboardAccessory", 1, 0);
  histogram_tester.ExpectUniqueSample("Autofill.ProfileDeleted.Any", 1, 0);
}

TEST_F(AutofillPopupControllerTest, UpdateDataListValues) {
  ShowSuggestions(manager(), {PopupItemId::kAddressEntry});
  std::vector<SelectOption> options = {
      {.value = u"data list value 1", .content = u"data list label 1"}};
  client().popup_controller(manager()).UpdateDataListValues(options);

  ASSERT_EQ(3, client().popup_controller(manager()).GetLineCount());

  Suggestion result0 = client().popup_controller(manager()).GetSuggestionAt(0);
  EXPECT_EQ(options[0].value, result0.main_text.value);
  EXPECT_EQ(options[0].value,
            client().popup_controller(manager()).GetSuggestionMainTextAt(0));
  ASSERT_EQ(1u, result0.labels.size());
  ASSERT_EQ(1u, result0.labels[0].size());
  EXPECT_EQ(options[0].content, result0.labels[0][0].value);
  EXPECT_EQ(std::u16string(), result0.additional_label);
  EXPECT_EQ(options[0].content, client()
                                    .popup_controller(manager())
                                    .GetSuggestionLabelsAt(0)[0][0]
                                    .value);
  EXPECT_EQ(PopupItemId::kDatalistEntry, result0.popup_item_id);

  Suggestion result1 = client().popup_controller(manager()).GetSuggestionAt(1);
  EXPECT_EQ(std::u16string(), result1.main_text.value);
  EXPECT_TRUE(result1.labels.empty());
  EXPECT_EQ(std::u16string(), result1.additional_label);
  EXPECT_EQ(PopupItemId::kSeparator, result1.popup_item_id);

  Suggestion result2 = client().popup_controller(manager()).GetSuggestionAt(2);
  EXPECT_EQ(std::u16string(), result2.main_text.value);
  EXPECT_TRUE(result2.labels.empty());
  EXPECT_EQ(std::u16string(), result2.additional_label);
  EXPECT_EQ(PopupItemId::kAddressEntry, result2.popup_item_id);

  // Add two data list entries (which should replace the current one).
  options.push_back(
      {.value = u"data list value 1", .content = u"data list label 1"});
  client().popup_controller(manager()).UpdateDataListValues(options);
  ASSERT_EQ(4, client().popup_controller(manager()).GetLineCount());

  // Original one first, followed by new one, then separator.
  EXPECT_EQ(
      options[0].value,
      client().popup_controller(manager()).GetSuggestionAt(0).main_text.value);
  EXPECT_EQ(options[0].value,
            client().popup_controller(manager()).GetSuggestionMainTextAt(0));
  ASSERT_EQ(
      1u,
      client().popup_controller(manager()).GetSuggestionAt(0).labels.size());
  ASSERT_EQ(
      1u,
      client().popup_controller(manager()).GetSuggestionAt(0).labels[0].size());
  EXPECT_EQ(options[0].content, client()
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
  EXPECT_EQ(options[1].value,
            client().popup_controller(manager()).GetSuggestionMainTextAt(1));
  ASSERT_EQ(
      1u,
      client().popup_controller(manager()).GetSuggestionAt(1).labels.size());
  ASSERT_EQ(
      1u,
      client().popup_controller(manager()).GetSuggestionAt(1).labels[0].size());
  EXPECT_EQ(options[1].content, client()
                                    .popup_controller(manager())
                                    .GetSuggestionAt(1)
                                    .labels[0][0]
                                    .value);
  EXPECT_EQ(
      std::u16string(),
      client().popup_controller(manager()).GetSuggestionAt(1).additional_label);
  EXPECT_EQ(
      PopupItemId::kSeparator,
      client().popup_controller(manager()).GetSuggestionAt(2).popup_item_id);

  // Clear all data list values.
  options.clear();
  client().popup_controller(manager()).UpdateDataListValues(options);

  ASSERT_EQ(1, client().popup_controller(manager()).GetLineCount());
  EXPECT_EQ(
      PopupItemId::kAddressEntry,
      client().popup_controller(manager()).GetSuggestionAt(0).popup_item_id);
}

TEST_F(AutofillPopupControllerTest, PopupsWithOnlyDataLists) {
  // Create the popup with a single datalist element.
  ShowSuggestions(manager(), {PopupItemId::kDatalistEntry});

  // Replace the datalist element with a new one.
  std::vector<SelectOption> options = {
      {.value = u"data list value 1", .content = u"data list label 1"}};
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
  EXPECT_EQ(options[0].content, client()
                                    .popup_controller(manager())
                                    .GetSuggestionAt(0)
                                    .labels[0][0]
                                    .value);
  EXPECT_EQ(
      std::u16string(),
      client().popup_controller(manager()).GetSuggestionAt(0).additional_label);
  EXPECT_EQ(
      PopupItemId::kDatalistEntry,
      client().popup_controller(manager()).GetSuggestionAt(0).popup_item_id);

  // Clear datalist values and check that the popup becomes hidden.
  EXPECT_CALL(client().popup_controller(manager()),
              Hide(PopupHidingReason::kNoSuggestions));
  options.clear();
  client().popup_controller(manager()).UpdateDataListValues(options);
}

TEST_F(AutofillPopupControllerTest, GetOrCreate) {
  auto create_controller = [&](gfx::RectF bounds) {
    return AutofillPopupController::GetOrCreate(
        client().popup_controller(manager()).GetWeakPtr(),
        manager().external_delegate().GetWeakPtrForTest(), nullptr,
        PopupControllerCommon(std::move(bounds), base::i18n::UNKNOWN_DIRECTION,
                              nullptr),
        /*form_control_ax_id=*/0);
  };
  WeakPtr<AutofillPopupController> controller = create_controller(gfx::RectF());
  EXPECT_TRUE(controller);

  controller->Hide(PopupHidingReason::kViewDestroyed);
  EXPECT_FALSE(controller);

  controller = create_controller(gfx::RectF());
  EXPECT_TRUE(controller);

  WeakPtr<AutofillPopupController> controller2 =
      create_controller(gfx::RectF());
  EXPECT_EQ(controller.get(), controller2.get());

  controller->Hide(PopupHidingReason::kViewDestroyed);
  EXPECT_FALSE(controller);
  EXPECT_FALSE(controller2);

  EXPECT_CALL(client().popup_controller(manager()),
              Hide(PopupHidingReason::kViewDestroyed));
  gfx::RectF bounds(0.f, 0.f, 1.f, 2.f);
  base::WeakPtr<AutofillPopupController> controller3 =
      create_controller(bounds);
  EXPECT_EQ(&client().popup_controller(manager()), controller3.get());
  EXPECT_EQ(bounds, static_cast<AutofillPopupController*>(controller3.get())
                        ->element_bounds());
  controller3->Hide(PopupHidingReason::kViewDestroyed);

  client().popup_controller(manager()).DoHide();

  const base::WeakPtr<AutofillPopupController> controller4 =
      create_controller(bounds);
  EXPECT_EQ(&client().popup_controller(manager()), controller4.get());
  EXPECT_EQ(bounds,
            static_cast<const AutofillPopupController*>(controller4.get())
                ->element_bounds());

  client().popup_controller(manager()).DoHide();
}

TEST_F(AutofillPopupControllerTest, ProperlyResetController) {
  ShowSuggestions(manager(), {PopupItemId::kAutocompleteEntry,
                              PopupItemId::kAutocompleteEntry});

  // Now show a new popup with the same controller, but with fewer items.
  WeakPtr<AutofillPopupController> controller =
      AutofillPopupController::GetOrCreate(
          client().popup_controller(manager()).GetWeakPtr(),
          manager().external_delegate().GetWeakPtrForTest(), nullptr,
          PopupControllerCommon(gfx::RectF(), base::i18n::UNKNOWN_DIRECTION,
                                nullptr),
          /*form_control_ax_id=*/0);
  EXPECT_EQ(0, controller->GetLineCount());
}

TEST_F(AutofillPopupControllerTest, UnselectingClearsPreview) {
  EXPECT_CALL(manager().external_delegate(), ClearPreviewedForm());
  client().popup_controller(manager()).UnselectSuggestion();
}

TEST_F(AutofillPopupControllerTest, HidingClearsPreview) {
  EXPECT_CALL(manager().external_delegate(), ClearPreviewedForm());
  EXPECT_CALL(manager().external_delegate(), OnPopupHidden());
  client().popup_controller(manager()).DoHide();
}

TEST_F(AutofillPopupControllerTest, DontHideWhenWaitingForData) {
  EXPECT_CALL(client().popup_view(), Hide).Times(0);
  client().popup_controller(manager()).PinView();

  // Hide() will not work for stale data or when focusing native UI.
  client().popup_controller(manager()).DoHide(PopupHidingReason::kStaleData);
  client().popup_controller(manager()).DoHide(PopupHidingReason::kEndEditing);

  // Check the expectations now since TearDown will perform a successful hide.
  Mock::VerifyAndClearExpectations(&manager().external_delegate());
  Mock::VerifyAndClearExpectations(&client().popup_view());
}

TEST_F(AutofillPopupControllerTest, ShouldReportHidingPopupReason) {
  base::HistogramTester histogram_tester;
  client().popup_controller(manager()).DoHide(PopupHidingReason::kTabGone);
  histogram_tester.ExpectTotalCount("Autofill.PopupHidingReason", 1);
  histogram_tester.ExpectBucketCount("Autofill.PopupHidingReason",
                                     PopupHidingReason::kTabGone, 1);
}

// This is a regression test for crbug.com/521133 to ensure that we don't crash
// when suggestions updates race with user selections.
TEST_F(AutofillPopupControllerTest, SelectInvalidSuggestion) {
  ShowSuggestions(manager(), {PopupItemId::kAddressEntry});

  EXPECT_CALL(manager().external_delegate(), DidAcceptSuggestion).Times(0);

  // The following should not crash:
  client().popup_controller(manager()).AcceptSuggestion(
      /*index=*/1);  // Out of bounds!
}

TEST_F(AutofillPopupControllerTest, AcceptSuggestionRespectsTimeout) {
  base::HistogramTester histogram_tester;
  ShowSuggestions(manager(), {PopupItemId::kAddressEntry});

  // Calls before the threshold are ignored.
  EXPECT_CALL(manager().external_delegate(), DidAcceptSuggestion).Times(0);
  client().popup_controller(manager()).AcceptSuggestion(0);
  task_environment()->FastForwardBy(base::Milliseconds(100));
  client().popup_controller(manager()).AcceptSuggestion(/*index=*/0);

  EXPECT_CALL(manager().external_delegate(), DidAcceptSuggestion);
  task_environment()->FastForwardBy(base::Milliseconds(400));
  client().popup_controller(manager()).AcceptSuggestion(/*index=*/0);

  histogram_tester.ExpectTotalCount(
      "Autofill.Popup.AcceptanceDelayThresholdNotMet", 2);
}

TEST_F(AutofillPopupControllerTest,
       AcceptSuggestionTimeoutIsUpdatedOnPopupMove) {
  base::HistogramTester histogram_tester;
  ShowSuggestions(manager(), {PopupItemId::kAddressEntry});

  // Calls before the threshold are ignored.
  EXPECT_CALL(manager().external_delegate(), DidAcceptSuggestion).Times(0);
  client().popup_controller(manager()).AcceptSuggestion(/*index=*/0);
  task_environment()->FastForwardBy(base::Milliseconds(100));
  client().popup_controller(manager()).AcceptSuggestion(/*index=*/0);

  histogram_tester.ExpectTotalCount(
      "Autofill.Popup.AcceptanceDelayThresholdNotMet", 2);
  task_environment()->FastForwardBy(base::Milliseconds(400));
  // Show the suggestions again (simulating, e.g., a click somewhere slightly
  // different).
  ShowSuggestions(manager(), {PopupItemId::kAddressEntry});

  EXPECT_CALL(manager().external_delegate(), DidAcceptSuggestion).Times(0);
  client().popup_controller(manager()).AcceptSuggestion(/*index=*/0);
  histogram_tester.ExpectTotalCount(
      "Autofill.Popup.AcceptanceDelayThresholdNotMet", 3);

  EXPECT_CALL(manager().external_delegate(), DidAcceptSuggestion);
  // After waiting, suggestions are accepted again.
  task_environment()->FastForwardBy(base::Milliseconds(500));
  client().popup_controller(manager()).AcceptSuggestion(/*index=*/0);
  histogram_tester.ExpectTotalCount(
      "Autofill.Popup.AcceptanceDelayThresholdNotMet", 3);
}

// Tests that when a picture-in-picture window is initialized, there is a call
// to the popup view to check if the autofill popup bounds overlap with the
// picture-in-picture window.
TEST_F(AutofillPopupControllerTest, CheckBoundsOverlapWithPictureInPicture) {
  ShowSuggestions(manager(), {PopupItemId::kAddressEntry});
  PictureInPictureWindowManager* picture_in_picture_window_manager =
      PictureInPictureWindowManager::GetInstance();
  EXPECT_CALL(client().popup_view(), OverlapsWithPictureInPictureWindow);
  picture_in_picture_window_manager->NotifyObserversOnEnterPictureInPicture();
}

TEST_F(AutofillPopupControllerTest,
       GetRemovalConfirmationText_UnrelatedPopupItemId) {
  std::u16string title;
  std::u16string body;
  ShowSuggestions(
      manager(),
      {Suggestion(u"Entry", PopupItemId::kAddressFieldByFieldFilling)});

  EXPECT_FALSE(client().popup_controller(manager()).GetRemovalConfirmationText(
      0, &title, &body));
}

TEST_F(AutofillPopupControllerTest,
       GetRemovalConfirmationText_InvalidUniqueId) {
  std::u16string title;
  std::u16string body;
  ShowSuggestions(manager(), {test::CreateAutofillSuggestion(
                                 PopupItemId::kAddressFieldByFieldFilling,
                                 u"Entry", Suggestion::Guid("1111"))});

  EXPECT_FALSE(client().popup_controller(manager()).GetRemovalConfirmationText(
      0, &title, &body));
}

TEST_F(AutofillPopupControllerTest, GetRemovalConfirmationText_Autocomplete) {
  std::u16string title;
  std::u16string body;
  ShowSuggestions(manager(), {Suggestion(u"Autocomplete entry",
                                         PopupItemId::kAutocompleteEntry)});

  EXPECT_TRUE(client().popup_controller(manager()).GetRemovalConfirmationText(
      0, &title, &body));
  EXPECT_EQ(title, u"Autocomplete entry");
  EXPECT_EQ(body,
            l10n_util::GetStringUTF16(
                IDS_AUTOFILL_DELETE_AUTOCOMPLETE_SUGGESTION_CONFIRMATION_BODY));
}

TEST_F(AutofillPopupControllerTest,
       GetRemovalConfirmationText_LocalCreditCard) {
  CreditCard local_card = test::GetCreditCard();
  personal_data().AddCreditCard(local_card);

  std::u16string title;
  std::u16string body;
  ShowSuggestions(manager(),
                  {test::CreateAutofillSuggestion(
                      PopupItemId::kCreditCardEntry, u"Local credit card",
                      Suggestion::Guid(local_card.guid()))});

  EXPECT_TRUE(client().popup_controller(manager()).GetRemovalConfirmationText(
      0, &title, &body));
  EXPECT_EQ(title, local_card.CardNameAndLastFourDigits());
  EXPECT_EQ(body,
            l10n_util::GetStringUTF16(
                IDS_AUTOFILL_DELETE_CREDIT_CARD_SUGGESTION_CONFIRMATION_BODY));
}

TEST_F(AutofillPopupControllerTest,
       GetRemovalConfirmationText_ServerCreditCard) {
  CreditCard server_card = test::GetMaskedServerCard();
  personal_data().AddServerCreditCard(server_card);

  std::u16string title;
  std::u16string body;
  ShowSuggestions(manager(),
                  {test::CreateAutofillSuggestion(
                      PopupItemId::kCreditCardEntry, u"Server credit card",
                      Suggestion::Guid(server_card.guid()))});

  EXPECT_FALSE(client().popup_controller(manager()).GetRemovalConfirmationText(
      0, &title, &body));
}

TEST_F(AutofillPopupControllerTest,
       GetRemovalConfirmationText_CompleteAutofillProfile) {
  AutofillProfile complete_profile = test::GetFullProfile();
  personal_data().AddProfile(complete_profile);

  std::u16string title;
  std::u16string body;
  ShowSuggestions(manager(),
                  {test::CreateAutofillSuggestion(
                      PopupItemId::kAddressEntry, u"Complete autofill profile",
                      Suggestion::Guid(complete_profile.guid()))});

  EXPECT_TRUE(client().popup_controller(manager()).GetRemovalConfirmationText(
      0, &title, &body));
  EXPECT_EQ(title, complete_profile.GetRawInfo(ADDRESS_HOME_CITY));
  EXPECT_EQ(body,
            l10n_util::GetStringUTF16(
                IDS_AUTOFILL_DELETE_PROFILE_SUGGESTION_CONFIRMATION_BODY));
}

TEST_F(AutofillPopupControllerTest,
       GetRemovalConfirmationText_AutofillProfile_EmptyCity) {
  AutofillProfile profile = test::GetFullProfile();
  profile.ClearFields({ADDRESS_HOME_CITY});
  personal_data().AddProfile(profile);

  std::u16string title;
  std::u16string body;
  ShowSuggestions(manager(), {test::CreateAutofillSuggestion(
                                 PopupItemId::kAddressEntry,
                                 u"Autofill profile without city",
                                 Suggestion::Guid(profile.guid()))});

  EXPECT_TRUE(client().popup_controller(manager()).GetRemovalConfirmationText(
      0, &title, &body));
  EXPECT_EQ(title, u"Autofill profile without city");
  EXPECT_EQ(body,
            l10n_util::GetStringUTF16(
                IDS_AUTOFILL_DELETE_PROFILE_SUGGESTION_CONFIRMATION_BODY));
}

#if BUILDFLAG(IS_ANDROID)
TEST_F(AutofillPopupControllerTest, AcceptPwdSuggestionInvokesWarningAndroid) {
  base::test::ScopedFeatureList scoped_feature_list(
      password_manager::features::
          kUnifiedPasswordManagerLocalPasswordsMigrationWarning);
  ShowSuggestions(manager(), {PopupItemId::kPasswordEntry});

  // Calls are accepted immediately.
  EXPECT_CALL(manager().external_delegate(), DidAcceptSuggestion).Times(1);
  EXPECT_CALL(client().show_pwd_migration_warning_callback(),
              Run(_, _,
                  password_manager::metrics_util::
                      PasswordMigrationWarningTriggers::kKeyboardAcessoryBar));
  task_environment()->FastForwardBy(base::Milliseconds(500));
  client().popup_controller(manager()).AcceptSuggestion(0);
}

TEST_F(AutofillPopupControllerTest,
       AcceptUsernameSuggestionInvokesWarningAndroid) {
  base::test::ScopedFeatureList scoped_feature_list(
      password_manager::features::
          kUnifiedPasswordManagerLocalPasswordsMigrationWarning);
  ShowSuggestions(manager(), {PopupItemId::kPasswordEntry});

  // Calls are accepted immediately.
  EXPECT_CALL(manager().external_delegate(), DidAcceptSuggestion).Times(1);
  EXPECT_CALL(client().show_pwd_migration_warning_callback(), Run);
  task_environment()->FastForwardBy(base::Milliseconds(500));
  client().popup_controller(manager()).AcceptSuggestion(0);
}

TEST_F(AutofillPopupControllerTest,
       AcceptPwdSuggestionNoWarningIfDisabledAndroid) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      password_manager::features::
          kUnifiedPasswordManagerLocalPasswordsMigrationWarning);
  ShowSuggestions(manager(), {PopupItemId::kPasswordEntry});

  // Calls are accepted immediately.
  EXPECT_CALL(manager().external_delegate(), DidAcceptSuggestion).Times(1);
  EXPECT_CALL(client().show_pwd_migration_warning_callback(), Run).Times(0);
  task_environment()->FastForwardBy(base::Milliseconds(500));
  client().popup_controller(manager()).AcceptSuggestion(0);
}

TEST_F(AutofillPopupControllerTest, AcceptAddressNoPwdWarningAndroid) {
  base::test::ScopedFeatureList scoped_feature_list(
      password_manager::features::
          kUnifiedPasswordManagerLocalPasswordsMigrationWarning);
  ShowSuggestions(manager(), {PopupItemId::kAddressEntry});

  // Calls are accepted immediately.
  EXPECT_CALL(manager().external_delegate(), DidAcceptSuggestion).Times(1);
  EXPECT_CALL(client().show_pwd_migration_warning_callback(), Run).Times(0);
  task_environment()->FastForwardBy(base::Milliseconds(500));
  client().popup_controller(manager()).AcceptSuggestion(0);
}

// When a suggestion is accepted, the popup is hidden inside
// `delegate->DidAcceptSuggestion()`. On Android, some code is still being
// executed after hiding. This test makes sure no use-after-free, null pointer
// dereferencing or other memory violations occur.
TEST_F(AutofillPopupControllerTest, AcceptSuggestionIsMemorySafe) {
  ShowSuggestions(manager(), {PopupItemId::kPasswordEntry});
  task_environment()->FastForwardBy(base::Milliseconds(500));

  EXPECT_CALL(manager().external_delegate(), DidAcceptSuggestion)
      .WillOnce([this]() {
        client().popup_controller(manager()).Hide(
            PopupHidingReason::kAcceptSuggestion);
      });
  client().popup_controller(manager()).AcceptSuggestion(/*index=*/0);
}

#endif  // BUILDFLAG(IS_ANDROID)

// Tests that the popup controller queries the view for its screen location.
TEST_F(AutofillPopupControllerTest, GetPopupScreenLocationCallsView) {
  ShowSuggestions(manager(), {PopupItemId::kCompose});

  using PopupScreenLocation = AutofillClient::PopupScreenLocation;
  constexpr gfx::Rect kSampleRect = gfx::Rect(123, 234);
  EXPECT_CALL(client().popup_view(), GetPopupScreenLocation)
      .WillOnce(Return(PopupScreenLocation{.bounds = kSampleRect}));
  EXPECT_THAT(client().popup_controller(manager()).GetPopupScreenLocation(),
              Optional(Field(&PopupScreenLocation::bounds, kSampleRect)));
}

// Tests that a change to a text field hides a popup with a Compose suggestion.
TEST_F(AutofillPopupControllerTest, HidesOnFieldChangeForComposeEntries) {
  ShowSuggestions(manager(), {PopupItemId::kCompose});
  EXPECT_CALL(client().popup_controller(manager()),
              Hide(PopupHidingReason::kFieldValueChanged));
  manager().NotifyObservers(
      &AutofillManager::Observer::OnBeforeTextFieldDidChange, FormGlobalId(),
      FieldGlobalId());
}

// Tests that a change to a text field does not hide a popup with an
// Autocomplete suggestion.
TEST_F(AutofillPopupControllerTest,
       DoeNotHideOnFieldChangeForNonComposeEntries) {
  ShowSuggestions(manager(), {PopupItemId::kAutocompleteEntry});
  EXPECT_CALL(client().popup_controller(manager()), Hide).Times(0);
  manager().NotifyObservers(
      &AutofillManager::Observer::OnBeforeTextFieldDidChange, FormGlobalId(),
      FieldGlobalId());
  Mock::VerifyAndClearExpectations(&client().popup_controller(manager()));
}

class AutofillPopupControllerTestHidingLogic
    : public AutofillPopupControllerTest {
 public:
  void SetUp() override {
    AutofillPopupControllerTest::SetUp();
    sub_frame_ = CreateAndNavigateChildFrame(
                     main_frame(), GURL("https://bar.com"), "sub_frame")
                     ->GetWeakDocumentPtr();
  }

  TestManager& sub_manager() { return manager(sub_frame()); }

  content::RenderFrameHost* sub_frame() {
    return sub_frame_.AsRenderFrameHostIfValid();
  }

 private:
  content::WeakDocumentPtr sub_frame_;
};

// Tests that if the popup is shown in the *main frame*, destruction of the
// *sub frame* does not hide the popup.
TEST_F(AutofillPopupControllerTestHidingLogic,
       KeepOpenInMainFrameOnSubFrameDestruction) {
  ShowSuggestions(manager(), {PopupItemId::kAddressEntry});
  test::GenerateTestAutofillPopup(&manager().external_delegate());
  EXPECT_CALL(client().popup_controller(manager()), Hide).Times(0);
  content::RenderFrameHostTester::For(sub_frame())->Detach();
  // Verify and clear before TearDown() closes the popup.
  Mock::VerifyAndClearExpectations(&client().popup_controller(manager()));
}

// Tests that if the popup is shown in the *main frame*, a navigation in the
// *sub frame* does not hide the popup.
TEST_F(AutofillPopupControllerTestHidingLogic,
       KeepOpenInMainFrameOnSubFrameNavigation) {
  ShowSuggestions(manager(), {PopupItemId::kAddressEntry});
  test::GenerateTestAutofillPopup(&manager().external_delegate());
  EXPECT_CALL(client().popup_controller(manager()), Hide).Times(0);
  NavigateAndCommitFrame(sub_frame(), GURL("https://bar.com/"));
  // Verify and clear before TearDown() closes the popup.
  Mock::VerifyAndClearExpectations(&client().popup_controller(manager()));
}

// Tests that if the popup is shown, destruction of the WebContents hides the
// popup.
TEST_F(AutofillPopupControllerTestHidingLogic, HideOnWebContentsDestroyed) {
  ShowSuggestions(manager(), {PopupItemId::kAddressEntry});
  test::GenerateTestAutofillPopup(&manager().external_delegate());
  EXPECT_CALL(client().popup_controller(manager()),
              Hide(PopupHidingReason::kRendererEvent));
  DeleteContents();
}

// Tests that if the popup is shown in the *main frame*, destruction of the
// *main frame* hides the popup.
TEST_F(AutofillPopupControllerTestHidingLogic, HideInMainFrameOnDestruction) {
  ShowSuggestions(manager(), {PopupItemId::kAddressEntry});
  test::GenerateTestAutofillPopup(&manager().external_delegate());
  EXPECT_CALL(client().popup_controller(manager()),
              Hide(PopupHidingReason::kRendererEvent));
}

// Tests that if the popup is shown in the *sub frame*, destruction of the
// *sub frame* hides the popup.
TEST_F(AutofillPopupControllerTestHidingLogic, HideInSubFrameOnDestruction) {
  ShowSuggestions(sub_manager(), {PopupItemId::kAddressEntry});
  test::GenerateTestAutofillPopup(&sub_manager().external_delegate());
  EXPECT_CALL(client().popup_controller(sub_manager()),
              Hide(PopupHidingReason::kRendererEvent));
  content::RenderFrameHostTester::For(sub_frame())->Detach();
  // Verify and clear before TearDown() closes the popup.
  Mock::VerifyAndClearExpectations(&client().popup_controller(sub_manager()));
}

// Tests that if the popup is shown in the *main frame*, a navigation in the
// *main frame* hides the popup.
TEST_F(AutofillPopupControllerTestHidingLogic,
       HideInMainFrameOnMainFrameNavigation) {
  ShowSuggestions(manager(), {PopupItemId::kAddressEntry});
  test::GenerateTestAutofillPopup(&manager().external_delegate());
  EXPECT_CALL(client().popup_controller(manager()),
              Hide(PopupHidingReason::kNavigation));
  NavigateAndCommitFrame(main_frame(), GURL("https://bar.com/"));
  // Verify and clear before TearDown() closes the popup.
  Mock::VerifyAndClearExpectations(&client().popup_controller(manager()));
}

// Tests that if the popup is shown in the *sub frame*, a navigation in the
// *sub frame* hides the popup.
TEST_F(AutofillPopupControllerTestHidingLogic,
       HideInSubFrameOnSubFrameNavigation) {
  ShowSuggestions(sub_manager(), {PopupItemId::kAddressEntry});
  test::GenerateTestAutofillPopup(&sub_manager().external_delegate());
  if (sub_frame()->ShouldChangeRenderFrameHostOnSameSiteNavigation()) {
    // If the RenderFrameHost changes, a RenderFrameDeleted will fire first.
    EXPECT_CALL(client().popup_controller(sub_manager()),
                Hide(PopupHidingReason::kRendererEvent));
  } else {
    EXPECT_CALL(client().popup_controller(sub_manager()),
                Hide(PopupHidingReason::kNavigation));
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
TEST_F(AutofillPopupControllerTestHidingLogic,
       HideInSubFrameOnMainFrameNavigation) {
  ShowSuggestions(sub_manager(), {PopupItemId::kAddressEntry});
  test::GenerateTestAutofillPopup(&sub_manager().external_delegate());
  EXPECT_CALL(client().popup_controller(sub_manager()),
              Hide(PopupHidingReason::kRendererEvent));
  NavigateAndCommitFrame(main_frame(), GURL("https://bar.com/"));
}

// Tests that Compose saved state notification popup gets hidden after 2
// seconds, but not after 1 second.
TEST_F(AutofillPopupControllerTestHidingLogic,
       TimedHideComposeSavedStateNotification) {
  ShowSuggestions(manager(), {PopupItemId::kComposeSavedStateNotification});
  test::GenerateTestAutofillPopup(&manager().external_delegate());
  ::testing::MockFunction<void()> check;
  {
    ::testing::InSequence s;
    EXPECT_CALL(check, Call);
    EXPECT_CALL(client().popup_controller(manager()),
                Hide(PopupHidingReason::kFadeTimerExpired));
  }
  task_environment()->FastForwardBy(base::Seconds(1));
  check.Call();
  task_environment()->FastForwardBy(base::Seconds(1));
  Mock::VerifyAndClearExpectations(&client().popup_controller(manager()));
}

#if !BUILDFLAG(IS_ANDROID)
// Tests that if the popup is shown in the *main frame*, changing the zoom hides
// the popup.
TEST_F(AutofillPopupControllerTestHidingLogic, HideInMainFrameOnZoomChange) {
  zoom::ZoomController::CreateForWebContents(web_contents());
  ShowSuggestions(manager(), {PopupItemId::kAddressEntry});
  test::GenerateTestAutofillPopup(&manager().external_delegate());
  // Triggered by OnZoomChanged().
  EXPECT_CALL(client().popup_controller(manager()),
              Hide(PopupHidingReason::kContentAreaMoved));
  // Override the default ON_CALL behavior to do nothing to avoid destroying the
  // hide helper. We want to test ZoomObserver events explicitly.
  EXPECT_CALL(client().popup_controller(manager()),
              Hide(PopupHidingReason::kWidgetChanged))
      .WillOnce(Return());
  auto* zoom_controller = zoom::ZoomController::FromWebContents(web_contents());
  zoom_controller->SetZoomLevel(zoom_controller->GetZoomLevel() + 1.0);
  // Verify and clear before TearDown() closes the popup.
  Mock::VerifyAndClearExpectations(&client().popup_controller(manager()));
}
#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace autofill
