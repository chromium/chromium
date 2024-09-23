// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/suggestion_group_util.h"

#include <optional>

#include "base/feature_list.h"
#include "base/lazy_instance.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/strings/grit/components_strings.h"
#include "third_party/omnibox_proto/groups.pb.h"
#include "ui/base/l10n/l10n_util.h"

namespace omnibox {
namespace {
GroupConfig CreateGroup(GroupSection section,
                        GroupConfig::RenderType render_type =
                            GroupConfig_RenderType_DEFAULT_VERTICAL,
                        std::optional<int32_t> header_text = {}) {
  GroupConfig group;
  group.set_section(section);
  group.set_render_type(render_type);
  if (header_text) {
    group.set_header_text(l10n_util::GetStringUTF8(*header_text));
  }
  return group;
}

base::LazyInstance<GroupConfigMap>::DestructorAtExit g_default_groups =
    LAZY_INSTANCE_INITIALIZER;
}  // namespace

const GroupConfigMap& BuildDefaultGroups() {
  if (g_default_groups.Get().empty()) {
    g_default_groups.Get() = {
        // clang-format off
        {GROUP_MOBILE_SEARCH_READY_OMNIBOX, CreateGroup(SECTION_MOBILE_VERBATIM)},
        {GROUP_MOBILE_CLIPBOARD,            CreateGroup(SECTION_MOBILE_CLIPBOARD)},
        {GROUP_PERSONALIZED_ZERO_SUGGEST,   CreateGroup(SECTION_PERSONALIZED_ZERO_SUGGEST)},
        {GROUP_MOBILE_MOST_VISITED,
         CreateGroup(SECTION_MOBILE_MOST_VISITED,
                     base::FeatureList::IsEnabled(
                         kMostVisitedTilesHorizontalRenderGroup)
                         ? GroupConfig_RenderType_HORIZONTAL
                         : GroupConfig_RenderType_DEFAULT_VERTICAL)},

        {GROUP_MOBILE_RICH_ANSWER,
            OmniboxFieldTrial::kAnswerActionsShowRichCard.Get()
          ? CreateGroup(SECTION_MOBILE_RICH_ANSWER)
          : CreateGroup(SECTION_SEARCH)},
        {GROUP_SEARCH, CreateGroup(SECTION_SEARCH)},
        {GROUP_OTHER_NAVS, CreateGroup(SECTION_SEARCH)},
        // clang-format on
    };
  }
  return g_default_groups.Get();
}

void ResetDefaultGroupsForTest() {
  g_default_groups.Get().clear();
}

GroupId GroupIdForNumber(int value) {
  return GroupId_IsValid(value) ? static_cast<GroupId>(value) : GROUP_INVALID;
}

}  // namespace omnibox
