// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <vector>

#include "ash/ash_element_identifiers.h"
#include "ash/picker/picker_controller.h"
#include "ash/picker/views/picker_emoji_item_view.h"
#include "ash/picker/views/picker_feature_tour.h"
#include "ash/picker/views/picker_list_item_view.h"
#include "ash/shell.h"
#include "base/strings/string_util.h"
#include "base/time/time_override.h"
#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#include "chrome/browser/ash/accessibility/speech_monitor.h"
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

class ReusableSpeechMonitor {
 public:
  ReusableSpeechMonitor() { CreateNewSpeechMonitor(); }

  void ExpectSpeechPattern(const std::string& pattern,
                           const base::Location& location = FROM_HERE) {
    GetActiveSpeechMonitor().ExpectSpeechPattern(pattern, location);
  }

  void Call(base::FunctionRef<void()> func,
            const base::Location& location = FROM_HERE) {
    GetActiveSpeechMonitor().Call([func]() { func(); }, location);
  }

  void Replay() {
    GetActiveSpeechMonitor().Replay();

    // Create a new SpeechMonitor since `Replay` can only be called once.
    CreateNewSpeechMonitor();
  }

 private:
  ash::test::SpeechMonitor& GetActiveSpeechMonitor() {
    return *speech_monitors_.back();
  }

  void CreateNewSpeechMonitor() {
    speech_monitors_.push_back(std::make_unique<ash::test::SpeechMonitor>());
  }

  // A pool of SpeechMonitors.
  // Old SpeechMonitors are not deleted until the test ends, since the
  // SpeechMonitor destructor will unintentionally create a TtsEngineDelegate
  // that will block future utterances.
  std::vector<std::unique_ptr<ash::test::SpeechMonitor>> speech_monitors_;
};

void SendKeyPress(ui::KeyboardCode keyboard_code) {
  ui_controls::SendKeyPress(/*window=*/nullptr, keyboard_code,
                            /*control=*/false, /*shift=*/false,
                            /*alt=*/false, /*command=*/false);
}

void TogglePickerByAccelerator() {
  ui_controls::SendKeyPress(/*window=*/nullptr, ui::VKEY_F,
                            /*control=*/false, /*shift=*/false,
                            /*alt=*/false, /*command=*/true);
}

void TogglePicker() {
  ash::Shell::Get()->picker_controller()->ToggleWidget();
}

class PickerInteractiveUiTest : public InteractiveAshTest {
 public:
  const WebContentsInteractionTestUtil::DeepQuery kInputFieldQuery{
      "input[type=\"text\"]",
  };

  PickerInteractiveUiTest() {
    ash::PickerController::DisableFeatureKeyCheckForTesting();
    ash::PickerFeatureTour::DisableFeatureTourForTesting();
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
IN_PROC_BROWSER_TEST_F(PickerInteractiveUiTest, SearchAndInsertMath) {
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

class PickerSpokenFeedbackInteractiveUiTest : public PickerInteractiveUiTest {
 public:
  void SetUpOnMainThread() override {
    PickerInteractiveUiTest::SetUpOnMainThread();

    ash::AccessibilityManager::Get()->EnableSpokenFeedback(true);
    // Ignore the intro.
    sm_.ExpectSpeechPattern("*");
    // Disable earcons which can be annoying in tests.
    sm_.Call([this]() {
      ImportJSModuleForChromeVox("ChromeVox",
                                 "/chromevox/background/chromevox.js");
      DisableEarcons();
    });
    sm_.Replay();
  }

  void TearDownOnMainThread() override {
    ash::AccessibilityManager::Get()->EnableSpokenFeedback(false);
    PickerInteractiveUiTest::TearDownOnMainThread();
  }

 protected:
  ReusableSpeechMonitor sm_;

 private:
  void ImportJSModuleForChromeVox(std::string_view name,
                                  std::string_view path) {
    extensions::browsertest_util::ExecuteScriptInBackgroundPageDeprecated(
        ash::AccessibilityManager::Get()->profile(),
        extension_misc::kChromeVoxExtensionId,
        base::ReplaceStringPlaceholders(
            R"(import('$1').then(mod => {
            globalThis.$2 = mod.$2;
            window.domAutomationController.send('done');
          }))",
            {std::string(path), std::string(name)}, nullptr));
  }

  void DisableEarcons() {
    extensions::browsertest_util::ExecuteScriptInBackgroundPageNoWait(
        ash::AccessibilityManager::Get()->profile(),
        extension_misc::kChromeVoxExtensionId,
        "ChromeVox.earcons.playEarcon = function() {};");
  }
};

IN_PROC_BROWSER_TEST_F(PickerSpokenFeedbackInteractiveUiTest,
                       AnnouncesOnWindowShown) {
  ASSERT_TRUE(CreateBrowserWindow(
      GURL("data:text/html,<input type=\"text\" autofocus/>")));
  const ui::ElementContext browser_context =
      chrome::FindLastActive()->window()->GetElementContext();
  views::Textfield* picker_search_field = nullptr;

  RunTestSequence(
      InContext(browser_context, Steps(InstrumentTab(kWebContentsElementId),
                                       WaitForWebInputFieldFocus())),
      Do([]() { TogglePicker(); }),
      AfterShow(ash::kPickerSearchFieldTextfieldElementId,
                [&picker_search_field](ui::TrackedElement* el) {
                  picker_search_field = AsView<views::Textfield>(el);
                }),
      ObserveState(kSearchFieldFocusedState, std::ref(picker_search_field)),
      WaitForState(kSearchFieldFocusedState, true));

  sm_.ExpectSpeechPattern("Edit text");
  sm_.ExpectSpeechPattern("window");
  sm_.Replay();
}

IN_PROC_BROWSER_TEST_F(PickerSpokenFeedbackInteractiveUiTest,
                       AnnouncesKeyboardNavigationOnZeroState) {
  ASSERT_TRUE(CreateBrowserWindow(
      GURL("data:text/html,<input type=\"text\" autofocus/>")));
  const ui::ElementContext browser_context =
      chrome::FindLastActive()->window()->GetElementContext();
  views::Textfield* picker_search_field = nullptr;

  RunTestSequence(
      InContext(browser_context, Steps(InstrumentTab(kWebContentsElementId),
                                       WaitForWebInputFieldFocus())),
      Do([]() { TogglePicker(); }),
      AfterShow(ash::kPickerSearchFieldTextfieldElementId,
                [&picker_search_field](ui::TrackedElement* el) {
                  picker_search_field = AsView<views::Textfield>(el);
                }),
      ObserveState(kSearchFieldFocusedState, std::ref(picker_search_field)),
      WaitForState(kSearchFieldFocusedState, true), Do([&sm = sm_]() {
        SendKeyPress(ui::VKEY_DOWN);

        sm.ExpectSpeechPattern("*Browsing history*");
        // TODO: b/338142316 - Use correct role for zero state items.
        sm.ExpectSpeechPattern("Button");
        sm.Replay();

        SendKeyPress(ui::VKEY_DOWN);
        sm.ExpectSpeechPattern("*Emojis*");
        // TODO: b/338142316 - Use correct role for zero state items.
        sm.ExpectSpeechPattern("Button");
        sm.Replay();

        SendKeyPress(ui::VKEY_UP);
        sm.ExpectSpeechPattern("*Browsing history*");
        // TODO: b/338142316 - Use correct role for zero state items.
        sm.ExpectSpeechPattern("Button");
        sm.Replay();
      }));
}

IN_PROC_BROWSER_TEST_F(PickerSpokenFeedbackInteractiveUiTest,
                       AnnouncesKeyboardNavigationOnResultsPage) {
  ASSERT_TRUE(CreateBrowserWindow(
      GURL("data:text/html,<input type=\"text\" autofocus/>")));
  const ui::ElementContext browser_context =
      chrome::FindLastActive()->window()->GetElementContext();
  views::Textfield* picker_search_field = nullptr;

  RunTestSequence(
      InContext(browser_context, Steps(InstrumentTab(kWebContentsElementId),
                                       WaitForWebInputFieldFocus())),
      Do([]() { TogglePicker(); }),
      AfterShow(ash::kPickerSearchFieldTextfieldElementId,
                [&picker_search_field](ui::TrackedElement* el) {
                  picker_search_field = AsView<views::Textfield>(el);
                }),
      ObserveState(kSearchFieldFocusedState, std::ref(picker_search_field)),
      WaitForState(kSearchFieldFocusedState, true),
      // Enter a query that is guaranteed to have some results.
      EnterText(ash::kPickerSearchFieldTextfieldElementId, u"a"),
      WaitForShow(ash::kPickerSearchResultsPageElementId),
      WaitForShow(ash::kPickerSearchResultsListItemElementId),
      Do([&sm = sm_]() {
        SendKeyPress(ui::VKEY_DOWN);
        sm.ExpectSpeechPattern("*");
        // TODO: b/338142316 - Use correct role for result items.
        sm.ExpectSpeechPattern("Button");
        sm.Replay();

        SendKeyPress(ui::VKEY_UP);
        sm.ExpectSpeechPattern("*");
        // TODO: b/338142316 - Use correct role for result items.
        sm.ExpectSpeechPattern("Button");
        sm.Replay();
      }));
}

// TODO: b/330786933: Add interactive UI test for file previews.

}  // namespace
