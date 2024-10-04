// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"
#include "ash/picker/metrics/picker_performance_metrics.h"
#include "ash/picker/model/picker_action_type.h"
#include "ash/picker/model/picker_search_results_section.h"
#include "ash/picker/picker_controller.h"
#include "ash/picker/picker_search_result.h"
#include "ash/picker/views/mock_picker_search_results_view_delegate.h"
#include "ash/picker/views/picker_emoji_bar_view.h"
#include "ash/picker/views/picker_emoji_item_view.h"
#include "ash/picker/views/picker_feature_tour.h"
#include "ash/picker/views/picker_image_item_view.h"
#include "ash/picker/views/picker_item_with_submenu_view.h"
#include "ash/picker/views/picker_key_event_handler.h"
#include "ash/picker/views/picker_list_item_view.h"
#include "ash/picker/views/picker_preview_bubble_controller.h"
#include "ash/picker/views/picker_pseudo_focus.h"
#include "ash/picker/views/picker_search_bar_textfield.h"
#include "ash/picker/views/picker_search_field_view.h"
#include "ash/picker/views/picker_search_results_view.h"
#include "ash/picker/views/picker_section_list_view.h"
#include "ash/picker/views/picker_section_view.h"
#include "ash/picker/views/picker_view_delegate.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/icon_button.h"
#include "ash/test/test_widget_builder.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#include "chrome/browser/ash/accessibility/speech_monitor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/picker/picker_client_impl.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/strings/grit/components_strings.h"
#include "components/user_manager/user_manager.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/browsertest_util.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout_view.h"
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
    sm_.ExpectSpeechPattern("ChromeVox*");
    // Disable earcons which can be annoying in tests.
    sm_.Call([this]() {
      ImportJSModuleForChromeVox("ChromeVox",
                                 "/chromevox/background/chromevox.js");
      DisableEarcons();
    });

    ash::PickerController::DisableFeatureTourForTesting();
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

  base::test::ScopedFeatureList scoped_feature_list_;
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

  sm_.ExpectSpeechPattern("cat");
  sm_.ExpectSpeechPattern("Edit text");
  sm_.Replay();
}

IN_PROC_BROWSER_TEST_F(PickerAccessibilityBrowserTest,
                       SetDescendantAnnouncesDescendant) {
  std::unique_ptr<views::Widget> widget =
      ash::TestWidgetBuilder()
          .SetWidgetType(views::Widget::InitParams::TYPE_WINDOW_FRAMELESS)
          .BuildClientOwnsWidget();
  ash::PickerKeyEventHandler key_event_handler;
  ash::PickerPerformanceMetrics metrics;
  auto* container_view =
      widget->SetContentsView(views::Builder<views::BoxLayoutView>().Build());
  auto* search_field_view =
      container_view->AddChildView(std::make_unique<ash::PickerSearchFieldView>(
          base::DoNothing(), base::DoNothing(), &key_event_handler, &metrics));
  auto* other_view =
      container_view->AddChildView(std::make_unique<views::Label>(u"test"));
  search_field_view->SetPlaceholderText(u"cat");

  sm_.Call([search_field_view]() { search_field_view->RequestFocus(); });

  sm_.ExpectSpeechPattern("cat");
  sm_.ExpectSpeechPattern("Edit text");

  sm_.Call([search_field_view, other_view]() {
    search_field_view->SetTextfieldActiveDescendant(other_view);
  });

  sm_.ExpectSpeechPattern("test");
  sm_.Replay();
}

IN_PROC_BROWSER_TEST_F(PickerAccessibilityBrowserTest,
                       SetDescendantAnnouncesDescendantAfterKeyEvent) {
  std::unique_ptr<views::Widget> widget =
      ash::TestWidgetBuilder()
          .SetWidgetType(views::Widget::InitParams::TYPE_WINDOW_FRAMELESS)
          .BuildClientOwnsWidget();
  ash::PickerKeyEventHandler key_event_handler;
  ash::PickerPerformanceMetrics metrics;
  auto* container_view =
      widget->SetContentsView(views::Builder<views::BoxLayoutView>().Build());
  auto* search_field_view =
      container_view->AddChildView(std::make_unique<ash::PickerSearchFieldView>(
          base::DoNothing(), base::DoNothing(), &key_event_handler, &metrics));
  auto* other_view =
      container_view->AddChildView(std::make_unique<views::Label>(u"test"));
  search_field_view->SetPlaceholderText(u"cat");

  sm_.Call([search_field_view]() { search_field_view->RequestFocus(); });

  sm_.ExpectSpeechPattern("cat");
  sm_.ExpectSpeechPattern("Edit text");

  sm_.Call([search_field_view, other_view]() {
    ui::test::EventGenerator event_generator(
        ash::Shell::Get()->GetPrimaryRootWindow());
    event_generator.PressAndReleaseKey(ui::VKEY_A);
    search_field_view->SetTextfieldActiveDescendant(other_view);
  });

  sm_.ExpectSpeechPattern("A");
  sm_.ExpectSpeechPattern("test");
  sm_.Replay();
}

IN_PROC_BROWSER_TEST_F(PickerAccessibilityBrowserTest,
                       SetDescendantToTextfieldAnnouncesPlaceholder) {
  std::unique_ptr<views::Widget> widget =
      ash::TestWidgetBuilder()
          .SetWidgetType(views::Widget::InitParams::TYPE_WINDOW_FRAMELESS)
          .BuildClientOwnsWidget();
  ash::PickerKeyEventHandler key_event_handler;
  ash::PickerPerformanceMetrics metrics;
  auto* container_view =
      widget->SetContentsView(views::Builder<views::BoxLayoutView>().Build());
  auto* search_field_view =
      container_view->AddChildView(std::make_unique<ash::PickerSearchFieldView>(
          base::DoNothing(), base::DoNothing(), &key_event_handler, &metrics));
  auto* other_view =
      container_view->AddChildView(std::make_unique<views::Label>(u"test"));
  search_field_view->SetPlaceholderText(u"cat");

  sm_.Call([search_field_view]() { search_field_view->RequestFocus(); });

  sm_.ExpectSpeechPattern("cat");
  sm_.ExpectSpeechPattern("Edit text");

  sm_.Call([search_field_view, other_view]() {
    search_field_view->SetTextfieldActiveDescendant(other_view);
  });

  sm_.ExpectSpeechPattern("test");

  sm_.Call([search_field_view]() {
    search_field_view->SetTextfieldActiveDescendant(
        search_field_view->textfield());
  });

  sm_.ExpectSpeechPattern("cat");
  sm_.ExpectSpeechPattern("Edit text");
  sm_.Replay();
}

// TODO(crbug.com/356567533): flaky on MSAN. Deflake and re-enable the test.
#if defined(MEMORY_SANITIZER)
#define MAYBE_SetDescendantThenFocusingSearchFieldAnnouncesDescendant \
  DISABLED_SetDescendantThenFocusingSearchFieldAnnouncesDescendant
#else
#define MAYBE_SetDescendantThenFocusingSearchFieldAnnouncesDescendant \
  SetDescendantThenFocusingSearchFieldAnnouncesDescendant
#endif
IN_PROC_BROWSER_TEST_F(
    PickerAccessibilityBrowserTest,
    MAYBE_SetDescendantThenFocusingSearchFieldAnnouncesDescendant) {
  std::unique_ptr<views::Widget> widget =
      ash::TestWidgetBuilder()
          .SetWidgetType(views::Widget::InitParams::TYPE_WINDOW_FRAMELESS)
          .BuildClientOwnsWidget();
  ash::PickerKeyEventHandler key_event_handler;
  ash::PickerPerformanceMetrics metrics;
  auto* container_view =
      widget->SetContentsView(views::Builder<views::BoxLayoutView>().Build());
  auto* search_field_view =
      container_view->AddChildView(std::make_unique<ash::PickerSearchFieldView>(
          base::DoNothing(), base::DoNothing(), &key_event_handler, &metrics));
  auto* other_view =
      container_view->AddChildView(std::make_unique<views::Label>(u"test"));
  search_field_view->SetPlaceholderText(u"cat");

  sm_.Call([search_field_view, other_view]() {
    search_field_view->SetTextfieldActiveDescendant(other_view);
    search_field_view->RequestFocus();
  });

  sm_.ExpectSpeechPattern("cat");
  sm_.ExpectSpeechPattern("Edit text");
  sm_.ExpectSpeechPattern("test");
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

  sm_.ExpectSpeechPattern("cat");
  sm_.ExpectSpeechPattern("Edit text");
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

  sm_.ExpectSpeechPattern(
      l10n_util::GetStringUTF8(IDS_APP_LIST_CLEAR_SEARCHBOX));
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

  sm_.ExpectSpeechPattern(l10n_util::GetStringUTF8(IDS_ACCNAME_BACK));
  sm_.ExpectSpeechPattern("Button");
  sm_.ExpectSpeechPattern("Press * to activate");
  sm_.Replay();
}

IN_PROC_BROWSER_TEST_F(PickerAccessibilityBrowserTest,
                       FocusingEmojiBarItemsAnnouncesGrid) {
  std::unique_ptr<views::Widget> widget =
      ash::TestWidgetBuilder()
          .SetWidgetType(views::Widget::InitParams::TYPE_WINDOW_FRAMELESS)
          .BuildClientOwnsWidget();
  auto* view =
      widget->SetContentsView(std::make_unique<ash::PickerEmojiBarView>(
          /*delegate=*/nullptr, /*picker_width=*/1000,
          /*is_gifs_enabled=*/true));
  view->SetSearchResults({
      ash::PickerEmojiResult::Emoji(u"😊", u"happy"),
      ash::PickerEmojiResult::Symbol(u"♬", u"music"),
      ash::PickerEmojiResult::Emoticon(u"(°□°)", u"surprise"),
  });

  sm_.Call([view]() { view->GetItemsForTesting()[0]->RequestFocus(); });

  sm_.ExpectSpeechPattern("happy emoji");
  sm_.ExpectSpeechPattern("Button");
  sm_.ExpectSpeechPattern("row 1 column 1");
  sm_.ExpectSpeechPattern("Table Emojis and GIFs, 1 by 5");

  sm_.Call([view]() { view->GetItemsForTesting()[1]->RequestFocus(); });

  sm_.ExpectSpeechPattern("music");
  sm_.ExpectSpeechPattern("Button");
  sm_.ExpectSpeechPattern("row 1 column 2");

  sm_.Call([view]() { view->GetItemsForTesting()[2]->RequestFocus(); });

  sm_.ExpectSpeechPattern("surprise emoticon");
  sm_.ExpectSpeechPattern("Button");
  sm_.ExpectSpeechPattern("row 1 column 3");

  sm_.Call([view]() { view->gifs_button_for_testing()->RequestFocus(); });

  sm_.ExpectSpeechPattern("GIF");
  sm_.ExpectSpeechPattern("Button");
  sm_.ExpectSpeechPattern("row 1 column 4");

  sm_.Call(
      [view]() { view->more_emojis_button_for_testing()->RequestFocus(); });

  sm_.ExpectSpeechPattern(l10n_util::GetStringUTF8(
      IDS_PICKER_MORE_EMOJIS_AND_GIFS_BUTTON_ACCESSIBLE_NAME));
  sm_.ExpectSpeechPattern("Button");
  sm_.ExpectSpeechPattern("row 1 column 5");

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
          /*delegate=*/nullptr, /*picker_width=*/100,
          /*is_gifs_enabled=*/true));

  sm_.Call([view]() { view->gifs_button_for_testing()->RequestFocus(); });

  sm_.ExpectSpeechPattern("GIF");
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

IN_PROC_BROWSER_TEST_F(PickerAccessibilityBrowserTest,
                       FocusingSectionShowAllAnnounces) {
  std::unique_ptr<views::Widget> widget =
      ash::TestWidgetBuilder()
          .SetWidgetType(views::Widget::InitParams::TYPE_WINDOW_FRAMELESS)
          .BuildClientOwnsWidget();
  auto* view =
      widget->SetContentsView(std::make_unique<ash::PickerSectionListView>(
          /*section_width=*/100, /*asset_fetcher=*/nullptr,
          /*submenu_controller=*/nullptr));
  ash::PickerSectionView* section_view = view->AddSection();
  section_view->AddTitleLabel(u"Section");
  section_view->AddTitleTrailingLink(u"Show all", u"cat", base::DoNothing());

  sm_.Call([section_view]() {
    section_view->title_trailing_link_for_testing()->RequestFocus();
  });

  sm_.ExpectSpeechPattern("cat");
  sm_.ExpectSpeechPattern("Button");
  sm_.ExpectSpeechPattern("Press * to activate");
  sm_.Replay();
}

IN_PROC_BROWSER_TEST_F(PickerAccessibilityBrowserTest,
                       ListItemAnnouncesTextWithAction) {
  std::unique_ptr<views::Widget> widget =
      ash::TestWidgetBuilder()
          .SetWidgetType(views::Widget::InitParams::TYPE_WINDOW_FRAMELESS)
          .BuildClientOwnsWidget();
  auto* view =
      widget->SetContentsView(std::make_unique<ash::PickerSectionListView>(
          /*section_width=*/100, /*asset_fetcher=*/nullptr,
          /*submenu_controller=*/nullptr));
  ash::PickerSectionView* section = view->AddSection();
  section->AddTitleLabel(u"Section1");
  ash::PickerListItemView* item = section->AddListItem(
      std::make_unique<ash::PickerListItemView>(base::DoNothing()));
  item->SetLeadingIcon(ui::ImageModel::FromImage(gfx::test::CreateImage(1)));
  item->SetPrimaryText(u"primary");
  item->SetSecondaryText(u"secondary");
  item->SetBadgeAction(ash::PickerActionType::kInsert);
  item->SetBadgeVisible(true);

  sm_.Call([item]() { item->RequestFocus(); });

  sm_.ExpectSpeechPattern("Insert primary, secondary");
  sm_.ExpectSpeechPattern("Button");
  sm_.ExpectSpeechPattern("Press * to activate");
  sm_.Replay();
}

IN_PROC_BROWSER_TEST_F(PickerAccessibilityBrowserTest,
                       ListItemAnnouncesPreviewMetadata) {
  ash::PickerPreviewBubbleController preview_controller;
  std::unique_ptr<views::Widget> widget =
      ash::TestWidgetBuilder()
          .SetWidgetType(views::Widget::InitParams::TYPE_WINDOW_FRAMELESS)
          .BuildClientOwnsWidget();
  auto* view =
      widget->SetContentsView(std::make_unique<ash::PickerSectionListView>(
          /*section_width=*/100, /*asset_fetcher=*/nullptr,
          /*submenu_controller=*/nullptr));
  ash::PickerSectionView* section = view->AddSection();
  section->AddTitleLabel(u"Section1");
  ash::PickerListItemView* item = section->AddListItem(
      std::make_unique<ash::PickerListItemView>(base::DoNothing()));
  item->SetPrimaryText(u"primary");
  base::test::TestFuture<void> file_info_future;
  base::File::Info file_info;
  EXPECT_TRUE(
      base::Time::FromString("23 Dec 2021 09:01:00", &file_info.last_modified));
  item->SetPreview(
      &preview_controller,
      file_info_future.GetSequenceBoundCallback().Then(
          base::ReturnValueOnce<std::optional<base::File::Info>>(file_info)),
      base::FilePath(), base::DoNothing(),
      /*update_icon=*/true);
  ASSERT_TRUE(file_info_future.Wait());

  sm_.Call([item]() { item->RequestFocus(); });

  sm_.ExpectSpeechPattern("Button");
  sm_.ExpectSpeechPattern("Edited · Dec 23");
  sm_.ExpectSpeechPattern("Press * to activate");
  sm_.Replay();
}

IN_PROC_BROWSER_TEST_F(PickerAccessibilityBrowserTest,
                       ImageRowItemAnnouncesTitle) {
  std::unique_ptr<views::Widget> widget =
      ash::TestWidgetBuilder()
          .SetWidgetType(views::Widget::InitParams::TYPE_WINDOW_FRAMELESS)
          .BuildClientOwnsWidget();
  auto* view =
      widget->SetContentsView(std::make_unique<ash::PickerSectionListView>(
          /*section_width=*/100, /*asset_fetcher=*/nullptr,
          /*submenu_controller=*/nullptr));
  ash::PickerSectionView* section = view->AddSection();
  section->AddTitleLabel(u"Section1");
  section->SetImageRowProperties(u"Image Row", base::DoNothing(),
                                 u"More Items");
  ash::PickerImageItemView* item =
      section->AddImageRowItem(std::make_unique<ash::PickerImageItemView>(
          std::make_unique<views::ImageView>(
              ui::ImageModel::FromImage(gfx::test::CreateImage(1))),
          u"title1", base::DoNothing()));
  item->SetAction(ash::PickerActionType::kInsert);
  section->AddImageRowItem(std::make_unique<ash::PickerImageItemView>(
      std::make_unique<views::ImageView>(
          ui::ImageModel::FromImage(gfx::test::CreateImage(1))),
      u"title2", base::DoNothing()));

  sm_.Call([item]() { item->RequestFocus(); });

  sm_.ExpectSpeechPattern("Insert title1");
  sm_.ExpectSpeechPattern("Button");
  sm_.ExpectSpeechPattern("row 1 column 1");
  sm_.ExpectSpeechPattern("Table Image Row, 1 by 3");
  sm_.ExpectSpeechPattern("Press * to activate");
  sm_.Replay();
}

IN_PROC_BROWSER_TEST_F(PickerAccessibilityBrowserTest,
                       ImageRowMoreItemsButtonAnnouncesTooltip) {
  std::unique_ptr<views::Widget> widget =
      ash::TestWidgetBuilder()
          .SetWidgetType(views::Widget::InitParams::TYPE_WINDOW_FRAMELESS)
          .BuildClientOwnsWidget();
  auto* view =
      widget->SetContentsView(std::make_unique<ash::PickerSectionListView>(
          /*section_width=*/100, /*asset_fetcher=*/nullptr,
          /*submenu_controller=*/nullptr));
  ash::PickerSectionView* section = view->AddSection();
  section->AddTitleLabel(u"Section1");
  section->SetImageRowProperties(u"Image Row", base::DoNothing(),
                                 u"More Items");
  section->AddImageRowItem(std::make_unique<ash::PickerImageItemView>(
      std::make_unique<views::ImageView>(
          ui::ImageModel::FromImage(gfx::test::CreateImage(1))),
      u"title", base::DoNothing()));

  sm_.Call([section]() {
    section->GetImageRowMoreItemsButtonForTesting()->RequestFocus();
  });

  sm_.ExpectSpeechPattern("More Items");
  sm_.ExpectSpeechPattern("Button");
  sm_.ExpectSpeechPattern("row 1 column 2");
  sm_.ExpectSpeechPattern("Table Image Row, 1 by 2");
  sm_.ExpectSpeechPattern("Press * to activate");
  sm_.Replay();
}

IN_PROC_BROWSER_TEST_F(PickerAccessibilityBrowserTest,
                       ImageGridItemAnnouncesTitle) {
  std::unique_ptr<views::Widget> widget =
      ash::TestWidgetBuilder()
          .SetWidgetType(views::Widget::InitParams::TYPE_WINDOW_FRAMELESS)
          .BuildClientOwnsWidget();
  auto* view =
      widget->SetContentsView(std::make_unique<ash::PickerSectionListView>(
          /*section_width=*/100, /*asset_fetcher=*/nullptr,
          /*submenu_controller=*/nullptr));
  ash::PickerSectionView* section = view->AddSection();
  section->AddTitleLabel(u"Section1");
  ash::PickerImageItemView* item1 =
      section->AddImageGridItem(std::make_unique<ash::PickerImageItemView>(
          std::make_unique<views::ImageView>(
              ui::ImageModel::FromImage(gfx::test::CreateImage(1))),
          u"title1", base::DoNothing()));
  item1->SetAction(ash::PickerActionType::kInsert);

  ash::PickerImageItemView* item2 =
      section->AddImageGridItem(std::make_unique<ash::PickerImageItemView>(
          std::make_unique<views::ImageView>(
              ui::ImageModel::FromImage(gfx::test::CreateImage(1))),
          u"title2", base::DoNothing()));
  item2->SetAction(ash::PickerActionType::kOpen);

  ash::PickerImageItemView* item3 =
      section->AddImageGridItem(std::make_unique<ash::PickerImageItemView>(
          std::make_unique<views::ImageView>(
              ui::ImageModel::FromImage(gfx::test::CreateImage(1))),
          u"title3", base::DoNothing()));

  sm_.Call([item1]() { item1->RequestFocus(); });

  sm_.ExpectSpeechPattern("Insert title1");
  sm_.ExpectSpeechPattern("Button");
  sm_.ExpectSpeechPattern("List item");
  sm_.ExpectSpeechPattern("1 of 3");
  sm_.ExpectSpeechPattern("Press * to activate");

  sm_.Call([item2]() { item2->RequestFocus(); });

  // TODO: b/362129770 - item2 should not have "List end".
  sm_.ExpectSpeechPattern("Open title2");
  sm_.ExpectSpeechPattern("Button");
  sm_.ExpectSpeechPattern("List item");
  sm_.ExpectSpeechPattern("2 of 3");
  sm_.ExpectSpeechPattern("List end");
  sm_.ExpectSpeechPattern("Press * to activate");

  sm_.Call([item3]() { item3->RequestFocus(); });

  // TODO: b/362129770 - item3 should have "List end".
  sm_.ExpectSpeechPattern("title3");
  sm_.ExpectSpeechPattern("Button");
  sm_.ExpectSpeechPattern("List item");
  sm_.ExpectSpeechPattern("3 of 3");
  sm_.ExpectSpeechPattern("Press * to activate");

  sm_.Replay();
}

IN_PROC_BROWSER_TEST_F(PickerAccessibilityBrowserTest,
                       FocusingItemInSectionListViewAnnouncesSizeAndPosition) {
  std::unique_ptr<views::Widget> widget =
      ash::TestWidgetBuilder()
          .SetWidgetType(views::Widget::InitParams::TYPE_WINDOW_FRAMELESS)
          .BuildClientOwnsWidget();
  auto* view =
      widget->SetContentsView(std::make_unique<ash::PickerSectionListView>(
          /*section_width=*/100, /*asset_fetcher=*/nullptr,
          /*submenu_controller=*/nullptr));
  ash::PickerSectionView* section1 = view->AddSection();
  ash::PickerListItemView* item1 = section1->AddListItem(
      std::make_unique<ash::PickerListItemView>(base::DoNothing()));
  item1->SetPrimaryText(u"item1");
  ash::PickerListItemView* item2 = section1->AddListItem(
      std::make_unique<ash::PickerListItemView>(base::DoNothing()));
  item2->SetPrimaryText(u"item2");
  ash::PickerSectionView* section2 = view->AddSection();
  ash::PickerListItemView* item3 = section2->AddListItem(
      std::make_unique<ash::PickerListItemView>(base::DoNothing()));
  item3->SetPrimaryText(u"item3");

  sm_.Call([item1]() { item1->RequestFocus(); });

  sm_.ExpectSpeechPattern("item1");
  sm_.ExpectSpeechPattern("Button");
  sm_.ExpectSpeechPattern("List item");
  sm_.ExpectSpeechPattern("1 of 2");
  sm_.ExpectSpeechPattern("List");
  sm_.ExpectSpeechPattern("with 2 items");

  sm_.Call([item2]() { item2->RequestFocus(); });

  sm_.ExpectSpeechPattern("item2");
  sm_.ExpectSpeechPattern("Button");
  sm_.ExpectSpeechPattern("List item");
  sm_.ExpectSpeechPattern("2 of 2");

  sm_.Call([item3]() { item3->RequestFocus(); });

  sm_.ExpectSpeechPattern("item3");
  sm_.ExpectSpeechPattern("Button");
  sm_.ExpectSpeechPattern("List item");
  sm_.ExpectSpeechPattern("1 of 1");
  sm_.ExpectSpeechPattern("List");
  sm_.ExpectSpeechPattern("with 1 item");

  sm_.Replay();
}

IN_PROC_BROWSER_TEST_F(PickerAccessibilityBrowserTest,
                       FocusingItemWithSubmenuAnnouncesMenuRole) {
  std::unique_ptr<views::Widget> widget =
      ash::TestWidgetBuilder()
          .SetWidgetType(views::Widget::InitParams::TYPE_WINDOW_FRAMELESS)
          .BuildClientOwnsWidget();
  auto* view = widget->SetContentsView(
      std::make_unique<ash::PickerItemWithSubmenuView>());
  view->SetLeadingIcon(ui::ImageModel::FromImage(gfx::test::CreateImage(1)));
  view->SetText(u"meow");

  sm_.Call([view]() { view->RequestFocus(); });

  sm_.ExpectSpeechPattern("meow");
  sm_.ExpectSpeechPattern("Button");
  sm_.ExpectSpeechPattern("has pop up");
  sm_.ExpectSpeechPattern("Press * to activate");
  sm_.Replay();
}

IN_PROC_BROWSER_TEST_F(PickerAccessibilityBrowserTest,
                       FocusingEmojiResultButtonAnnouncesNameOfEmoji) {
  std::unique_ptr<views::Widget> widget =
      ash::TestWidgetBuilder()
          .SetWidgetType(views::Widget::InitParams::TYPE_WINDOW_FRAMELESS)
          .BuildClientOwnsWidget();
  auto* view =
      widget->SetContentsView(std::make_unique<ash::PickerEmojiBarView>(
          /*delegate=*/nullptr, /*picker_width=*/1000));
  view->SetSearchResults({ash::PickerEmojiResult::Emoji(u"😊", u"happy")});

  sm_.Call([view]() { view->GetItemsForTesting().front()->RequestFocus(); });

  sm_.ExpectSpeechPattern("happy emoji");
  sm_.ExpectSpeechPattern("Button");
  sm_.ExpectSpeechPattern("Press * to activate");
  sm_.Replay();
}

IN_PROC_BROWSER_TEST_F(PickerAccessibilityBrowserTest,
                       FocusingSymbolResultButtonAnnouncesNameOfSymbol) {
  std::unique_ptr<views::Widget> widget =
      ash::TestWidgetBuilder()
          .SetWidgetType(views::Widget::InitParams::TYPE_WINDOW_FRAMELESS)
          .BuildClientOwnsWidget();
  auto* view =
      widget->SetContentsView(std::make_unique<ash::PickerEmojiBarView>(
          /*delegate=*/nullptr, /*picker_width=*/1000));
  view->SetSearchResults({ash::PickerEmojiResult::Symbol(u"♬", u"music")});

  sm_.Call([view]() { view->GetItemsForTesting().front()->RequestFocus(); });

  sm_.ExpectSpeechPattern("music");
  sm_.ExpectSpeechPattern("Button");
  sm_.ExpectSpeechPattern("Press * to activate");
  sm_.Replay();
}

IN_PROC_BROWSER_TEST_F(PickerAccessibilityBrowserTest,
                       FocusingEmoticonResultButtonAnnouncesNameOfEmoticon) {
  std::unique_ptr<views::Widget> widget =
      ash::TestWidgetBuilder()
          .SetWidgetType(views::Widget::InitParams::TYPE_WINDOW_FRAMELESS)
          .BuildClientOwnsWidget();
  auto* view =
      widget->SetContentsView(std::make_unique<ash::PickerEmojiBarView>(
          /*delegate=*/nullptr, /*picker_width=*/1000));
  view->SetSearchResults(
      {ash::PickerEmojiResult::Emoticon(u"(°□°)", u"surprise")});

  sm_.Call([view]() { view->GetItemsForTesting().front()->RequestFocus(); });

  sm_.ExpectSpeechPattern("surprise emoticon");
  sm_.ExpectSpeechPattern("Button");
  sm_.ExpectSpeechPattern("Press * to activate");
  sm_.Replay();
}

IN_PROC_BROWSER_TEST_F(PickerAccessibilityBrowserTest,
                       StoppingSearchAnnouncesEmojiResults) {
  std::unique_ptr<views::Widget> widget =
      ash::TestWidgetBuilder()
          .SetWidgetType(views::Widget::InitParams::TYPE_WINDOW_FRAMELESS)
          .BuildClientOwnsWidget();
  ash::MockPickerSearchResultsViewDelegate mock_delegate;
  auto* view =
      widget->SetContentsView(std::make_unique<ash::PickerSearchResultsView>(
          &mock_delegate, /*picker_width=*/1000, /*asset_fetcher=*/nullptr,
          /*submenu_controller=*/nullptr, /*preview_controller=*/nullptr));

  sm_.Call([view]() {
    view->SetNumEmojiResultsForA11y(5);
    view->SearchStopped(/*illustration=*/{}, /*description=*/u"");
  });
  sm_.ExpectSpeechPattern("5 emojis. No other results.");
  sm_.ExpectSpeechPattern("Status");
  sm_.Replay();
}

IN_PROC_BROWSER_TEST_F(PickerAccessibilityBrowserTest,
                       StoppingSearchAnnouncesNoResults) {
  std::unique_ptr<views::Widget> widget =
      ash::TestWidgetBuilder()
          .SetWidgetType(views::Widget::InitParams::TYPE_WINDOW_FRAMELESS)
          .BuildClientOwnsWidget();
  ash::MockPickerSearchResultsViewDelegate mock_delegate;
  auto* view =
      widget->SetContentsView(std::make_unique<ash::PickerSearchResultsView>(
          &mock_delegate, /*picker_width=*/1000, /*asset_fetcher=*/nullptr,
          /*submenu_controller=*/nullptr, /*preview_controller=*/nullptr));

  sm_.Call([view]() {
    view->SearchStopped(/*illustration=*/{}, /*description=*/u"");
  });
  sm_.ExpectSpeechPattern(l10n_util::GetStringUTF8(IDS_PICKER_NO_RESULTS_TEXT));
  sm_.ExpectSpeechPattern("Status");
  sm_.Replay();
}

IN_PROC_BROWSER_TEST_F(PickerAccessibilityBrowserTest,
                       ShowingFeatureTourAnnouncesContents) {
  ash::PickerFeatureTour feature_tour;

  sm_.Call([this, &feature_tour]() {
    feature_tour.MaybeShowForFirstUse(
        browser()->profile()->GetPrefs(),
        ash::PickerFeatureTour::EditorStatus::kEligible, base::DoNothing(),
        base::DoNothing());
  });

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  sm_.ExpectSpeechPattern("*insert content*");
#endif
  sm_.ExpectSpeechPattern("Dialog");
  sm_.ExpectSpeechPattern("Get started");
  sm_.ExpectSpeechPattern("Button");
  sm_.Replay();
}

IN_PROC_BROWSER_TEST_F(PickerAccessibilityBrowserTest,
                       InsertingAnnouncesInsertionBeforeTextfieldRefocus) {
  ash::PickerController controller;
  PickerClientImpl client(&controller, user_manager::UserManager::Get());
  std::unique_ptr<views::Widget> textfield_widget =
      ash::TestWidgetBuilder()
          .SetWidgetType(views::Widget::InitParams::TYPE_WINDOW_FRAMELESS)
          .BuildClientOwnsWidget();
  auto* textfield =
      textfield_widget->SetContentsView(std::make_unique<views::Textfield>());
  textfield->GetViewAccessibility().SetName(u"textfield");
  sm_.Call([&textfield]() { textfield->RequestFocus(); });
  sm_.ExpectSpeechPattern("textfield");
  sm_.ExpectSpeechPattern("Edit text");

  sm_.Call([&controller]() {
    controller.ToggleWidget();
    controller.CloseWidgetThenInsertResultOnNextFocus(
        ash::PickerTextResult(u"abc"));
  });

  sm_.ExpectSpeechPattern("Picker");
  sm_.ExpectSpeechPattern(", window");
  sm_.ExpectSpeechPattern("Picker");
  sm_.ExpectSpeechPattern("Status");
  sm_.ExpectSpeechPattern("Inserting selected result");
  sm_.ExpectSpeechPattern("textfield");
  sm_.ExpectSpeechPattern("abc");
  sm_.ExpectSpeechPattern("Edit text");
  sm_.Replay();
}

}  // namespace
