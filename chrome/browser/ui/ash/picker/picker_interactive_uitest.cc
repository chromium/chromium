// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "ash/ash_element_identifiers.h"
#include "ash/picker/picker_controller.h"
#include "ash/picker/views/picker_emoji_item_view.h"
#include "ash/shell.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/test/base/chromeos/crosier/interactive_ash_test.h"
#include "content/public/test/browser_test_utils.h"
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
  ui_controls::SendKeyPress(/*window=*/nullptr, ui::VKEY_S,
                            /*control=*/false, /*shift=*/false,
                            /*alt=*/false, /*command=*/true);
}

class PickerInteractiveUiTest : public InteractiveAshTest {
 public:
  const WebContentsInteractionTestUtil::DeepQuery kInputFieldQuery{
      "input[type=\"text\"]",
  };

  PickerInteractiveUiTest() {
    ash::PickerController::DisableFeatureKeyCheckForTesting();
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
      WaitForShow(ash::kPickerSearchResultsEmojiItemElementId),
      WaitForShow(ash::kPickerSearchResultsPageElementId),
      NameDescendantView(
          ash::kPickerSearchResultsPageElementId, kFirstEmojiResultName,
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

}  // namespace
