// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <vector>

#include "ash/ash_element_identifiers.h"
#include "ash/picker/picker_controller.h"
#include "ash/picker/views/picker_emoji_item_view.h"
#include "ash/picker/views/picker_emoticon_item_view.h"
#include "ash/picker/views/picker_list_item_view.h"
#include "ash/picker/views/picker_symbol_item_view.h"
#include "ash/shell.h"
#include "base/strings/string_util.h"
#include "base/time/time_override.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/test/base/ash/interactive/interactive_ash_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/browsertest_util.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/state_observer.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/view_observer.h"

namespace {

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kWebContentsElementId);
DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kWebInputFieldFocusedEvent);
DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kWebInputFieldValueEvent);

class ViewFocusObserver
    : public ui::test::
          ObservationStateObserver<bool, views::View, views::ViewObserver> {
 public:
  explicit ViewFocusObserver(views::View* view)
      : ObservationStateObserver(view) {}
  ~ViewFocusObserver() override = default;

  // ui::test::ObservationStateObserver:
  bool GetStateObserverInitialState() const override {
    return source()->HasFocus();
  }

  // views::ViewObserver:
  void OnViewFocused(views::View* observed_view) override {
    if (observed_view == source()) {
      OnStateObserverStateChanged(true);
    }
  }
  void OnViewBlurred(views::View* observed_view) override {
    if (observed_view == source()) {
      OnStateObserverStateChanged(false);
    }
  }
  void OnViewIsDeleting(views::View* observed_view) override {
    OnObservationStateObserverSourceDestroyed();
  }
};

DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(ViewFocusObserver,
                                    kSearchFieldFocusedState);

void TogglePickerByAccelerator() {
  ui_controls::SendKeyPress(/*window=*/nullptr, ui::VKEY_F,
                            /*control=*/false, /*shift=*/false,
                            /*alt=*/false, /*command=*/true);
}

class PickerInteractiveUiTest : public InteractiveAshTest {
 public:
  const WebContentsInteractionTestUtil::DeepQuery kInputFieldQuery{
      "input[type=\"text\"]",
  };

  PickerInteractiveUiTest() {
    ash::PickerController::DisableFeatureKeyCheck();
    ash::PickerController::DisableFeatureTourForTesting();
  }

  void SetUpOnMainThread() override {
    InteractiveAshTest::SetUpOnMainThread();

    // Set up context for element tracking for InteractiveAshTest.
    SetupContextWidget();
  }

  auto WaitForWebInputFieldFocus() {
    StateChange expected_state;
    expected_state.type = StateChange::Type::kExistsAndConditionTrue;
    expected_state.where = kInputFieldQuery;
    expected_state.test_function = "el => el === document.activeElement";
    expected_state.event = kWebInputFieldFocusedEvent;

    return Steps(WaitForStateChange(kWebContentsElementId, expected_state));
  }

  auto WaitForWebInputFieldValue(std::u16string_view value) {
    StateChange expected_state;
    expected_state.type = StateChange::Type::kExistsAndConditionTrue;
    expected_state.where = kInputFieldQuery;
    expected_state.test_function =
        content::JsReplace("el => el.value === $1", value);
    expected_state.event = kWebInputFieldValueEvent;

    return Steps(WaitForStateChange(kWebContentsElementId, expected_state));
  }

 private:
  base::test::ScopedFeatureList feature_list_{ash::features::kPicker};
};

// Searches for 'thumbs up', checks the top emoji result is '👍', and inserts it
// into a web input field.
IN_PROC_BROWSER_TEST_F(PickerInteractiveUiTest, SearchAndInsertEmoji) {
  ASSERT_TRUE(CreateBrowserWindow(
      GURL("data:text/html,<input type=\"text\" autofocus/>")));
  const ui::ElementContext browser_context =
      chrome::FindLastActive()->window()->GetElementContext();
  constexpr std::string_view kFirstEmojiResultName = "FirstEmojiResult";
  constexpr std::u16string_view kExpectedFirstEmoji = u"👍";
  views::Textfield* picker_search_field = nullptr;

  RunTestSequence(
      InContext(browser_context, Steps(InstrumentTab(kWebContentsElementId),
                                       WaitForWebInputFieldFocus())),
      Do([]() { TogglePickerByAccelerator(); }),
      AfterShow(ash::kPickerSearchFieldTextfieldElementId,
                [&picker_search_field](ui::TrackedElement* el) {
                  picker_search_field = AsView<views::Textfield>(el);
                }),
      ObserveState(kSearchFieldFocusedState, std::ref(picker_search_field)),
      WaitForState(kSearchFieldFocusedState, true),
      EnterText(ash::kPickerSearchFieldTextfieldElementId, u"thumbs up"),
      WaitForShow(ash::kPickerEmojiItemElementId,
                  /*transition_only_on_event=*/true),
      NameDescendantView(
          ash::kPickerEmojiBarElementId, kFirstEmojiResultName,
          base::BindLambdaForTesting(
              [kExpectedFirstEmoji](const views::View* view) {
                if (const auto* emoji_item_view =
                        views::AsViewClass<ash::PickerEmojiItemView>(view)) {
                  return emoji_item_view->GetTextForTesting() ==
                         kExpectedFirstEmoji;
                }
                return false;
              })),
      PressButton(kFirstEmojiResultName), WaitForHide(ash::kPickerElementId),
      InContext(browser_context,
                WaitForWebInputFieldValue(kExpectedFirstEmoji)));
}

// Searches for 'greek letter alpha', checks the top emoji result is 'α'; and
// inserts it into a web input field.
IN_PROC_BROWSER_TEST_F(PickerInteractiveUiTest, SearchAndInsertSymbol) {
  ASSERT_TRUE(CreateBrowserWindow(
      GURL("data:text/html,<input type=\"text\" autofocus/>")));
  const ui::ElementContext browser_context =
      chrome::FindLastActive()->window()->GetElementContext();
  constexpr std::string_view kFirstSymbolResultName = "FirstSymbolResult";
  constexpr std::u16string_view kExpectedFirstSymbol = u"α";
  views::Textfield* picker_search_field = nullptr;

  RunTestSequence(
      InContext(browser_context, Steps(InstrumentTab(kWebContentsElementId),
                                       WaitForWebInputFieldFocus())),
      Do([]() { TogglePickerByAccelerator(); }),
      AfterShow(ash::kPickerSearchFieldTextfieldElementId,
                [&picker_search_field](ui::TrackedElement* el) {
                  picker_search_field = AsView<views::Textfield>(el);
                }),
      ObserveState(kSearchFieldFocusedState, std::ref(picker_search_field)),
      WaitForState(kSearchFieldFocusedState, true),
      EnterText(ash::kPickerSearchFieldTextfieldElementId,
                u"greek letter alpha"),
      WaitForShow(ash::kPickerEmojiItemElementId,
                  /*transition_only_on_event=*/true),
      NameDescendantView(
          ash::kPickerEmojiBarElementId, kFirstSymbolResultName,
          base::BindLambdaForTesting(
              [kExpectedFirstSymbol](const views::View* view) {
                if (const auto* symbol_item_view =
                        views::AsViewClass<ash::PickerSymbolItemView>(view)) {
                  return symbol_item_view->GetTextForTesting() ==
                         kExpectedFirstSymbol;
                }
                return false;
              })),
      PressButton(kFirstSymbolResultName), WaitForHide(ash::kPickerElementId),
      InContext(browser_context,
                WaitForWebInputFieldValue(kExpectedFirstSymbol)));
}

// Searches for 'denko of disapproval', checks the top emoji result is 'ಠωಠ';
// and inserts it into a web input field.
IN_PROC_BROWSER_TEST_F(PickerInteractiveUiTest, SearchAndInsertEmoticon) {
  ASSERT_TRUE(CreateBrowserWindow(
      GURL("data:text/html,<input type=\"text\" autofocus/>")));
  const ui::ElementContext browser_context =
      chrome::FindLastActive()->window()->GetElementContext();
  constexpr std::string_view kFirstEmoticonResultName = "FirstEmoticonResult";
  constexpr std::u16string_view kExpectedFirstEmoticon = u"ಠωಠ";
  views::Textfield* picker_search_field = nullptr;

  RunTestSequence(
      InContext(browser_context, Steps(InstrumentTab(kWebContentsElementId),
                                       WaitForWebInputFieldFocus())),
      Do([]() { TogglePickerByAccelerator(); }),
      AfterShow(ash::kPickerSearchFieldTextfieldElementId,
                [&picker_search_field](ui::TrackedElement* el) {
                  picker_search_field = AsView<views::Textfield>(el);
                }),
      ObserveState(kSearchFieldFocusedState, std::ref(picker_search_field)),
      WaitForState(kSearchFieldFocusedState, true),
      EnterText(ash::kPickerSearchFieldTextfieldElementId,
                u"denko of disapproval"),
      WaitForShow(ash::kPickerEmojiItemElementId,
                  /*transition_only_on_event=*/true),
      NameDescendantView(
          ash::kPickerEmojiBarElementId, kFirstEmoticonResultName,
          base::BindLambdaForTesting(
              [kExpectedFirstEmoticon](const views::View* view) {
                if (const auto* emoticon_item_view =
                        views::AsViewClass<ash::PickerEmoticonItemView>(view)) {
                  return emoticon_item_view->GetTextForTesting() ==
                         kExpectedFirstEmoticon;
                }
                return false;
              })),
      PressButton(kFirstEmoticonResultName), WaitForHide(ash::kPickerElementId),
      InContext(browser_context,
                WaitForWebInputFieldValue(kExpectedFirstEmoticon)));
}

IN_PROC_BROWSER_TEST_F(PickerInteractiveUiTest, SearchAndSelectMoreEmojis) {
  ASSERT_TRUE(CreateBrowserWindow(
      GURL("data:text/html,<input type=\"text\" autofocus/>")));
  const ui::ElementContext browser_context =
      chrome::FindLastActive()->window()->GetElementContext();
  views::Textfield* picker_search_field = nullptr;

  RunTestSequence(
      InContext(browser_context, Steps(InstrumentTab(kWebContentsElementId),
                                       WaitForWebInputFieldFocus())),
      Do([]() { TogglePickerByAccelerator(); }),
      AfterShow(ash::kPickerSearchFieldTextfieldElementId,
                [&picker_search_field](ui::TrackedElement* el) {
                  picker_search_field = AsView<views::Textfield>(el);
                }),
      ObserveState(kSearchFieldFocusedState, std::ref(picker_search_field)),
      WaitForState(kSearchFieldFocusedState, true),
      EnterText(ash::kPickerSearchFieldTextfieldElementId, u"thumbs"),
      WaitForShow(ash::kPickerMoreEmojisElementId),
      PressButton(ash::kPickerMoreEmojisElementId),
      WaitForHide(ash::kPickerElementId),
      WaitForShow(ash::kEmojiPickerElementId));
}

IN_PROC_BROWSER_TEST_F(PickerInteractiveUiTest, SearchGifs) {
  ASSERT_TRUE(CreateBrowserWindow(
      GURL("data:text/html,<input type=\"text\" autofocus/>")));
  const ui::ElementContext browser_context =
      chrome::FindLastActive()->window()->GetElementContext();
  views::Textfield* picker_search_field = nullptr;

  RunTestSequence(
      InContext(browser_context, Steps(InstrumentTab(kWebContentsElementId),
                                       WaitForWebInputFieldFocus())),
      Do([]() { TogglePickerByAccelerator(); }),
      AfterShow(ash::kPickerSearchFieldTextfieldElementId,
                [&picker_search_field](ui::TrackedElement* el) {
                  picker_search_field = AsView<views::Textfield>(el);
                }),
      ObserveState(kSearchFieldFocusedState, std::ref(picker_search_field)),
      WaitForState(kSearchFieldFocusedState, true),
      EnterText(ash::kPickerSearchFieldTextfieldElementId, u"happy"),
      WaitForShow(ash::kPickerGifElementId),
      PressButton(ash::kPickerGifElementId), WaitForHide(ash::kPickerElementId),
      WaitForShow(ash::kEmojiPickerElementId));
}

// Searches for 'today', checks the top result is the date, and inserts it
// into a web input field.
IN_PROC_BROWSER_TEST_F(PickerInteractiveUiTest, SearchAndInsertDate) {
  ASSERT_TRUE(CreateBrowserWindow(
      GURL("data:text/html,<input type=\"text\" autofocus/>")));
  const ui::ElementContext browser_context =
      chrome::FindLastActive()->window()->GetElementContext();
  constexpr std::string_view kDateResultName = "DateResult";
  constexpr std::u16string_view kExpectedDate = u"Feb 19";
  views::Textfield* picker_search_field = nullptr;
  base::subtle::ScopedTimeClockOverrides time_override(
      []() {
        base::Time date;
        bool result = base::Time::FromString("19 Feb 2024 12:00 GMT", &date);
        CHECK(result);
        return date;
      },
      /*time_ticks_override=*/nullptr,
      /*thread_ticks_override=*/nullptr);

  RunTestSequence(
      InContext(browser_context, Steps(InstrumentTab(kWebContentsElementId),
                                       WaitForWebInputFieldFocus())),
      Do([]() { TogglePickerByAccelerator(); }),
      AfterShow(ash::kPickerSearchFieldTextfieldElementId,
                [&picker_search_field](ui::TrackedElement* el) {
                  picker_search_field = AsView<views::Textfield>(el);
                }),
      ObserveState(kSearchFieldFocusedState, std::ref(picker_search_field)),
      WaitForState(kSearchFieldFocusedState, true),
      EnterText(ash::kPickerSearchFieldTextfieldElementId, u"today"),
      WaitForShow(ash::kPickerSearchResultsPageElementId),
      WaitForShow(ash::kPickerSearchResultsListItemElementId),
      NameDescendantView(
          ash::kPickerSearchResultsPageElementId, kDateResultName,
          base::BindLambdaForTesting([kExpectedDate](const views::View* view) {
            if (const auto* list_item_view =
                    views::AsViewClass<ash::PickerListItemView>(view)) {
              return list_item_view->GetPrimaryTextForTesting() ==
                     kExpectedDate;
            }
            return false;
          })),
      PressButton(kDateResultName), WaitForHide(ash::kPickerElementId),
      InContext(browser_context, WaitForWebInputFieldValue(kExpectedDate)));
}

// Searches for '1 + 1', checks the top result is '2', and inserts it
// into a web input field.
// TODO: crbug.com/355618977 - Fix flakiness.
IN_PROC_BROWSER_TEST_F(PickerInteractiveUiTest, DISABLED_SearchAndInsertMath) {
  ASSERT_TRUE(CreateBrowserWindow(
      GURL("data:text/html,<input type=\"text\" autofocus/>")));
  const ui::ElementContext browser_context =
      chrome::FindLastActive()->window()->GetElementContext();
  constexpr std::string_view kMathResultName = "MathResult";
  constexpr std::u16string_view kExpectedResult = u"2";
  views::Textfield* picker_search_field = nullptr;

  RunTestSequence(
      InContext(browser_context, Steps(InstrumentTab(kWebContentsElementId),
                                       WaitForWebInputFieldFocus())),
      Do([]() { TogglePickerByAccelerator(); }),
      AfterShow(ash::kPickerSearchFieldTextfieldElementId,
                [&picker_search_field](ui::TrackedElement* el) {
                  picker_search_field = AsView<views::Textfield>(el);
                }),
      ObserveState(kSearchFieldFocusedState, std::ref(picker_search_field)),
      WaitForState(kSearchFieldFocusedState, true),
      EnterText(ash::kPickerSearchFieldTextfieldElementId, u"1 + 1"),
      WaitForShow(ash::kPickerSearchResultsPageElementId),
      WaitForShow(ash::kPickerSearchResultsListItemElementId),
      NameDescendantView(
          ash::kPickerSearchResultsPageElementId, kMathResultName,
          base::BindLambdaForTesting(
              [kExpectedResult](const views::View* view) {
                if (const auto* list_item_view =
                        views::AsViewClass<ash::PickerListItemView>(view)) {
                  return list_item_view->GetPrimaryTextForTesting() ==
                         kExpectedResult;
                }
                return false;
              })),
      PressButton(kMathResultName), WaitForHide(ash::kPickerElementId),
      InContext(browser_context, WaitForWebInputFieldValue(kExpectedResult)));
}

// TODO: b/330786933: Add interactive UI test for file previews.

}  // namespace
