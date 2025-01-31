// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/saved_tab_groups/most_recent_update_store.h"

#include "chrome/browser/ui/user_education/browser_user_education_interface.h"

namespace tab_groups {

MostRecentUpdateStore::MostRecentUpdateStore(
    BrowserWindowInterface* browser_window)
    : browser_window_(browser_window) {}
MostRecentUpdateStore::~MostRecentUpdateStore() = default;

void MostRecentUpdateStore::SetLastUpdatedTab(
    LocalTabGroupID group_id,
    std::optional<LocalTabID> tab_id) {
  last_updated_tab_ = {group_id, tab_id};

  // TODO(crbug.com/370924453): Trigger IPH from here
}

void MostRecentUpdateStore::MaybeShowPromo(const base::Feature& feature) {
  if (auto* user_education_interface =
          browser_window_->GetUserEducationInterface()) {
    user_education_interface->MaybeShowFeaturePromo(feature);
  }
}

}  // namespace tab_groups
