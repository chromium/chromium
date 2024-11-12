// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <vector>

#include "ash/ash_element_identifiers.h"
#include "ash/quick_insert/quick_insert_controller.h"
#include "ash/quick_insert/views/quick_insert_emoji_item_view.h"
#include "ash/quick_insert/views/quick_insert_image_item_row_view.h"
#include "ash/quick_insert/views/quick_insert_image_item_view.h"
#include "ash/quick_insert/views/quick_insert_list_item_view.h"
#include "ash/shell.h"
#include "base/files/file_util.h"
#include "base/strings/string_util.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time_override.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/test/base/ash/interactive/interactive_ash_test.h"
#include "components/history/core/browser/history_database_params.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/test/history_service_test_util.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/browsertest_util.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/state_observer.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/view_observer.h"

namespace {

using ::testing::SizeIs;

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

void SendKeyPressWithoutModifiers(ui::KeyboardCode key) {
  ui_controls::SendKeyPress(/*window=*/nullptr, key,
                            /*control=*/false, /*shift=*/false,
                            /*alt=*/false, /*command=*/false);
}

void AddUrlToHistory(Profile* profile,
                     GURL url,
                     base::Time last_visit = base::Time::Now()) {
  history::HistoryService* history_service =
      HistoryServiceFactory::GetForProfile(profile,
                                           ServiceAccessType::EXPLICIT_ACCESS);
  history_service->AddPageWithDetails(url, /*title=*/u"", /*visit_count=*/1,
                                      /*typed_count=*/1,
                                      /*last_visit=*/last_visit,
                                      /*hidden=*/false,
                                      history::SOURCE_BROWSED);
  history::BlockUntilHistoryProcessesPendingRequests(history_service);
}

bool AddLocalFileToDownloads(Profile* profile, const std::string& file_name) {
  const base::FilePath mount_path =
      file_manager::util::GetDownloadsFolderForProfile(profile);
  base::FilePath absolute_path = mount_path.AppendASCII(file_name);
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    return base::WriteFile(absolute_path, file_name);
  }
}

class QuickInsertInteractiveUiTest : public InteractiveAshTest {
 public:
  const WebContentsInteractionTestUtil::DeepQuery kInputFieldQuery{
      "input[type=\"text\"]",
  };

  QuickInsertInteractiveUiTest() {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{ash::features::kPickerGrid},
        /*disabled_features=*/{});
    ash::QuickInsertController::DisableFeatureTourForTesting();
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

  // Same as `NameDescendantView` but matches on a property of the view.
  template <typename V, typename R, typename T>
  auto NameDescendantViewByProperty(ElementSpecifier view,
                                    std::string_view name,
                                    R (V::*property)() const,
                                    T&& value) {
    return NameDescendantView(
        view, name,
        base::BindLambdaForTesting([property, value](const views::View* view) {
          if (const auto* v = views::AsViewClass<V>(view)) {
            return (v->*property)() == value;
          }
          return false;
        }));
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Searches for 'thumbs up', checks the top emoji result is '👍', and inserts it
// into a web input field.
IN_PROC_BROWSER_TEST_F(QuickInsertInteractiveUiTest, SearchAndInsertEmoji) {
  ASSERT_TRUE(CreateBrowserWindow(
      GURL("data:text/html,<input type=\"text\" autofocus/>")));
  const ui::ElementContext browser_context =
      chrome::FindLastActive()->window()->GetElementContext();
  constexpr std::string_view kFirstEmojiResultName = "FirstEmojiResult";
  constexpr std::u16string_view kExpectedFirstEmoji = u"👍";
  views::Textfield* quick_insert_search_field = nullptr;

  RunTestSequence(
      InContext(browser_context, Steps(InstrumentTab(kWebContentsElementId),
                                       WaitForWebInputFieldFocus())),
      Do([]() { TogglePickerByAccelerator(); }),
      AfterShow(ash::kQuickInsertSearchFieldTextfieldElementId,
                [&quick_insert_search_field](ui::TrackedElement* el) {
                  quick_insert_search_field = AsView<views::Textfield>(el);
                }),
      ObserveState(kSearchFieldFocusedState,
                   std::ref(quick_insert_search_field)),
      WaitForState(kSearchFieldFocusedState, true),
      EnterText(ash::kQuickInsertSearchFieldTextfieldElementId, u"thumbs up"),
      WaitForShow(ash::kQuickInsertEmojiItemElementId,
                  /*transition_only_on_event=*/true),
      NameDescendantViewByProperty(
          ash::kQuickInsertEmojiBarElementId, kFirstEmojiResultName,
          &ash::PickerEmojiItemView::GetTextForTesting, kExpectedFirstEmoji),
      PressButton(kFirstEmojiResultName),
      WaitForHide(ash::kQuickInsertElementId),
      InContext(browser_context,
                WaitForWebInputFieldValue(kExpectedFirstEmoji)));
}

// Searches for 'greek letter alpha', checks the top emoji result is 'α'; and
// inserts it into a web input field.
IN_PROC_BROWSER_TEST_F(QuickInsertInteractiveUiTest, SearchAndInsertSymbol) {
  ASSERT_TRUE(CreateBrowserWindow(
      GURL("data:text/html,<input type=\"text\" autofocus/>")));
  const ui::ElementContext browser_context =
      chrome::FindLastActive()->window()->GetElementContext();
  constexpr std::string_view kFirstSymbolResultName = "FirstSymbolResult";
  constexpr std::u16string_view kExpectedFirstSymbol = u"α";
  views::Textfield* quick_insert_search_field = nullptr;

  RunTestSequence(
      InContext(browser_context, Steps(InstrumentTab(kWebContentsElementId),
                                       WaitForWebInputFieldFocus())),
      Do([]() { TogglePickerByAccelerator(); }),
      AfterShow(ash::kQuickInsertSearchFieldTextfieldElementId,
                [&quick_insert_search_field](ui::TrackedElement* el) {
                  quick_insert_search_field = AsView<views::Textfield>(el);
                }),
      ObserveState(kSearchFieldFocusedState,
                   std::ref(quick_insert_search_field)),
      WaitForState(kSearchFieldFocusedState, true),
      EnterText(ash::kQuickInsertSearchFieldTextfieldElementId,
                u"greek letter alpha"),
      WaitForShow(ash::kQuickInsertEmojiItemElementId,
                  /*transition_only_on_event=*/true),
      NameDescendantViewByProperty(
          ash::kQuickInsertEmojiBarElementId, kFirstSymbolResultName,
          &ash::PickerEmojiItemView::GetTextForTesting, kExpectedFirstSymbol),
      PressButton(kFirstSymbolResultName),
      WaitForHide(ash::kQuickInsertElementId),
      InContext(browser_context,
                WaitForWebInputFieldValue(kExpectedFirstSymbol)));
}

// Searches for 'denko of disapproval', checks the top emoji result is 'ಠωಠ';
// and inserts it into a web input field.
IN_PROC_BROWSER_TEST_F(QuickInsertInteractiveUiTest, SearchAndInsertEmoticon) {
  ASSERT_TRUE(CreateBrowserWindow(
      GURL("data:text/html,<input type=\"text\" autofocus/>")));
  const ui::ElementContext browser_context =
      chrome::FindLastActive()->window()->GetElementContext();
  constexpr std::string_view kFirstEmoticonResultName = "FirstEmoticonResult";
  constexpr std::u16string_view kExpectedFirstEmoticon = u"ಠωಠ";
  views::Textfield* quick_insert_search_field = nullptr;

  RunTestSequence(
      InContext(browser_context, Steps(InstrumentTab(kWebContentsElementId),
                                       WaitForWebInputFieldFocus())),
      Do([]() { TogglePickerByAccelerator(); }),
      AfterShow(ash::kQuickInsertSearchFieldTextfieldElementId,
                [&quick_insert_search_field](ui::TrackedElement* el) {
                  quick_insert_search_field = AsView<views::Textfield>(el);
                }),
      ObserveState(kSearchFieldFocusedState,
                   std::ref(quick_insert_search_field)),
      WaitForState(kSearchFieldFocusedState, true),
      EnterText(ash::kQuickInsertSearchFieldTextfieldElementId,
                u"denko of disapproval"),
      WaitForShow(ash::kQuickInsertEmojiItemElementId,
                  /*transition_only_on_event=*/true),
      NameDescendantViewByProperty(
          ash::kQuickInsertEmojiBarElementId, kFirstEmoticonResultName,
          &ash::PickerEmojiItemView::GetTextForTesting, kExpectedFirstEmoticon),
      PressButton(kFirstEmoticonResultName),
      WaitForHide(ash::kQuickInsertElementId),
      InContext(browser_context,
                WaitForWebInputFieldValue(kExpectedFirstEmoticon)));
}

IN_PROC_BROWSER_TEST_F(QuickInsertInteractiveUiTest,
                       SearchAndSelectMoreEmojis) {
  ASSERT_TRUE(CreateBrowserWindow(
      GURL("data:text/html,<input type=\"text\" autofocus/>")));
  const ui::ElementContext browser_context =
      chrome::FindLastActive()->window()->GetElementContext();
  views::Textfield* quick_insert_search_field = nullptr;

  RunTestSequence(
      InContext(browser_context, Steps(InstrumentTab(kWebContentsElementId),
                                       WaitForWebInputFieldFocus())),
      Do([]() { TogglePickerByAccelerator(); }),
      AfterShow(ash::kQuickInsertSearchFieldTextfieldElementId,
                [&quick_insert_search_field](ui::TrackedElement* el) {
                  quick_insert_search_field = AsView<views::Textfield>(el);
                }),
      ObserveState(kSearchFieldFocusedState,
                   std::ref(quick_insert_search_field)),
      WaitForState(kSearchFieldFocusedState, true),
      EnterText(ash::kQuickInsertSearchFieldTextfieldElementId, u"thumbs"),
      WaitForShow(ash::kQuickInsertMoreEmojisElementId),
      PressButton(ash::kQuickInsertMoreEmojisElementId),
      WaitForHide(ash::kQuickInsertElementId),
      WaitForShow(ash::kEmojiPickerElementId));
}

IN_PROC_BROWSER_TEST_F(QuickInsertInteractiveUiTest, SearchGifs) {
  ASSERT_TRUE(CreateBrowserWindow(
      GURL("data:text/html,<input type=\"text\" autofocus/>")));
  const ui::ElementContext browser_context =
      chrome::FindLastActive()->window()->GetElementContext();
  views::Textfield* quick_insert_search_field = nullptr;

  RunTestSequence(
      InContext(browser_context, Steps(InstrumentTab(kWebContentsElementId),
                                       WaitForWebInputFieldFocus())),
      Do([]() { TogglePickerByAccelerator(); }),
      AfterShow(ash::kQuickInsertSearchFieldTextfieldElementId,
                [&quick_insert_search_field](ui::TrackedElement* el) {
                  quick_insert_search_field = AsView<views::Textfield>(el);
                }),
      ObserveState(kSearchFieldFocusedState,
                   std::ref(quick_insert_search_field)),
      WaitForState(kSearchFieldFocusedState, true),
      EnterText(ash::kQuickInsertSearchFieldTextfieldElementId, u"happy"),
      WaitForShow(ash::kQuickInsertGifElementId),
      PressButton(ash::kQuickInsertGifElementId),
      WaitForHide(ash::kQuickInsertElementId),
      WaitForShow(ash::kEmojiPickerElementId));
}

IN_PROC_BROWSER_TEST_F(QuickInsertInteractiveUiTest, SearchBrowsingHistory) {
  AddUrlToHistory(GetActiveUserProfile(), GURL("https://foo.com/history"));
  ASSERT_TRUE(CreateBrowserWindow(
      GURL("data:text/html,<input type=\"text\" autofocus/>")));
  const ui::ElementContext browser_context =
      chrome::FindLastActive()->window()->GetElementContext();
  constexpr std::string_view kHistoryResultName = "HistoryResult";
  views::Textfield* quick_insert_search_field = nullptr;

  RunTestSequence(
      InContext(browser_context, Steps(InstrumentTab(kWebContentsElementId),
                                       WaitForWebInputFieldFocus())),
      Do([]() { TogglePickerByAccelerator(); }),
      AfterShow(ash::kQuickInsertSearchFieldTextfieldElementId,
                [&quick_insert_search_field](ui::TrackedElement* el) {
                  quick_insert_search_field = AsView<views::Textfield>(el);
                }),
      ObserveState(kSearchFieldFocusedState,
                   std::ref(quick_insert_search_field)),
      WaitForState(kSearchFieldFocusedState, true),
      EnterText(ash::kQuickInsertSearchFieldTextfieldElementId, u"foo.com"),
      WaitForShow(ash::kQuickInsertSearchResultsPageElementId),
      WaitForShow(ash::kQuickInsertSearchResultsListItemElementId),
      NameDescendantViewByProperty(
          ash::kQuickInsertSearchResultsPageElementId, kHistoryResultName,
          &ash::QuickInsertListItemView::GetPrimaryTextForTesting,
          u"foo.com/history"),
      PressButton(kHistoryResultName), WaitForHide(ash::kQuickInsertElementId),
      InContext(browser_context,
                WaitForWebInputFieldValue(u"https://foo.com/history")));
}

IN_PROC_BROWSER_TEST_F(QuickInsertInteractiveUiTest,
                       SearchBrowsingHistoryCategory) {
  AddUrlToHistory(GetActiveUserProfile(), GURL("https://foo.com/history"));
  ASSERT_TRUE(CreateBrowserWindow(
      GURL("data:text/html,<input type=\"text\" autofocus/>")));
  const ui::ElementContext browser_context =
      chrome::FindLastActive()->window()->GetElementContext();
  constexpr std::string_view kHistoryCategoryResultName =
      "HistoryCategoryResult";
  constexpr std::string_view kHistoryResultName = "HistoryResult";
  views::Textfield* quick_insert_search_field = nullptr;

  RunTestSequence(
      InContext(browser_context, Steps(InstrumentTab(kWebContentsElementId),
                                       WaitForWebInputFieldFocus())),
      Do([]() { TogglePickerByAccelerator(); }),
      AfterShow(ash::kQuickInsertSearchFieldTextfieldElementId,
                [&quick_insert_search_field](ui::TrackedElement* el) {
                  quick_insert_search_field = AsView<views::Textfield>(el);
                }),
      ObserveState(kSearchFieldFocusedState,
                   std::ref(quick_insert_search_field)),
      WaitForState(kSearchFieldFocusedState, true),
      EnterText(ash::kQuickInsertSearchFieldTextfieldElementId, u"history"),
      WaitForShow(ash::kQuickInsertSearchResultsPageElementId),
      WaitForShow(ash::kQuickInsertSearchResultsListItemElementId),
      NameDescendantViewByProperty(
          ash::kQuickInsertSearchResultsPageElementId,
          kHistoryCategoryResultName,
          &ash::QuickInsertListItemView::GetPrimaryTextForTesting,
          u"Browsing history"),
      PressButton(kHistoryCategoryResultName),
      EnterText(ash::kQuickInsertSearchFieldTextfieldElementId, u"f"),
      WaitForShow(ash::kQuickInsertSearchResultsPageElementId,
                  /*transition_only_on_event=*/true),
      WaitForShow(ash::kQuickInsertSearchResultsListItemElementId),
      NameDescendantViewByProperty(
          ash::kQuickInsertSearchResultsPageElementId, kHistoryResultName,
          &ash::QuickInsertListItemView::GetPrimaryTextForTesting,
          u"foo.com/history"),
      PressButton(kHistoryResultName), WaitForHide(ash::kQuickInsertElementId),
      InContext(browser_context,
                WaitForWebInputFieldValue(u"https://foo.com/history")));
}

IN_PROC_BROWSER_TEST_F(QuickInsertInteractiveUiTest, SearchLocalFile) {
  ASSERT_TRUE(AddLocalFileToDownloads(GetActiveUserProfile(), "test.png"));
  // TODO: b/360229206 - Use a contenteditable input field so the file can be
  // inserted.
  ASSERT_TRUE(CreateBrowserWindow(
      GURL("data:text/html,<input type=\"text\" autofocus/>")));
  const ui::ElementContext browser_context =
      chrome::FindLastActive()->window()->GetElementContext();
  constexpr std::string_view kFileResultName = "FileResult";
  views::Textfield* quick_insert_search_field = nullptr;

  RunTestSequence(
      InContext(browser_context, Steps(InstrumentTab(kWebContentsElementId),
                                       WaitForWebInputFieldFocus())),
      Do([]() { TogglePickerByAccelerator(); }),
      AfterShow(ash::kQuickInsertSearchFieldTextfieldElementId,
                [&quick_insert_search_field](ui::TrackedElement* el) {
                  quick_insert_search_field = AsView<views::Textfield>(el);
                }),
      ObserveState(kSearchFieldFocusedState,
                   std::ref(quick_insert_search_field)),
      WaitForState(kSearchFieldFocusedState, true),
      EnterText(ash::kQuickInsertSearchFieldTextfieldElementId, u"test"),
      WaitForShow(ash::kQuickInsertSearchResultsPageElementId),
      WaitForShow(ash::kQuickInsertSearchResultsListItemElementId),
      NameDescendantViewByProperty(
          ash::kQuickInsertSearchResultsPageElementId, kFileResultName,
          &ash::QuickInsertListItemView::GetPrimaryTextForTesting, u"test.png"),
      PressButton(kFileResultName), WaitForHide(ash::kQuickInsertElementId));
}

IN_PROC_BROWSER_TEST_F(QuickInsertInteractiveUiTest, SearchLocalFileCategory) {
  ASSERT_TRUE(AddLocalFileToDownloads(GetActiveUserProfile(), "test.png"));
  // TODO: b/360229206 - Use a contenteditable input field so the file can be
  // inserted.
  ASSERT_TRUE(CreateBrowserWindow(
      GURL("data:text/html,<input type=\"text\" autofocus/>")));
  const ui::ElementContext browser_context =
      chrome::FindLastActive()->window()->GetElementContext();
  constexpr std::string_view kFileCategoryResultName = "FileCategoryResult";
  constexpr std::string_view kFileResultName = "FileResult";
  views::Textfield* quick_insert_search_field = nullptr;

  RunTestSequence(
      InContext(browser_context, Steps(InstrumentTab(kWebContentsElementId),
                                       WaitForWebInputFieldFocus())),
      Do([]() { TogglePickerByAccelerator(); }),
      AfterShow(ash::kQuickInsertSearchFieldTextfieldElementId,
                [&quick_insert_search_field](ui::TrackedElement* el) {
                  quick_insert_search_field = AsView<views::Textfield>(el);
                }),
      ObserveState(kSearchFieldFocusedState,
                   std::ref(quick_insert_search_field)),
      WaitForState(kSearchFieldFocusedState, true),
      // Search for the file category
      EnterText(ash::kQuickInsertSearchFieldTextfieldElementId, u"file"),
      WaitForShow(ash::kQuickInsertSearchResultsPageElementId),
      WaitForShow(ash::kQuickInsertSearchResultsListItemElementId),
      NameDescendantViewByProperty(
          ash::kQuickInsertSearchResultsPageElementId, kFileCategoryResultName,
          &ash::QuickInsertListItemView::GetPrimaryTextForTesting, u"Files"),
      // Press the file category and check the file grid.
      PressButton(kFileCategoryResultName),
      WaitForShow(ash::kQuickInsertSearchResultsImageItemElementId),
      // Search for a file and insert it.
      EnterText(ash::kQuickInsertSearchFieldTextfieldElementId, u"t"),
      WaitForShow(ash::kQuickInsertSearchResultsListItemElementId),
      NameDescendantViewByProperty(
          ash::kQuickInsertSearchResultsPageElementId, kFileResultName,
          &ash::QuickInsertListItemView::GetPrimaryTextForTesting, u"test.png"),
      PressButton(kFileResultName), WaitForHide(ash::kQuickInsertElementId));
}

// Searches for 'today', checks the top result is the date, and inserts it
// into a web input field.
IN_PROC_BROWSER_TEST_F(QuickInsertInteractiveUiTest, SearchAndInsertDate) {
  ASSERT_TRUE(CreateBrowserWindow(
      GURL("data:text/html,<input type=\"text\" autofocus/>")));
  const ui::ElementContext browser_context =
      chrome::FindLastActive()->window()->GetElementContext();
  constexpr std::string_view kDateResultName = "DateResult";
  constexpr std::u16string_view kExpectedDate = u"Feb 19";
  views::Textfield* quick_insert_search_field = nullptr;
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
      AfterShow(ash::kQuickInsertSearchFieldTextfieldElementId,
                [&quick_insert_search_field](ui::TrackedElement* el) {
                  quick_insert_search_field = AsView<views::Textfield>(el);
                }),
      ObserveState(kSearchFieldFocusedState,
                   std::ref(quick_insert_search_field)),
      WaitForState(kSearchFieldFocusedState, true),
      EnterText(ash::kQuickInsertSearchFieldTextfieldElementId, u"today"),
      WaitForShow(ash::kQuickInsertSearchResultsPageElementId),
      WaitForShow(ash::kQuickInsertSearchResultsListItemElementId),
      NameDescendantViewByProperty(
          ash::kQuickInsertSearchResultsPageElementId, kDateResultName,
          &ash::QuickInsertListItemView::GetPrimaryTextForTesting,
          kExpectedDate),
      PressButton(kDateResultName), WaitForHide(ash::kQuickInsertElementId),
      InContext(browser_context, WaitForWebInputFieldValue(kExpectedDate)));
}

// Searches for '1 + 1', checks the top result is '2', and inserts it
// into a web input field.
// TODO: crbug.com/355618977 - Fix flakiness.
IN_PROC_BROWSER_TEST_F(QuickInsertInteractiveUiTest,
                       DISABLED_SearchAndInsertMath) {
  ASSERT_TRUE(CreateBrowserWindow(
      GURL("data:text/html,<input type=\"text\" autofocus/>")));
  const ui::ElementContext browser_context =
      chrome::FindLastActive()->window()->GetElementContext();
  constexpr std::string_view kMathResultName = "MathResult";
  constexpr std::u16string_view kExpectedResult = u"2";
  views::Textfield* quick_insert_search_field = nullptr;

  RunTestSequence(
      InContext(browser_context, Steps(InstrumentTab(kWebContentsElementId),
                                       WaitForWebInputFieldFocus())),
      Do([]() { TogglePickerByAccelerator(); }),
      AfterShow(ash::kQuickInsertSearchFieldTextfieldElementId,
                [&quick_insert_search_field](ui::TrackedElement* el) {
                  quick_insert_search_field = AsView<views::Textfield>(el);
                }),
      ObserveState(kSearchFieldFocusedState,
                   std::ref(quick_insert_search_field)),
      WaitForState(kSearchFieldFocusedState, true),
      EnterText(ash::kQuickInsertSearchFieldTextfieldElementId, u"1 + 1"),
      WaitForShow(ash::kQuickInsertSearchResultsPageElementId),
      WaitForShow(ash::kQuickInsertSearchResultsListItemElementId),
      NameDescendantViewByProperty(
          ash::kQuickInsertSearchResultsPageElementId, kMathResultName,
          &ash::QuickInsertListItemView::GetPrimaryTextForTesting,
          kExpectedResult),
      PressButton(kMathResultName), WaitForHide(ash::kQuickInsertElementId),
      InContext(browser_context, WaitForWebInputFieldValue(kExpectedResult)));
}

// TODO(crbug.com/376317307): Re-enable once flakiness is fixed.
IN_PROC_BROWSER_TEST_F(QuickInsertInteractiveUiTest,
                       DISABLED_ZeroStateShowsSuggestions) {
  ASSERT_TRUE(AddLocalFileToDownloads(GetActiveUserProfile(), "test1.png"));
  ASSERT_TRUE(AddLocalFileToDownloads(GetActiveUserProfile(), "test2.png"));
  ASSERT_TRUE(AddLocalFileToDownloads(GetActiveUserProfile(), "test3.png"));
  ASSERT_TRUE(CreateBrowserWindow(
      GURL("data:text/html,<input type=\"text\" autofocus/>")));
  const ui::ElementContext browser_context =
      chrome::FindLastActive()->window()->GetElementContext();
  constexpr std::string_view kFile1Name = "File1";
  constexpr std::string_view kFile2Name = "File2";
  constexpr std::string_view kFile3Name = "File3";
  views::Textfield* quick_insert_search_field = nullptr;

  RunTestSequence(
      InContext(browser_context, Steps(InstrumentTab(kWebContentsElementId),
                                       WaitForWebInputFieldFocus())),
      Do([]() { TogglePickerByAccelerator(); }),
      AfterShow(ash::kQuickInsertSearchFieldTextfieldElementId,
                [&quick_insert_search_field](ui::TrackedElement* el) {
                  quick_insert_search_field = AsView<views::Textfield>(el);
                }),
      ObserveState(kSearchFieldFocusedState,
                   std::ref(quick_insert_search_field)),
      WaitForState(kSearchFieldFocusedState, true),
      WaitForShow(ash::kQuickInsertSearchResultsImageRowElementId),
      WaitForViewProperty(ash::kQuickInsertSearchResultsImageRowElementId,
                          ash::PickerImageItemRowView, Items, SizeIs(3)),
      NameDescendantViewByType<ash::PickerImageItemView>(
          ash::kQuickInsertElementId, kFile1Name, 0),
      NameDescendantViewByType<ash::PickerImageItemView>(
          ash::kQuickInsertElementId, kFile2Name, 1),
      NameDescendantViewByType<ash::PickerImageItemView>(
          ash::kQuickInsertElementId, kFile3Name, 2),
      CheckViewProperty(kFile1Name, &views::View::GetAccessibleName,
                        u"Insert test1.png"),
      CheckViewProperty(kFile2Name, &views::View::GetAccessibleName,
                        u"Insert test2.png"),
      CheckViewProperty(kFile3Name, &views::View::GetAccessibleName,
                        u"Insert test3.png"));
}

// Navigates through the zero-state UI using only the keyboard.
IN_PROC_BROWSER_TEST_F(QuickInsertInteractiveUiTest,
                       KeyboardNavigationInZeroState) {
  ASSERT_TRUE(CreateBrowserWindow(
      GURL("data:text/html,<input type=\"text\" autofocus/>")));
  const ui::ElementContext browser_context =
      chrome::FindLastActive()->window()->GetElementContext();
  constexpr std::string_view kItem1Name = "Item1";
  constexpr std::string_view kItem2Name = "Item2";
  constexpr std::string_view kEmoji1Name = "Emoji1";
  constexpr std::string_view kEmoji2Name = "Emoji2";
  views::Textfield* quick_insert_search_field = nullptr;

  RunTestSequence(
      InContext(browser_context, Steps(InstrumentTab(kWebContentsElementId),
                                       WaitForWebInputFieldFocus())),
      Do([]() { TogglePickerByAccelerator(); }),
      AfterShow(ash::kQuickInsertSearchFieldTextfieldElementId,
                [&quick_insert_search_field](ui::TrackedElement* el) {
                  quick_insert_search_field = AsView<views::Textfield>(el);
                }),
      ObserveState(kSearchFieldFocusedState,
                   std::ref(quick_insert_search_field)),
      WaitForState(kSearchFieldFocusedState, true),
      NameDescendantViewByType<ash::QuickInsertListItemView>(
          ash::kQuickInsertElementId, kItem1Name, 0),
      NameDescendantViewByType<ash::QuickInsertListItemView>(
          ash::kQuickInsertElementId, kItem2Name, 1),
      NameDescendantViewByType<ash::PickerEmojiItemView>(
          ash::kQuickInsertElementId, kEmoji1Name, 0),
      NameDescendantViewByType<ash::PickerEmojiItemView>(
          ash::kQuickInsertElementId, kEmoji2Name, 1),
      // The first item should be selected by default.
      CheckViewProperty(kItem1Name, &ash::QuickInsertItemView::GetItemState,
                        ash::QuickInsertItemView::ItemState::kPseudoFocused),
      // Down arrow should move to the next item.
      Do([]() { SendKeyPressWithoutModifiers(ui::VKEY_DOWN); }),
      CheckViewProperty(kItem2Name, &ash::QuickInsertItemView::GetItemState,
                        ash::QuickInsertItemView::ItemState::kPseudoFocused),
      CheckViewProperty(kItem1Name, &ash::QuickInsertItemView::GetItemState,
                        ash::QuickInsertItemView::ItemState::kNormal),
      // Up arrow should move to the previous item.
      Do([]() { SendKeyPressWithoutModifiers(ui::VKEY_UP); }),
      CheckViewProperty(kItem1Name, &ash::QuickInsertItemView::GetItemState,
                        ash::QuickInsertItemView::ItemState::kPseudoFocused),
      CheckViewProperty(kItem2Name, &ash::QuickInsertItemView::GetItemState,
                        ash::QuickInsertItemView::ItemState::kNormal),
      // Up arrow should move to the first emoji in the emoji bar.
      Do([]() { SendKeyPressWithoutModifiers(ui::VKEY_UP); }),
      CheckViewProperty(kEmoji1Name, &ash::QuickInsertItemView::GetItemState,
                        ash::QuickInsertItemView::ItemState::kPseudoFocused),
      CheckViewProperty(kItem1Name, &ash::QuickInsertItemView::GetItemState,
                        ash::QuickInsertItemView::ItemState::kNormal),
      // Right arrow should move to the second emoji in the emoji bar.
      Do([]() { SendKeyPressWithoutModifiers(ui::VKEY_RIGHT); }),
      CheckViewProperty(kEmoji2Name, &ash::QuickInsertItemView::GetItemState,
                        ash::QuickInsertItemView::ItemState::kPseudoFocused),
      CheckViewProperty(kEmoji1Name, &ash::QuickInsertItemView::GetItemState,
                        ash::QuickInsertItemView::ItemState::kNormal),
      // Left arrow should back move to the first emoji in the emoji bar.
      Do([]() { SendKeyPressWithoutModifiers(ui::VKEY_LEFT); }),
      CheckViewProperty(kEmoji1Name, &ash::QuickInsertItemView::GetItemState,
                        ash::QuickInsertItemView::ItemState::kPseudoFocused),
      CheckViewProperty(kEmoji2Name, &ash::QuickInsertItemView::GetItemState,
                        ash::QuickInsertItemView::ItemState::kNormal),
      // Down arrow should back move to the first item.
      Do([]() { SendKeyPressWithoutModifiers(ui::VKEY_DOWN); }),
      CheckViewProperty(kItem1Name, &ash::QuickInsertItemView::GetItemState,
                        ash::QuickInsertItemView::ItemState::kPseudoFocused),
      CheckViewProperty(kEmoji1Name, &ash::QuickInsertItemView::GetItemState,
                        ash::QuickInsertItemView::ItemState::kNormal));
}

IN_PROC_BROWSER_TEST_F(QuickInsertInteractiveUiTest, LocalFilePreview) {
  ASSERT_TRUE(AddLocalFileToDownloads(GetActiveUserProfile(), "test.png"));
  constexpr std::string_view kFileResultName = "FileResult";
  views::Textfield* quick_insert_search_field = nullptr;

  RunTestSequence(
      Do([]() { TogglePickerByAccelerator(); }),
      AfterShow(ash::kQuickInsertSearchFieldTextfieldElementId,
                [&quick_insert_search_field](ui::TrackedElement* el) {
                  quick_insert_search_field = AsView<views::Textfield>(el);
                }),
      ObserveState(kSearchFieldFocusedState,
                   std::ref(quick_insert_search_field)),
      WaitForState(kSearchFieldFocusedState, true),
      EnterText(ash::kQuickInsertSearchFieldTextfieldElementId, u"test"),
      WaitForShow(ash::kQuickInsertSearchResultsPageElementId),
      WaitForShow(ash::kQuickInsertSearchResultsListItemElementId),
      NameDescendantViewByProperty(
          ash::kQuickInsertSearchResultsPageElementId, kFileResultName,
          &ash::QuickInsertListItemView::GetPrimaryTextForTesting, u"test.png"),
      MoveMouseTo(kFileResultName),
      WaitForShow(ash::kQuickInsertPreviewBubbleElementId));
}

}  // namespace
