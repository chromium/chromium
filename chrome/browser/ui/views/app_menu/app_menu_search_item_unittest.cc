// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/app_menu/app_menu_search_item.h"

#include <memory>
#include <string>
#include <vector>

#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/fuzzy_search/fuzzy_finder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/actions/actions.h"

namespace {

TEST(AppMenuSearchItemTest, BasicProperties) {
  std::vector<std::u16string> synonyms = {u"tab", u"create"};
  auto action_item = actions::ActionItem::Builder()
                         .SetText(u"New Tab")
                         .SetActionId(kActionNewTab)
                         .AddSynonyms({u"tab", u"create"})
                         .Build();

  auto search_item = AppMenuSearchItem::Builder()
                         .SetType(AppMenuSearchItem::Type::kAction)
                         .SetAction(action_item.get())
                         .SetTitle(u"New Tab")
                         .SetSecondaryText(u"Tools")
                         .Build();

  EXPECT_EQ(search_item->GetType(), AppMenuSearchItem::Type::kAction);
  EXPECT_EQ(search_item->GetTitle(), u"New Tab");
  EXPECT_EQ(search_item->GetSecondaryText(), u"Tools");
  EXPECT_EQ(search_item->GetSynonyms(), synonyms);
  EXPECT_EQ(search_item->GetActionItem(), action_item.get());
}

TEST(AppMenuSearchItemTest, DefaultSecondaryTextAndSynonyms) {
  auto action_item = actions::ActionItem::Builder()
                         .SetText(u"Downloads")
                         .SetActionId(kActionShowDownloads)
                         .Build();

  auto search_item = AppMenuSearchItem::Builder()
                         .SetType(AppMenuSearchItem::Type::kAction)
                         .SetAction(action_item.get())
                         .SetTitle(u"Downloads")
                         .Build();

  EXPECT_EQ(search_item->GetType(), AppMenuSearchItem::Type::kAction);
  EXPECT_EQ(search_item->GetTitle(), u"Downloads");
  EXPECT_TRUE(search_item->GetSecondaryText().empty());
  EXPECT_TRUE(search_item->GetSynonyms().empty());
  EXPECT_EQ(search_item->GetActionItem(), action_item.get());
}

TEST(AppMenuSearchItemTest, Types) {
  auto action_item = actions::ActionItem::Builder()
                         .SetText(u"Item")
                         .SetActionId(kActionNewTab)
                         .Build();

  auto action_entry = AppMenuSearchItem::Builder()
                          .SetType(AppMenuSearchItem::Type::kAction)
                          .SetAction(action_item.get())
                          .SetTitle(u"Action")
                          .Build();
  EXPECT_EQ(action_entry->GetType(), AppMenuSearchItem::Type::kAction);

  auto bookmark_entry = AppMenuSearchItem::Builder()
                            .SetType(AppMenuSearchItem::Type::kBookmark)
                            .SetAction(action_item.get())
                            .SetTitle(u"Bookmark")
                            .SetSecondaryText(u"Folder")
                            .Build();
  EXPECT_EQ(bookmark_entry->GetType(), AppMenuSearchItem::Type::kBookmark);
  EXPECT_EQ(bookmark_entry->GetTitle(), u"Bookmark");
  EXPECT_EQ(bookmark_entry->GetSecondaryText(), u"Folder");

  auto tab_group_entry = AppMenuSearchItem::Builder()
                             .SetType(AppMenuSearchItem::Type::kTabGroup)
                             .SetAction(action_item.get())
                             .SetTitle(u"Work Tabs")
                             .Build();
  EXPECT_EQ(tab_group_entry->GetType(), AppMenuSearchItem::Type::kTabGroup);

  auto recent_tabs_entry = AppMenuSearchItem::Builder()
                               .SetType(AppMenuSearchItem::Type::kRecentTabs)
                               .SetAction(action_item.get())
                               .SetTitle(u"Recent Page")
                               .Build();
  EXPECT_EQ(recent_tabs_entry->GetType(), AppMenuSearchItem::Type::kRecentTabs);
}

TEST(AppMenuSearchItemTest, SupportsIndirectActionItem) {
  auto target_action = actions::ActionItem::Builder()
                           .SetText(u"Target Action")
                           .SetActionId(kActionNewTab)
                           .Build();

  actions::IndirectActionItem indirect_item(target_action.get());

  auto search_item = AppMenuSearchItem::Builder()
                         .SetType(AppMenuSearchItem::Type::kAction)
                         .SetAction(&indirect_item)
                         .SetTitle(u"Indirect Title")
                         .Build();

  EXPECT_EQ(search_item->GetTitle(), u"Indirect Title");
  EXPECT_EQ(search_item->GetActionItem(), target_action.get());
}

TEST(AppMenuSearchItemTest, WeakPtrExpired) {
  auto action_item = actions::ActionItem::Builder()
                         .SetText(u"Temporary Action")
                         .AddSynonyms({u"temp"})
                         .Build();

  auto search_item = AppMenuSearchItem::Builder()
                         .SetType(AppMenuSearchItem::Type::kAction)
                         .SetAction(action_item.get())
                         .SetTitle(u"Temporary Action")
                         .SetSecondaryText(u"Context")
                         .Build();
  EXPECT_EQ(search_item->GetActionItem(), action_item.get());
  EXPECT_FALSE(search_item->GetSynonyms().empty());

  action_item.reset();

  // action_item() becomes nullptr, but title and secondary text remain cached.
  EXPECT_EQ(search_item->GetActionItem(), nullptr);
  EXPECT_EQ(search_item->GetTitle(), u"Temporary Action");
  EXPECT_EQ(search_item->GetSecondaryText(), u"Context");
  EXPECT_TRUE(search_item->GetSynonyms().empty());
}

TEST(AppMenuSearchItemTest, BuilderCrashesWhenMissingAttributes) {
  auto action_item = actions::ActionItem::Builder()
                         .SetText(u"Test Action")
                         .SetActionId(kActionNewTab)
                         .Build();

  // Missing type crashes.
  EXPECT_DEATH_IF_SUPPORTED(std::ignore = AppMenuSearchItem::Builder()
                                              .SetAction(action_item.get())
                                              .SetTitle(u"Title")
                                              .Build(),
                            "");

  // Missing action crashes.
  EXPECT_DEATH_IF_SUPPORTED(
      std::ignore = AppMenuSearchItem::Builder()
                        .SetType(AppMenuSearchItem::Type::kAction)
                        .SetTitle(u"Title")
                        .Build(),
      "");

  // Missing title crashes.
  EXPECT_DEATH_IF_SUPPORTED(
      std::ignore = AppMenuSearchItem::Builder()
                        .SetType(AppMenuSearchItem::Type::kAction)
                        .SetAction(action_item.get())
                        .Build(),
      "");
}

TEST(AppMenuSearchItemTest, IntegratesWithFuzzyFinder) {
  auto action_item = actions::ActionItem::Builder()
                         .SetText(u"Open New Window")
                         .SetActionId(kActionNewWindow)
                         .AddSynonyms({u"incognito", u"create session"})
                         .Build();

  auto search_item = AppMenuSearchItem::Builder()
                         .SetType(AppMenuSearchItem::Type::kAction)
                         .SetAction(action_item.get())
                         .SetTitle(u"Open New Window")
                         .SetSecondaryText(u"File")
                         .Build();

  std::vector<FuzzySearchItem*> searchable_items = {search_item.get()};
  FuzzyFinder finder(std::move(searchable_items));

  auto title_results = finder.Find(u"Window", /*max_results=*/5);
  ASSERT_EQ(title_results.size(), 1u);
  EXPECT_EQ(title_results[0].item, search_item.get());

  auto secondary_results = finder.FuzzyFind(u"File", /*max_results=*/5);
  ASSERT_EQ(secondary_results.size(), 1u);
  EXPECT_EQ(secondary_results[0].item, search_item.get());

  auto synonym_results = finder.FuzzyFind(u"incognito", /*max_results=*/5);
  ASSERT_EQ(synonym_results.size(), 1u);
  EXPECT_EQ(synonym_results[0].item, search_item.get());

  auto no_results = finder.Find(u"NonExistentQuery", /*max_results=*/5);
  EXPECT_TRUE(no_results.empty());
}

}  // namespace
