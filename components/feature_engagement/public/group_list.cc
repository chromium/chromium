// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feature_engagement/public/group_list.h"

#include "build/build_config.h"
#include "components/feature_engagement/public/group_constants.h"

namespace feature_engagement {

namespace {
const base::Feature* const kAllGroups[] = {
    &kIPHDummyGroup,
};
}  // namespace

std::vector<const base::Feature*> GetAllGroups() {
  return std::vector<const base::Feature*>(kAllGroups,
                                           kAllGroups + std::size(kAllGroups));
}

}  // namespace feature_engagement
