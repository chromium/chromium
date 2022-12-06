// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/saved_tab_groups/saved_tab_group.h"
#include "base/token.h"
#include "components/saved_tab_groups/saved_tab_group_tab.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace {
base::GUID MakeUniqueGUID() {
  static uint64_t unique_value = 0;
  unique_value++;
  uint64_t kBytes[] = {0, unique_value};
  return base::GUID::FormatRandomDataAsV4ForTesting(
      as_bytes(base::make_span(kBytes)));
}

base::Token MakeUniqueToken() {
  static uint64_t unique_value = 0;
  unique_value++;
  return base::Token(0, unique_value);
}

SavedTabGroup CreateDefaultEmptySavedTabGroup() {
  return SavedTabGroup(std::u16string(u"default_group"),
                       tab_groups::TabGroupColorId::kGrey, {});
}

void AddTabToEndOfGroup(
    SavedTabGroup& group,
    absl::optional<base::GUID> saved_guid = absl::nullopt,
    absl::optional<base::Token> local_tab_id = absl::nullopt) {
  group.AddTab(group.saved_tabs().size(),
               SavedTabGroupTab(
                   GURL(url::kAboutBlankURL), std::u16string(u"default_title"),
                   group.saved_guid(), &group, saved_guid, local_tab_id));
}
}  // namespace

TEST(SavedTabGroup, GetTabByGUID) {
  base::GUID tab_1_saved_guid = MakeUniqueGUID();
  base::GUID tab_2_saved_guid = MakeUniqueGUID();

  // create a group with a couple tabs
  SavedTabGroup group = CreateDefaultEmptySavedTabGroup();
  AddTabToEndOfGroup(group, tab_1_saved_guid);
  AddTabToEndOfGroup(group, tab_2_saved_guid);
  ASSERT_EQ(2u, group.saved_tabs().size());

  SavedTabGroupTab* tab_1 = group.GetTab(tab_1_saved_guid);
  EXPECT_EQ(&group.saved_tabs()[0], tab_1);

  SavedTabGroupTab* tab_2 = group.GetTab(tab_2_saved_guid);
  EXPECT_EQ(&group.saved_tabs()[1], tab_2);
}

TEST(SavedTabGroup, GetTabByToken) {
  base::Token tab_1_local_id = MakeUniqueToken();
  base::Token tab_2_local_id = MakeUniqueToken();

  // create a group with a couple tabs
  SavedTabGroup group = CreateDefaultEmptySavedTabGroup();
  AddTabToEndOfGroup(group, absl::nullopt, tab_1_local_id);
  AddTabToEndOfGroup(group, absl::nullopt, tab_2_local_id);
  ASSERT_EQ(2u, group.saved_tabs().size());

  SavedTabGroupTab* tab_1 = group.GetTab(tab_1_local_id);
  EXPECT_EQ(&group.saved_tabs()[0], tab_1);

  SavedTabGroupTab* tab_2 = group.GetTab(tab_2_local_id);
  EXPECT_EQ(&group.saved_tabs()[1], tab_2);
}
