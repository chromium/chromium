// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/metrics/picker_performance_metrics.h"
#include "ash/picker/views/picker_emoji_bar_view.h"
#include "ash/picker/views/picker_key_event_handler.h"
#include "ash/picker/views/picker_search_field_view.h"
#include "ash/picker/views/picker_section_list_view.h"
#include "ash/picker/views/picker_section_view.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/icon_button.h"
#include "ash/test/test_widget_builder.h"
#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#include "chrome/browser/ash/accessibility/speech_monitor.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/browsertest_util.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace {

// These tests are not meant to be end-to-end tests.
// They should test Picker view components on a low-level.
// They are browser tests in order to bring in ChromeVox so that we can test
// announcements.
class PickerAccessibilityBrowserTest : public InProcessBrowserTest {
 public:
  PickerAccessibilityBrowserTest() = default;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    ash::AccessibilityManager::Get()->EnableSpokenFeedback(true);
    // Ignore the intro.
    sm_.ExpectSpeechPattern("*");
    // Disable earcons which can be annoying in tests.
    sm_.Call([this]() {
      ImportJSModuleForChromeVox("ChromeVox",
                                 "/chromevox/background/chromevox.js");
      DisableEarcons();
    });
  }

  void TearDownOnMainThread() override {
    ash::AccessibilityManager::Get()->EnableSpokenFeedback(false);
    InProcessBrowserTest::TearDownOnMainThread();
  }

 protected:
  ash::test::SpeechMonitor sm_;

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

IN_PROC_BROWSER_TEST_F(PickerAccessibilityBrowserTest,
                       FocusingEmptySearchFieldAnnouncesPlaceholder) {
  std::unique_ptr<views::Widget> widget =
      ash::TestWidgetBuilder()
          .SetWidgetType(views::Widget::InitParams::TYPE_WINDOW_FRAMELESS)
          .BuildClientOwnsWidget();
  ash::PickerKeyEventHandler key_event_handler;
  ash::PickerPerformanceMetrics metrics;
  auto* view =
      widget->SetContentsView(std::make_unique<ash::PickerSearchFieldView>(
          base::DoNothing(), base::DoNothing(), &key_event_handler, &metrics));
  view->SetPlaceholderText(u"cat");

  sm_.Call([view]() { view->RequestFocus(); });

  sm_.ExpectSpeechPattern("Edit text");
  sm_.ExpectSpeechPattern("cat");
  sm_.Replay();
}

IN_PROC_BROWSER_TEST_F(PickerAccessibilityBrowserTest,
                       FocusingNonEmptySearchFieldAnnouncesPlaceholder) {
  std::unique_ptr<views::Widget> widget =
      ash::TestWidgetBuilder()
          .SetWidgetType(views::Widget::InitParams::TYPE_WINDOW_FRAMELESS)
          .BuildClientOwnsWidget();
  ash::PickerKeyEventHandler key_event_handler;
  ash::PickerPerformanceMetrics metrics;
  auto* view =
      widget->SetContentsView(std::make_unique<ash::PickerSearchFieldView>(
          base::DoNothing(), base::DoNothing(), &key_event_handler, &metrics));
  view->SetPlaceholderText(u"cat");
  view->SetQueryText(u"query");

  sm_.Call([view]() { view->RequestFocus(); });

  sm_.ExpectSpeechPattern("Edit text");
  sm_.ExpectSpeechPattern("cat");
  sm_.Replay();
}

IN_PROC_BROWSER_TEST_F(PickerAccessibilityBrowserTest,
                       FocusingSearchFieldClearButtonAnnouncesTooltip) {
  std::unique_ptr<views::Widget> widget =
      ash::TestWidgetBuilder()
          .SetWidgetType(views::Widget::InitParams::TYPE_WINDOW_FRAMELESS)
          .BuildClientOwnsWidget();
  ash::PickerKeyEventHandler key_event_handler;
  ash::PickerPerformanceMetrics metrics;
  auto* view =
      widget->SetContentsView(std::make_unique<ash::PickerSearchFieldView>(
          base::DoNothing(), base::DoNothing(), &key_event_handler, &metrics));
  view->SetPlaceholderText(u"placeholder");

  sm_.Call([view]() {
    view->textfield_for_testing().InsertOrReplaceText(u"a");
    view->clear_button_for_testing().RequestFocus();
  });

  sm_.ExpectSpeechPattern(l10n_util::GetStringUTF8(
      IDS_PICKER_SEARCH_FIELD_CLEAR_BUTTON_TOOLTIP_TEXT));
  sm_.ExpectSpeechPattern("Button");
  sm_.ExpectSpeechPattern("Press * to activate");
  sm_.Replay();
}

IN_PROC_BROWSER_TEST_F(PickerAccessibilityBrowserTest,
                       FocusingSearchFieldBackButtonAnnouncesTooltip) {
  std::unique_ptr<views::Widget> widget =
      ash::TestWidgetBuilder()
          .SetWidgetType(views::Widget::InitParams::TYPE_WINDOW_FRAMELESS)
          .BuildClientOwnsWidget();
  ash::PickerKeyEventHandler key_event_handler;
  ash::PickerPerformanceMetrics metrics;
  auto* view =
      widget->SetContentsView(std::make_unique<ash::PickerSearchFieldView>(
          base::DoNothing(), base::DoNothing(), &key_event_handler, &metrics));
  view->SetPlaceholderText(u"placeholder");
  view->SetBackButtonVisible(true);

  sm_.Call([view]() { view->back_button_for_testing().RequestFocus(); });

  sm_.ExpectSpeechPattern(l10n_util::GetStringUTF8(
      IDS_PICKER_SEARCH_FIELD_BACK_BUTTON_TOOLTIP_TEXT));
  sm_.ExpectSpeechPattern("Button");
  sm_.ExpectSpeechPattern("Press * to activate");
  sm_.Replay();
}

IN_PROC_BROWSER_TEST_F(PickerAccessibilityBrowserTest,
                       FocusingGifsButtonAnnouncesLabel) {
  std::unique_ptr<views::Widget> widget =
      ash::TestWidgetBuilder()
          .SetWidgetType(views::Widget::InitParams::TYPE_WINDOW_FRAMELESS)
          .BuildClientOwnsWidget();
  auto* view =
      widget->SetContentsView(std::make_unique<ash::PickerEmojiBarView>(
          /*delegate=*/nullptr, /*picker_width=*/100));

  sm_.Call([view]() { view->gifs_button_for_testing()->RequestFocus(); });

  sm_.ExpectSpeechPattern(
      l10n_util::GetStringUTF8(IDS_PICKER_GIFS_BUTTON_LABEL));
  sm_.ExpectSpeechPattern("Button");
  sm_.ExpectSpeechPattern("Press * to activate");
  sm_.Replay();
}

IN_PROC_BROWSER_TEST_F(PickerAccessibilityBrowserTest,
                       FocusingMoreEmojisAnnouncesTooltip) {
  std::unique_ptr<views::Widget> widget =
      ash::TestWidgetBuilder()
          .SetWidgetType(views::Widget::InitParams::TYPE_WINDOW_FRAMELESS)
          .BuildClientOwnsWidget();
  auto* view =
      widget->SetContentsView(std::make_unique<ash::PickerEmojiBarView>(
          /*delegate=*/nullptr, /*picker_width=*/100));

  sm_.Call(
      [view]() { view->more_emojis_button_for_testing()->RequestFocus(); });

  sm_.ExpectSpeechPattern(
      l10n_util::GetStringUTF8(IDS_PICKER_MORE_EMOJIS_BUTTON_ACCESSIBLE_NAME));
  sm_.ExpectSpeechPattern("Button");
  sm_.ExpectSpeechPattern("Press * to activate");
  sm_.Replay();
}

IN_PROC_BROWSER_TEST_F(PickerAccessibilityBrowserTest,
                       SectionsAnnouncesHeadings) {
  std::unique_ptr<views::Widget> widget =
      ash::TestWidgetBuilder()
          .SetWidgetType(views::Widget::InitParams::TYPE_WINDOW_FRAMELESS)
          .BuildClientOwnsWidget();
  auto* view =
      widget->SetContentsView(std::make_unique<ash::PickerSectionListView>(
          /*section_width=*/100, /*asset_fetcher=*/nullptr,
          /*submenu_controller=*/nullptr));
  view->AddSection()->AddTitleLabel(u"Section1");
  view->AddSection()->AddTitleLabel(u"Section2");
  ui::test::EventGenerator event_generator(
      ash::Shell::Get()->GetPrimaryRootWindow());

  sm_.Call([view, &event_generator]() {
    view->RequestFocus();
    event_generator.PressAndReleaseKeyAndModifierKeys(ui::KeyboardCode::VKEY_H,
                                                      ui::EF_COMMAND_DOWN);
  });

  sm_.ExpectSpeechPattern("Section1");
  sm_.ExpectSpeechPattern("Heading");

  sm_.Call([&event_generator]() {
    event_generator.PressAndReleaseKeyAndModifierKeys(ui::KeyboardCode::VKEY_H,
                                                      ui::EF_COMMAND_DOWN);
  });

  sm_.ExpectSpeechPattern("Section2");
  sm_.ExpectSpeechPattern("Heading");
  sm_.Replay();
}

}  // namespace
