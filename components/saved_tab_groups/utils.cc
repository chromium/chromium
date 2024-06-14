// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/saved_tab_groups/utils.h"

#include "build/build_config.h"
#include "components/saved_tab_groups/types.h"

namespace tab_groups {

bool AreLocalIdsPersisted() {
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  return true;
#else
  return false;
#endif
}

std::string LocalTabGroupIDToString(const LocalTabGroupID& local_tab_group_id) {
  return local_tab_group_id.ToString();
}

std::optional<LocalTabGroupID> LocalTabGroupIDFromString(
    const std::string& serialized_local_tab_group_id) {
#if BUILDFLAG(IS_ANDROID)
  return base::Token::FromString(serialized_local_tab_group_id);
#else
  auto token = base::Token::FromString(serialized_local_tab_group_id);
  if (!token.has_value()) {
    return std::nullopt;
  }

  return tab_groups::TabGroupId::FromRawToken(token.value());
#endif
}

}  // namespace tab_groups
