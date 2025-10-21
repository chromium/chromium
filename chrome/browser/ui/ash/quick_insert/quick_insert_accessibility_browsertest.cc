// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"
#include "ash/quick_insert/metrics/quick_insert_performance_metrics.h"
#include "ash/quick_insert/model/quick_insert_action_type.h"
#include "ash/quick_insert/model/quick_insert_search_results_section.h"
#include "ash/quick_insert/quick_insert_controller.h"
#include "ash/quick_insert/quick_insert_search_result.h"
#include "ash/quick_insert/views/mock_quick_insert_search_results_view_delegate.h"
#include "ash/quick_insert/views/quick_insert_emoji_bar_view.h"
#include "ash/quick_insert/views/quick_insert_emoji_item_view.h"
#include "ash/quick_insert/views/quick_insert_feature_tour.h"
#include "ash/quick_insert/views/quick_insert_image_item_view.h"
#include "ash/quick_insert/views/quick_insert_item_with_submenu_view.h"
#include "ash/quick_insert/views/quick_insert_key_event_handler.h"
#include "ash/quick_insert/views/quick_insert_list_item_view.h"
#include "ash/quick_insert/views/quick_insert_preview_bubble_controller.h"
#include "ash/quick_insert/views/quick_insert_pseudo_focus.h"
#include "ash/quick_insert/views/quick_insert_search_bar_textfield.h"
#include "ash/quick_insert/views/quick_insert_search_field_view.h"
#include "ash/quick_insert/views/quick_insert_search_results_view.h"
#include "ash/quick_insert/views/quick_insert_section_list_view.h"
#include "ash/quick_insert/views/quick_insert_section_view.h"
#include "ash/quick_insert/views/quick_insert_view_delegate.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/icon_button.h"
#include "base/strings/string_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#include "chrome/browser/ash/accessibility/chromevox_test_utils.h"
#include "chrome/browser/ash/accessibility/speech_monitor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/quick_insert/quick_insert_client_impl.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/strings/grit/components_strings.h"
#include "components/user_manager/user_manager.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/browsertest_util.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/test/test_widget_builder.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace {

// These tests are not meant to be end-to-end tests.
// They should test Quick Insert view components on a low-level.
// They are browser tests in order to bring in ChromeVox so that we can test
// announcements.
class QuickInsertAccessibilityBrowserTest : public InProcessBrowserTest {
 public:
  QuickInsertAccessibilityBrowserTest() {
    // TODO(crbug.com/433771715): This test is forced to use ChromeVox in
    // manifest v2 due to flakiness on MSAN. Parameterize this test on the
    // manifest version and ensure the mv3 variants pass.
    scoped_feature_list_.InitWithFeatureStates(
        {{::features::kAccessibilityManifestV3ChromeVox, false}});
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    chromevox_test_utils_ = std::make_unique<ash::ChromeVoxTestUtils>();
    chromevox_test_utils_->EnableChromeVox();

    ash::QuickInsertController::DisableFeatureTourForTesting();
  }

  void TearDownOnMainThread() override {
    chromevox_test_utils_.reset();
    InProcessBrowserTest::TearDownOnMainThread();
  }

 protected:
  ash::test::SpeechMonitor* sm() { return chromevox_test_utils_->sm(); }

  std::unique_ptr<ash::ChromeVoxTestUtils> chromevox_test_utils_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(QuickInsertAccessibilityBrowserTest,
                       FocusingEmptySearchFieldAnnouncesPlaceholder) {
  std::unique_ptr<views::Widget> widget =
      views::test::TestWidgetBuilder()
          .SetWidgetType(views::Widget::InitParams::TYPE_WINDOW_FRAMELESS)
          .BuildClientOwnsWidget();
  ash::QuickInsertKeyEventHandler key_event_handler;
  ash::QuickInsertPerformanceMetrics metrics;
  auto* view =
      widget->SetContentsView(std::make_unique<ash::QuickInsertSearchFieldView>(
          base::DoNothing(), base::DoNothing(), &key_event_handler, &metrics));
  view->SetPlaceholderText(u"cat");

  sm()->Call([view]() { view->RequestFocus(); });

  sm()->ExpectSpeechPattern("cat");
  sm()->ExpectSpeechPattern("Edit text");
  sm()->Replay();
}

IN_PROC_BROWSER_TEST_F(QuickInsertAccessibilityBrowserTest,
                       SetDescendantAnnouncesDescendant) {
  std::unique_ptr<views::Widget> widget =
      views::test::TestWidgetBuilder()
          .SetWidgetType(views::Widget::InitParams::TYPE_WINDOW_FRAMELESS)
          .BuildClientOwnsWidget();
  ash::QuickInsertKeyEventHandler key_event_handler;
  ash::QuickInsertPerformanceMetrics metrics;
  auto* container_view =
      widget->SetContentsView(views::Builder<views::BoxLayoutView>().Build());
  auto* search_field_view = container_view->AddChildView(
      std::make_unique<ash::QuickInsertSearchFieldView>(
          base::DoNothing(), base::DoNothing(), &key_event_handler, &metrics));
  auto* other_view =
      container_view->AddChildView(std::make_unique<views::Label>(u"test"));
  search_field_view->SetPlaceholderText(u"cat");

  sm()->Call([search_field_view]() { search_field_view->RequestFocus(); });

  sm()->ExpectSpeechPattern("cat");
  sm()->ExpectSpeechPattern("Edit text");

  sm()->Call([search_field_view, other_view]() {
    search_field_view->SetTextfieldActiveDescendant(other_view);
  });

  sm()->ExpectSpeechPattern("test");
  sm()->Replay();
}

IN_PROC_BROWSER_TEST_F(QuickInsertAccessibilityBrowserTest,
                       SetDescendantAnnouncesDescendantAfterKeyEvent) {
  std::unique_ptr<views::Widget> widget =
      views::test::TestWidgetBuilder()
          .SetWidgetType(views::Widget::InitParams::TYPE_WINDOW_FRAMELESS)
          .BuildClientOwnsWidget();
  ash::QuickInsertKeyEventHandler key_event_handler;
  ash::QuickInsertPerformanceMetrics metrics;
  auto* container_view =
      widget->SetContentsView(views::Builder<views::BoxLayoutView>().Build());
  auto* search_field_view = container_view->AddChildView(
      std::make_unique<ash::QuickInsertSearchFieldView>(
          base::DoNothing(), base::DoNothing(), &key_event_handler, &metrics));
  auto* other_view =
      container_view->AddChildView(std::make_unique<views::Label>(u"test"));
  search_field_view->SetPlaceholderText(u"cat");

  sm()->Call([search_field_view]() { search_field_view->RequestFocus(); });

  sm()->ExpectSpeechPattern("cat");
  sm()->ExpectSpeechPattern("Edit text");

  sm()->Call([search_field_view, other_view]() {
    ui::test::EventGenerator event_generator(
        ash::Shell::Get()->GetPrimaryRootWindow());
    event_generator.PressAndReleaseKey(ui::VKEY_A);
    search_field_view->SetTextfieldActiveDescendant(other_view);
  });

  sm()->ExpectSpeechPattern("A");
  sm()->ExpectSpeechPattern("test");
  sm()->Replay();
}

IN_PROC_BROWSER_TEST_F(QuickInsertAccessibilityBrowserTest,
                       SetDescendantToTextfieldAnnouncesPlaceholder) {
  std::unique_ptr<views::Widget> widget =
      views::test::TestWidgetBuilder()
          .SetWidgetType(views::Widget::InitParams::TYPE_WINDOW_FRAMELESS)
          .BuildClientOwnsWidget();
  ash::QuickInsertKeyEventHandler key_event_handler;
  ash::QuickInsertPerformanceMetrics metrics;
  auto* container_view =
      widget->SetContentsView(views::Builder<views::BoxLayoutView>().Build());
  auto* search_field_view = container_view->AddChildView(
      std::make_unique<ash::QuickInsertSearchFieldView>(
          base::DoNothing(), base::DoNothing(), &key_event_handler, &metrics));
  auto* other_view =
      container_view->AddChildView(std::make_unique<views::Label>(u"test"));
  search_field_view->SetPlaceholderText(u"cat");

  sm()->Call([search_field_view]() { search_field_view->RequestFocus(); });

  sm()->ExpectSpeechPattern("cat");
  sm()->ExpectSpeechPattern("Edit text");

  sm()->Call([search_field_view, other_view]() {
    search_field_view->SetTextfieldActiveDescendant(other_view);
  });

  sm()->ExpectSpeechPattern("test");

  sm()->Call([search_field_view]() {
    search_field_view->SetTextfieldActiveDescendant(
        search_field_view->textfield());
  });

  sm()->ExpectSpeechPattern("cat");
  sm()->ExpectSpeechPattern("Edit text");
  sm()->Replay();
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
    QuickInsertAccessibilityBrowserTest,
    MAYBE_SetDescendantThenFocusingSearchFieldAnnouncesDescendant) {
  std::unique_ptr<views::Widget> widget =
      views::test::TestWidgetBuilder()
          .SetWidgetType(views::Widget::InitParams::TYPE_WINDOW_FRAMELESS)
          .BuildClientOwnsWidget();
  ash::QuickInsertKeyEventHandler key_event_handler;
  ash::QuickInsertPerformanceMetrics metrics;
  auto* container_view =
      widget->SetContentsView(views::Builder<views::BoxLayoutView>().Build());
  auto* search_field_view = container_view->AddChildView(
      std::make_unique<ash::QuickInsertSearchFieldView>(
          base::DoNothing(), base::DoNothing(), &key_event_handler, &metrics));
  auto* other_view =
      container_view->AddChildView(std::make_unique<views::Label>(u"test"));
  search_field_view->SetPlaceholderText(u"cat");

  sm()->Call([search_field_view, other_view]() {
    search_field_view->SetTextfieldActiveDescendant(other_view);
    search_field_view->RequestFocus();
  });

  sm()->ExpectSpeechPattern("cat");
  sm()->ExpectSpeechPattern("Edit text");
  sm()->ExpectSpeechPattern("test");
  sm()->Replay();
}

IN_PROC_BROWSER_TEST_F(QuickInsertAccessibilityBrowserTest,
                       FocusingNonEmptySearchFieldAnnouncesPlaceholder) {
  std::unique_ptr<views::Widget> widget =
      views::test::TestWidgetBuilder()
          .SetWidgetType(views::Widget::InitParams::TYPE_WINDOW_FRAMELESS)
          .BuildClientOwnsWidget();
  ash::QuickInsertKeyEventHandler key_event_handler;
  ash::QuickInsertPerformanceMetrics metrics;
  auto* view =
      widget->SetContentsView(std::make_unique<ash::QuickInsertSearchFieldView>(
          base::DoNothing(), base::DoNothing(), &key_event_handler, &metrics));
  view->SetPlaceholderText(u"cat");
  view->SetQueryText(u"query");

  sm()->Call([view]() { view->RequestFocus(); });

  sm()->ExpectSpeechPattern("cat");
  sm()->ExpectSpeechPattern("Edit text");
  sm()->Replay();
}

IN_PROC_BROWSER_TEST_F(QuickInsertAccessibilityBrowserTest,
                       FocusingSearchFieldClearButtonAnnouncesTooltip) {
  std::unique_ptr<views::Widget> widget =
      views::test::TestWidgetBuilder()
          .SetWidgetType(views::Widget::InitParams::TYPE_WINDOW_FRAMELESS)
          .BuildClientOwnsWidget();
  ash::QuickInsertKeyEventHandler key_event_handler;
  ash::QuickInsertPerformanceMetrics metrics;
  auto* view =
      widget->SetContentsView(std::make_unique<ash::QuickInsertSearchFieldView>(
          base::DoNothing(), base::DoNothing(), &key_event_handler, &metrics));
  view->SetPlaceholderText(u"placeholder");

  sm()->Call([view]() {
    view->textfield_for_testing().InsertOrReplaceText(u"a");
    view->clear_button_for_testing().RequestFocus();
  });

  sm()->ExpectSpeechPattern(
      l10n_util::GetStringUTF8(IDS_APP_LIST_CLEAR_SEARCHBOX));
  sm()->ExpectSpeechPattern("Button");
  sm()->ExpectSpeechPattern("Press * to activate");
  sm()->Replay();
}

IN_PROC_BROWSER_TEST_F(QuickInsertAccessibilityBrowserTest,
                       FocusingSearchFieldBackButtonAnnouncesTooltip) {
  std::unique_ptr<views::Widget> widget =
      views::test::TestWidgetBuilder()
          .SetWidgetType(views::Widget::InitParams::TYPE_WINDOW_FRAMELESS)
          .BuildClientOwnsWidget();
  ash::QuickInsertKeyEventHandler key_event_handler;
  ash::QuickInsertPerformanceMetrics metrics;
  auto* view =
      widget->SetContentsView(std::make_unique<ash::QuickInsertSearchFieldView>(
          base::DoNothing(), base::DoNothing(), &key_event_handler, &metrics));
  view->SetPlaceholderText(u"placeholder");
  view->SetBackButtonVisible(true);

  sm()->Call([view]() { view->back_button_for_testing().RequestFocus(); });

  sm()->ExpectSpeechPattern(l10n_util::GetStringUTF8(IDS_ACCNAME_BACK));
  sm()->ExpectSpeechPattern("Button");
  sm()->ExpectSpeechPattern("Press * to activate");
  sm()->Replay();
}

IN_PROC_BROWSER_TEST_F(QuickInsertAccessibilityBrowserTest,
                       FocusingEmojiBarItemsAnnouncesGrid) {
  std::unique_ptr<views::Widget> widget =
      views::test::TestWidgetBuilder()
          .SetWidgetType(views::Widget::InitParams::TYPE_WINDOW_FRAMELESS)
          .BuildClientOwnsWidget();
  auto* view =
      widget->SetContentsView(std::make_unique<ash::QuickInsertEmojiBarView>(
          /*delegate=*/nullptr, /*quick_insert_width=*/1000,
          /*is_gifs_enabled=*/true));
  view->SetSearchResults({
      ash::QuickInsertEmojiResult::Emoji(u"😊", u"happy"),
      ash::QuickInsertEmojiResult::Symbol(u"♬", u"music"),
      ash::QuickInsertEmojiResult::Emoticon(u"(°□°)", u"surprise"),
  });

  sm()->Call([view]() { view->GetItemsForTesting()[0]->RequestFocus(); });

  sm()->ExpectSpeechPattern("happy emoji");
  sm()->ExpectSpeechPattern("Button");
  sm()->ExpectSpeechPattern("row 1 column 1");
  sm()->ExpectSpeechPattern("Table Emojis and GIFs, 1 by 5");

  sm()->Call([view]() { view->GetItemsForTesting()[1]->RequestFocus(); });

  sm()->ExpectSpeechPattern("music");
  sm()->ExpectSpeechPattern("Button");
  sm()->ExpectSpeechPattern("row 1 column 2");

  sm()->Call([view]() { view->GetItemsForTesting()[2]->RequestFocus(); });

  sm()->ExpectSpeechPattern("surprise emoticon");
  sm()->ExpectSpeechPattern("Button");
  sm()->ExpectSpeechPattern("row 1 column 3");

  sm()->Call([view]() { view->gifs_button_for_testing()->RequestFocus(); });

  sm()->ExpectSpeechPattern("GIF");
  sm()->ExpectSpeechPattern("Button");
  sm()->ExpectSpeechPattern("row 1 column 4");

  sm()->Call(
      [view]() { view->more_emojis_button_for_testing()->RequestFocus(); });

  sm()->ExpectSpeechPattern(l10n_util::GetStringUTF8(
      IDS_PICKER_MORE_EMOJIS_AND_GIFS_BUTTON_ACCESSIBLE_NAME));
  sm()->ExpectSpeechPattern("Button");
  sm()->ExpectSpeechPattern("row 1 column 5");

  sm()->Replay();
}

class QuickInsertAccessibilityWithGifsFlagDisabledBrowserTest
    : public QuickInsertAccessibilityBrowserTest {
 public:
  QuickInsertAccessibilityWithGifsFlagDisabledBrowserTest() {
    scoped_feature_list_.InitAndDisableFeature(ash::features::kPickerGifs);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(QuickInsertAccessibilityWithGifsFlagDisabledBrowserTest,
                       FocusingGifsButtonAnnouncesLabel) {
  std::unique_ptr<views::Widget> widget =
      views::test::TestWidgetBuilder()
          .SetWidgetType(views::Widget::InitParams::TYPE_WINDOW_FRAMELESS)
          .BuildClientOwnsWidget();
  auto* view =
      widget->SetContentsView(std::make_unique<ash::QuickInsertEmojiBarView>(
          /*delegate=*/nullptr, /*quick_insert_width=*/100,
          /*is_gifs_enabled=*/true));

  sm()->Call([view]() { view->gifs_button_for_testing()->RequestFocus(); });

  sm()->ExpectSpeechPattern("GIF");
  sm()->ExpectSpeechPattern("Button");
  sm()->ExpectSpeechPattern("Press * to activate");
  sm()->Replay();
}

class QuickInsertAccessibilityWithGifsFlagEnabledBrowserTest
    : public QuickInsertAccessibilityBrowserTest {
 public:
  QuickInsertAccessibilityWithGifsFlagEnabledBrowserTest()
      : scoped_feature_list_(ash::features::kPickerGifs) {}

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(QuickInsertAccessibilityWithGifsFlagEnabledBrowserTest,
                       FocusingGifsToggleAnnouncesPressedState) {
  std::unique_ptr<views::Widget> widget =
      views::test::TestWidgetBuilder()
          .SetWidgetType(views::Widget::InitParams::TYPE_WINDOW_FRAMELESS)
          .BuildClientOwnsWidget();
  auto* view =
      widget->SetContentsView(std::make_unique<ash::QuickInsertEmojiBarView>(
          /*delegate=*/nullptr, /*quick_insert_width=*/100,
          /*is_gifs_enabled=*/true));

  sm()->Call([view]() { view->gifs_button_for_testing()->RequestFocus(); });

  sm()->ExpectSpeechPattern("GIF");
  sm()->ExpectSpeechPattern("Toggle Button");
  sm()->ExpectSpeechPattern("Not pressed");
  sm()->ExpectSpeechPattern("Press * to toggle");
  sm()->Replay();
}

IN_PROC_BROWSER_TEST_F(QuickInsertAccessibilityWithGifsFlagEnabledBrowserTest,
                       TogglingGifsToggleAnnouncesPressedState) {
  std::unique_ptr<views::Widget> widget =
      views::test::TestWidgetBuilder()
          .SetWidgetType(views::Widget::InitParams::TYPE_WINDOW_FRAMELESS)
          .BuildClientOwnsWidget();
  auto* view =
      widget->SetContentsView(std::make_unique<ash::QuickInsertEmojiBarView>(
          /*delegate=*/nullptr, /*quick_insert_width=*/100,
          /*is_gifs_enabled=*/true));
  sm()->Call([view]() { view->gifs_button_for_testing()->RequestFocus(); });
  sm()->ExpectSpeechPattern("Not pressed");
  sm()->ExpectSpeechPattern("Press * to toggle");

  sm()->Call([]() {
    ui::test::EventGenerator event_generator(
        ash::Shell::Get()->GetPrimaryRootWindow());
    event_generator.PressAndReleaseKey(ui::KeyboardCode::VKEY_RETURN);
  });

  sm()->ExpectSpeechPattern("GIF");
  sm()->ExpectSpeechPattern("Toggle Button");
  sm()->ExpectSpeechPattern("Pressed");
  sm()->ExpectSpeechPattern("Press * to toggle");
  sm()->Replay();
}

IN_PROC_BROWSER_TEST_F(QuickInsertAccessibilityBrowserTest,
                       FocusingMoreEmojisAnnouncesTooltip) {
  std::unique_ptr<views::Widget> widget =
      views::test::TestWidgetBuilder()
          .SetWidgetType(views::Widget::InitParams::TYPE_WINDOW_FRAMELESS)
          .BuildClientOwnsWidget();
  auto* view =
      widget->SetContentsView(std::make_unique<ash::QuickInsertEmojiBarView>(
          /*delegate=*/nullptr, /*quick_insert_width=*/100));

  sm()->Call(
      [view]() { view->more_emojis_button_for_testing()->RequestFocus(); });

  sm()->ExpectSpeechPattern(
      l10n_util::GetStringUTF8(IDS_PICKER_MORE_EMOJIS_BUTTON_ACCESSIBLE_NAME));
  sm()->ExpectSpeechPattern("Button");
  sm()->ExpectSpeechPattern("Press * to activate");
  sm()->Replay();
}

IN_PROC_BROWSER_TEST_F(QuickInsertAccessibilityBrowserTest,
                       SectionsAnnouncesHeadings) {
  std::unique_ptr<views::Widget> widget =
      views::test::TestWidgetBuilder()
          .SetWidgetType(views::Widget::InitParams::TYPE_WINDOW_FRAMELESS)
          .BuildClientOwnsWidget();
  auto* view =
      widget->SetContentsView(std::make_unique<ash::QuickInsertSectionListView>(
          /*section_width=*/100, /*asset_fetcher=*/nullptr,
          /*submenu_controller=*/nullptr));
  view->AddSection()->AddTitleLabel(u"Section1");
  view->AddSection()->AddTitleLabel(u"Section2");
  ui::test::EventGenerator event_generator(
      ash::Shell::Get()->GetPrimaryRootWindow());

  sm()->Call([view, &event_generator]() {
    view->RequestFocus();
    event_generator.PressAndReleaseKeyAndModifierKeys(ui::KeyboardCode::VKEY_H,
                                                      ui::EF_COMMAND_DOWN);
  });

  sm()->ExpectSpeechPattern("Section1");
  sm()->ExpectSpeechPattern("Heading");

  sm()->Call([&event_generator]() {
    event_generator.PressAndReleaseKeyAndModifierKeys(ui::KeyboardCode::VKEY_H,
                                                      ui::EF_COMMAND_DOWN);
  });

  sm()->ExpectSpeechPattern("Section2");
  sm()->ExpectSpeechPattern("Heading");
  sm()->Replay();
}

IN_PROC_BROWSER_TEST_F(QuickInsertAccessibilityBrowserTest,
                       FocusingSectionShowAllAnnounces) {
  std::unique_ptr<views::Widget> widget =
      views::test::TestWidgetBuilder()
          .SetWidgetType(views::Widget::InitParams::TYPE_WINDOW_FRAMELESS)
          .BuildClientOwnsWidget();
  auto* view =
      widget->SetContentsView(std::make_unique<ash::QuickInsertSectionListView>(
          /*section_width=*/100, /*asset_fetcher=*/nullptr,
          /*submenu_controller=*/nullptr));
  ash::QuickInsertSectionView* section_view = view->AddSection();
  section_view->AddTitleLabel(u"Section");
  section_view->AddTitleTrailingLink(u"Show all", u"cat", base::DoNothing());

  sm()->Call([section_view]() {
    section_view->title_trailing_link_for_testing()->RequestFocus();
  });

  sm()->ExpectSpeechPattern("cat");
  sm()->ExpectSpeechPattern("Button");
  sm()->ExpectSpeechPattern("Press * to activate");
  sm()->Replay();
}

IN_PROC_BROWSER_TEST_F(QuickInsertAccessibilityBrowserTest,
                       ListItemAnnouncesTextWithAction) {
  std::unique_ptr<views::Widget> widget =
      views::test::TestWidgetBuilder()
          .SetWidgetType(views::Widget::InitParams::TYPE_WINDOW_FRAMELESS)
          .BuildClientOwnsWidget();
  auto* view =
      widget->SetContentsView(std::make_unique<ash::QuickInsertSectionListView>(
          /*section_width=*/100, /*asset_fetcher=*/nullptr,
          /*submenu_controller=*/nullptr));
  ash::QuickInsertSectionView* section = view->AddSection();
  section->AddTitleLabel(u"Section1");
  ash::QuickInsertListItemView* item = section->AddListItem(
      std::make_unique<ash::QuickInsertListItemView>(base::DoNothing()));
  item->SetLeadingIcon(ui::ImageModel::FromImage(gfx::test::CreateImage(1)));
  item->SetPrimaryText(u"primary");
  item->SetSecondaryText(u"secondary");
  item->SetBadgeAction(ash::QuickInsertActionType::kInsert);
  item->SetBadgeVisible(true);

  sm()->Call([item]() { item->RequestFocus(); });

  sm()->ExpectSpeechPattern("Insert primary, secondary");
  sm()->ExpectSpeechPattern("Button");
  sm()->ExpectSpeechPattern("Press * to activate");
  sm()->Replay();
}

IN_PROC_BROWSER_TEST_F(QuickInsertAccessibilityBrowserTest,
                       ListItemAnnouncesPreviewMetadata) {
  ash::QuickInsertPreviewBubbleController preview_controller;
  std::unique_ptr<views::Widget> widget =
      views::test::TestWidgetBuilder()
          .SetWidgetType(views::Widget::InitParams::TYPE_WINDOW_FRAMELESS)
          .BuildClientOwnsWidget();
  auto* view =
      widget->SetContentsView(std::make_unique<ash::QuickInsertSectionListView>(
          /*section_width=*/100, /*asset_fetcher=*/nullptr,
          /*submenu_controller=*/nullptr));
  ash::QuickInsertSectionView* section = view->AddSection();
  section->AddTitleLabel(u"Section1");
  ash::QuickInsertListItemView* item = section->AddListItem(
      std::make_unique<ash::QuickInsertListItemView>(base::DoNothing()));
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

  sm()->Call([item]() { item->RequestFocus(); });

  sm()->ExpectSpeechPattern("Button");
  sm()->ExpectSpeechPattern("Edited · Dec 23");
  sm()->ExpectSpeechPattern("Press * to activate");
  sm()->Replay();
}

IN_PROC_BROWSER_TEST_F(QuickInsertAccessibilityBrowserTest,
                       ImageRowItemAnnouncesTitle) {
  std::unique_ptr<views::Widget> widget =
      views::test::TestWidgetBuilder()
          .SetWidgetType(views::Widget::InitParams::TYPE_WINDOW_FRAMELESS)
          .BuildClientOwnsWidget();
  auto* view =
      widget->SetContentsView(std::make_unique<ash::QuickInsertSectionListView>(
          /*section_width=*/100, /*asset_fetcher=*/nullptr,
          /*submenu_controller=*/nullptr));
  ash::QuickInsertSectionView* section = view->AddSection();
  section->AddTitleLabel(u"Section1");
  section->SetImageRowProperties(u"Image Row", base::DoNothing(),
                                 u"More Items");
  ash::QuickInsertImageItemView* item =
      section->AddImageRowItem(std::make_unique<ash::QuickInsertImageItemView>(
          std::make_unique<views::ImageView>(
              ui::ImageModel::FromImage(gfx::test::CreateImage(1))),
          u"title1", base::DoNothing()));
  item->SetAction(ash::QuickInsertActionType::kInsert);
  section->AddImageRowItem(std::make_unique<ash::QuickInsertImageItemView>(
      std::make_unique<views::ImageView>(
          ui::ImageModel::FromImage(gfx::test::CreateImage(1))),
      u"title2", base::DoNothing()));

  sm()->Call([item]() { item->RequestFocus(); });

  sm()->ExpectSpeechPattern("Insert title1");
  sm()->ExpectSpeechPattern("Button");
  sm()->ExpectSpeechPattern("row 1 column 1");
  sm()->ExpectSpeechPattern("Table Image Row, 1 by 3");
  sm()->ExpectSpeechPattern("Press * to activate");
  sm()->Replay();
}

IN_PROC_BROWSER_TEST_F(QuickInsertAccessibilityBrowserTest,
                       ImageRowMoreItemsButtonAnnouncesTooltip) {
  std::unique_ptr<views::Widget> widget =
      views::test::TestWidgetBuilder()
          .SetWidgetType(views::Widget::InitParams::TYPE_WINDOW_FRAMELESS)
          .BuildClientOwnsWidget();
  auto* view =
      widget->SetContentsView(std::make_unique<ash::QuickInsertSectionListView>(
          /*section_width=*/100, /*asset_fetcher=*/nullptr,
          /*submenu_controller=*/nullptr));
  ash::QuickInsertSectionView* section = view->AddSection();
  section->AddTitleLabel(u"Section1");
  section->SetImageRowProperties(u"Image Row", base::DoNothing(),
                                 u"More Items");
  section->AddImageRowItem(std::make_unique<ash::QuickInsertImageItemView>(
      std::make_unique<views::ImageView>(
          ui::ImageModel::FromImage(gfx::test::CreateImage(1))),
      u"title", base::DoNothing()));

  sm()->Call([section]() {
    section->GetImageRowMoreItemsButtonForTesting()->RequestFocus();
  });

  sm()->ExpectSpeechPattern("More Items");
  sm()->ExpectSpeechPattern("Button");
  sm()->ExpectSpeechPattern("row 1 column 2");
  sm()->ExpectSpeechPattern("Table Image Row, 1 by 2");
  sm()->ExpectSpeechPattern("Press * to activate");
  sm()->Replay();
}

IN_PROC_BROWSER_TEST_F(QuickInsertAccessibilityBrowserTest,
                       SetDescendantToImageGridItemAnnouncesTitle) {
  std::unique_ptr<views::Widget> widget =
      views::test::TestWidgetBuilder()
          .SetWidgetType(views::Widget::InitParams::TYPE_WINDOW_FRAMELESS)
          .BuildClientOwnsWidget();
  ash::QuickInsertKeyEventHandler key_event_handler;
  ash::QuickInsertPerformanceMetrics metrics;
  auto* container_view =
      widget->SetContentsView(views::Builder<views::BoxLayoutView>().Build());
  auto* search_field_view = container_view->AddChildView(
      std::make_unique<ash::QuickInsertSearchFieldView>(
          base::DoNothing(), base::DoNothing(), &key_event_handler, &metrics));
  search_field_view->SetPlaceholderText(u"cat");
  auto* contents_view = container_view->AddChildView(
      std::make_unique<ash::QuickInsertSectionListView>(
          /*section_width=*/100, /*asset_fetcher=*/nullptr,
          /*submenu_controller=*/nullptr));
  ash::QuickInsertSectionView* section = contents_view->AddSection();
  section->AddTitleLabel(u"Section1");
  ash::QuickInsertImageItemView* item1 =
      section->AddImageGridItem(std::make_unique<ash::QuickInsertImageItemView>(
          std::make_unique<views::ImageView>(
              ui::ImageModel::FromImage(gfx::test::CreateImage(1))),
          u"title1", base::DoNothing()));
  item1->SetAction(ash::QuickInsertActionType::kInsert);

  ash::QuickInsertImageItemView* item2 =
      section->AddImageGridItem(std::make_unique<ash::QuickInsertImageItemView>(
          std::make_unique<views::ImageView>(
              ui::ImageModel::FromImage(gfx::test::CreateImage(1))),
          u"title2", base::DoNothing()));
  item2->SetAction(ash::QuickInsertActionType::kOpen);

  ash::QuickInsertImageItemView* item3 =
      section->AddImageGridItem(std::make_unique<ash::QuickInsertImageItemView>(
          std::make_unique<views::ImageView>(
              ui::ImageModel::FromImage(gfx::test::CreateImage(1))),
          u"title3", base::DoNothing()));

  sm()->Call([search_field_view]() { search_field_view->RequestFocus(); });

  sm()->Call([search_field_view, item1]() {
    search_field_view->SetTextfieldActiveDescendant(item1);
  });

  sm()->ExpectSpeechPattern("Insert title1");
  sm()->ExpectSpeechPattern("Button");
  sm()->ExpectSpeechPattern("List item");
  sm()->ExpectSpeechPattern("1 of 3");

  sm()->Call([search_field_view, item2]() {
    search_field_view->SetTextfieldActiveDescendant(item2);
  });

  // TODO: b/362129770 - item2 should not have "List end".
  sm()->ExpectSpeechPattern("Open title2");
  sm()->ExpectSpeechPattern("Button");
  sm()->ExpectSpeechPattern("List item");
  sm()->ExpectSpeechPattern("3 of 3");
  sm()->ExpectSpeechPattern("List end");

  sm()->Call([search_field_view, item3]() {
    search_field_view->SetTextfieldActiveDescendant(item3);
  });

  // TODO: b/362129770 - item3 should have "List end".
  sm()->ExpectSpeechPattern("title3");
  sm()->ExpectSpeechPattern("Button");
  sm()->ExpectSpeechPattern("List item");
  sm()->ExpectSpeechPattern("2 of 3");

  sm()->Replay();
}

IN_PROC_BROWSER_TEST_F(QuickInsertAccessibilityBrowserTest,
                       FocusingItemInSectionListViewAnnouncesSizeAndPosition) {
  std::unique_ptr<views::Widget> widget =
      views::test::TestWidgetBuilder()
          .SetWidgetType(views::Widget::InitParams::TYPE_WINDOW_FRAMELESS)
          .BuildClientOwnsWidget();
  auto* view =
      widget->SetContentsView(std::make_unique<ash::QuickInsertSectionListView>(
          /*section_width=*/100, /*asset_fetcher=*/nullptr,
          /*submenu_controller=*/nullptr));
  ash::QuickInsertSectionView* section1 = view->AddSection();
  ash::QuickInsertListItemView* item1 = section1->AddListItem(
      std::make_unique<ash::QuickInsertListItemView>(base::DoNothing()));
  item1->SetPrimaryText(u"item1");
  ash::QuickInsertListItemView* item2 = section1->AddListItem(
      std::make_unique<ash::QuickInsertListItemView>(base::DoNothing()));
  item2->SetPrimaryText(u"item2");
  ash::QuickInsertSectionView* section2 = view->AddSection();
  ash::QuickInsertListItemView* item3 = section2->AddListItem(
      std::make_unique<ash::QuickInsertListItemView>(base::DoNothing()));
  item3->SetPrimaryText(u"item3");

  sm()->Call([item1]() { item1->RequestFocus(); });

  sm()->ExpectSpeechPattern("item1");
  sm()->ExpectSpeechPattern("Button");
  sm()->ExpectSpeechPattern("List item");
  sm()->ExpectSpeechPattern("1 of 2");
  sm()->ExpectSpeechPattern("List");
  sm()->ExpectSpeechPattern("with 2 items");

  sm()->Call([item2]() { item2->RequestFocus(); });

  sm()->ExpectSpeechPattern("item2");
  sm()->ExpectSpeechPattern("Button");
  sm()->ExpectSpeechPattern("List item");
  sm()->ExpectSpeechPattern("2 of 2");

  sm()->Call([item3]() { item3->RequestFocus(); });

  sm()->ExpectSpeechPattern("item3");
  sm()->ExpectSpeechPattern("Button");
  sm()->ExpectSpeechPattern("List item");
  sm()->ExpectSpeechPattern("1 of 1");
  sm()->ExpectSpeechPattern("List");
  sm()->ExpectSpeechPattern("with 1 item");

  sm()->Replay();
}

IN_PROC_BROWSER_TEST_F(QuickInsertAccessibilityBrowserTest,
                       FocusingItemWithSubmenuAnnouncesMenuRole) {
  std::unique_ptr<views::Widget> widget =
      views::test::TestWidgetBuilder()
          .SetWidgetType(views::Widget::InitParams::TYPE_WINDOW_FRAMELESS)
          .BuildClientOwnsWidget();
  auto* view = widget->SetContentsView(
      std::make_unique<ash::QuickInsertItemWithSubmenuView>());
  view->SetLeadingIcon(ui::ImageModel::FromImage(gfx::test::CreateImage(1)));
  view->SetText(u"meow");

  sm()->Call([view]() { view->RequestFocus(); });

  sm()->ExpectSpeechPattern("meow");
  sm()->ExpectSpeechPattern("Button");
  sm()->ExpectSpeechPattern("has pop up");
  sm()->ExpectSpeechPattern("Press * to activate");
  sm()->Replay();
}

IN_PROC_BROWSER_TEST_F(QuickInsertAccessibilityBrowserTest,
                       FocusingEmojiResultButtonAnnouncesNameOfEmoji) {
  std::unique_ptr<views::Widget> widget =
      views::test::TestWidgetBuilder()
          .SetWidgetType(views::Widget::InitParams::TYPE_WINDOW_FRAMELESS)
          .BuildClientOwnsWidget();
  auto* view =
      widget->SetContentsView(std::make_unique<ash::QuickInsertEmojiBarView>(
          /*delegate=*/nullptr, /*quick_insert_width=*/1000));
  view->SetSearchResults({ash::QuickInsertEmojiResult::Emoji(u"😊", u"happy")});

  sm()->Call([view]() { view->GetItemsForTesting().front()->RequestFocus(); });

  sm()->ExpectSpeechPattern("happy emoji");
  sm()->ExpectSpeechPattern("Button");
  sm()->ExpectSpeechPattern("Press * to activate");
  sm()->Replay();
}

IN_PROC_BROWSER_TEST_F(QuickInsertAccessibilityBrowserTest,
                       FocusingSymbolResultButtonAnnouncesNameOfSymbol) {
  std::unique_ptr<views::Widget> widget =
      views::test::TestWidgetBuilder()
          .SetWidgetType(views::Widget::InitParams::TYPE_WINDOW_FRAMELESS)
          .BuildClientOwnsWidget();
  auto* view =
      widget->SetContentsView(std::make_unique<ash::QuickInsertEmojiBarView>(
          /*delegate=*/nullptr, /*quick_insert_width=*/1000));
  view->SetSearchResults({ash::QuickInsertEmojiResult::Symbol(u"♬", u"music")});

  sm()->Call([view]() { view->GetItemsForTesting().front()->RequestFocus(); });

  sm()->ExpectSpeechPattern("music");
  sm()->ExpectSpeechPattern("Button");
  sm()->ExpectSpeechPattern("Press * to activate");
  sm()->Replay();
}

IN_PROC_BROWSER_TEST_F(QuickInsertAccessibilityBrowserTest,
                       FocusingEmoticonResultButtonAnnouncesNameOfEmoticon) {
  std::unique_ptr<views::Widget> widget =
      views::test::TestWidgetBuilder()
          .SetWidgetType(views::Widget::InitParams::TYPE_WINDOW_FRAMELESS)
          .BuildClientOwnsWidget();
  auto* view =
      widget->SetContentsView(std::make_unique<ash::QuickInsertEmojiBarView>(
          /*delegate=*/nullptr, /*quick_insert_width=*/1000));
  view->SetSearchResults(
      {ash::QuickInsertEmojiResult::Emoticon(u"(°□°)", u"surprise")});

  sm()->Call([view]() { view->GetItemsForTesting().front()->RequestFocus(); });

  sm()->ExpectSpeechPattern("surprise emoticon");
  sm()->ExpectSpeechPattern("Button");
  sm()->ExpectSpeechPattern("Press * to activate");
  sm()->Replay();
}

IN_PROC_BROWSER_TEST_F(QuickInsertAccessibilityBrowserTest,
                       StoppingSearchAnnouncesEmojiResults) {
  std::unique_ptr<views::Widget> widget =
      views::test::TestWidgetBuilder()
          .SetWidgetType(views::Widget::InitParams::TYPE_WINDOW_FRAMELESS)
          .BuildClientOwnsWidget();
  ash::MockQuickInsertSearchResultsViewDelegate mock_delegate;
  auto* view = widget->SetContentsView(
      std::make_unique<ash::QuickInsertSearchResultsView>(
          &mock_delegate, /*quick_insert_width=*/1000,
          /*asset_fetcher=*/nullptr,
          /*submenu_controller=*/nullptr, /*preview_controller=*/nullptr));

  sm()->Call([view]() {
    view->SetNumEmojiResultsForA11y(5);
    view->SearchStopped(/*illustration=*/{}, /*description=*/u"");
  });
  sm()->ExpectSpeechPattern("5 emojis. No other results.");
  sm()->ExpectSpeechPattern("Status");
  sm()->Replay();
}

IN_PROC_BROWSER_TEST_F(QuickInsertAccessibilityBrowserTest,
                       StoppingSearchAnnouncesNoResults) {
  std::unique_ptr<views::Widget> widget =
      views::test::TestWidgetBuilder()
          .SetWidgetType(views::Widget::InitParams::TYPE_WINDOW_FRAMELESS)
          .BuildClientOwnsWidget();
  ash::MockQuickInsertSearchResultsViewDelegate mock_delegate;
  auto* view = widget->SetContentsView(
      std::make_unique<ash::QuickInsertSearchResultsView>(
          &mock_delegate, /*quick_insert_width=*/1000,
          /*asset_fetcher=*/nullptr,
          /*submenu_controller=*/nullptr, /*preview_controller=*/nullptr));

  sm()->Call([view]() {
    view->SearchStopped(/*illustration=*/{}, /*description=*/u"");
  });
  sm()->ExpectSpeechPattern(
      l10n_util::GetStringUTF8(IDS_PICKER_NO_RESULTS_TEXT));
  sm()->ExpectSpeechPattern("Status");
  sm()->Replay();
}

IN_PROC_BROWSER_TEST_F(QuickInsertAccessibilityBrowserTest,
                       ShowingFeatureTourAnnouncesContents) {
  ash::QuickInsertFeatureTour feature_tour;

  sm()->Call([this, &feature_tour]() {
    feature_tour.MaybeShowForFirstUse(
        browser()->profile()->GetPrefs(),
        ash::QuickInsertFeatureTour::EditorStatus::kEligible, base::DoNothing(),
        base::DoNothing());
  });

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  sm()->ExpectSpeechPattern("*insert content*");
#endif
  sm()->ExpectSpeechPattern("Dialog");
  sm()->ExpectSpeechPattern("Get started");
  sm()->ExpectSpeechPattern("Button");
  sm()->Replay();
}

IN_PROC_BROWSER_TEST_F(QuickInsertAccessibilityBrowserTest,
                       InsertingAnnouncesInsertionBeforeTextfieldRefocus) {
  ash::QuickInsertController controller;
  QuickInsertClientImpl client(&controller, user_manager::UserManager::Get());
  std::unique_ptr<views::Widget> textfield_widget =
      views::test::TestWidgetBuilder()
          .SetWidgetType(views::Widget::InitParams::TYPE_WINDOW_FRAMELESS)
          .BuildClientOwnsWidget();
  auto* textfield =
      textfield_widget->SetContentsView(std::make_unique<views::Textfield>());
  textfield->GetViewAccessibility().SetName(u"textfield");
  sm()->Call([&textfield]() { textfield->RequestFocus(); });
  sm()->ExpectSpeechPattern("textfield");
  sm()->ExpectSpeechPattern("Edit text");

  sm()->Call([&controller]() {
    controller.ToggleWidget();
    controller.CloseWidgetThenInsertResultOnNextFocus(
        ash::QuickInsertTextResult(u"abc"));
  });

  sm()->ExpectSpeechPattern("Quick Insert");
  sm()->ExpectSpeechPattern(", window");
  sm()->ExpectSpeechPattern("Quick Insert");
  sm()->ExpectSpeechPattern("Status");
  sm()->ExpectSpeechPattern("Inserting selected result");
  sm()->ExpectSpeechPattern("textfield");
  sm()->ExpectSpeechPattern("abc");
  sm()->ExpectSpeechPattern("Edit text");
  sm()->Replay();
}

}  // namespace
