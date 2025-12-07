// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/saved_tab_groups/public/saved_tab_group_tab.h"

#include "base/strings/utf_string_conversions.h"
#include "components/saved_tab_groups/public/features.h"
#include "components/saved_tab_groups/public/saved_tab_group.h"
#include "components/saved_tab_groups/public/utils.h"

namespace tab_groups {

SavedTabGroupTab::SavedTabGroupTab(
    const GURL& url,
    const std::u16string& title,
    const base::Uuid& group_guid,
    std::optional<size_t> position,
    std::optional<base::Uuid> saved_tab_guid,
    std::optional<LocalTabID> local_tab_id,
    std::optional<std::string> creator_cache_guid,
    std::optional<std::string> last_updater_cache_guid,
    std::optional<base::Time> creation_time,
    std::optional<base::Time> update_time,
    std::optional<gfx::Image> favicon,
    bool is_pending_ntp)
    : saved_tab_guid_(saved_tab_guid.has_value()
                          ? saved_tab_guid.value()
                          : base::Uuid::GenerateRandomV4()),
      saved_group_guid_(group_guid),
      local_tab_id_(local_tab_id),
      position_(position),
      url_(url),
      title_(title),
      creator_cache_guid_(std::move(creator_cache_guid)),
      last_updater_cache_guid_(std::move(last_updater_cache_guid)),
      creation_time_(creation_time.has_value() ? creation_time.value()
                                               : base::Time::Now()),
      update_time_(update_time.has_value() ? update_time.value()
                                           : base::Time::Now()),
      favicon_(favicon),
      is_pending_ntp_(is_pending_ntp) {}

SavedTabGroupTab::SavedTabGroupTab(const SavedTabGroupTab& other) = default;
SavedTabGroupTab& SavedTabGroupTab::operator=(const SavedTabGroupTab& other) =
    default;
SavedTabGroupTab::SavedTabGroupTab(SavedTabGroupTab&& other) = default;
SavedTabGroupTab& SavedTabGroupTab::operator=(SavedTabGroupTab&& other) =
    default;
SavedTabGroupTab::~SavedTabGroupTab() = default;

SavedTabGroupTab& SavedTabGroupTab::SetUpdatedByAttribution(GaiaId updated_by) {
  if (shared_attribution_.created_by.empty()) {
    shared_attribution_.created_by = updated_by;
  }
  shared_attribution_.updated_by = std::move(updated_by);
  return *this;
}

SavedTabGroupTab& SavedTabGroupTab::SetCreatedByAttribution(GaiaId created_by) {
  CHECK(shared_attribution_.created_by.empty());
  shared_attribution_.created_by = std::move(created_by);
  return *this;
}

void SavedTabGroupTab::MergeRemoteTab(const SavedTabGroupTab& remote_tab) {
  // If a remote tab's URL is not supported, don't change the URL and title
  // for the existing tab as it will allow user to continue navigating the
  // current page. Only keep other information such as position, cache guid
  // and attribution up-to-date from the remote tab.
  if (IsURLValidForSavedTabGroups(remote_tab.url())) {
    SetURL(remote_tab.url());
    SetTitle(remote_tab.title());
  }
  // TODO(crbug.com/370714643): check that remote tab always contains position.
  SetPosition(remote_tab.position().value_or(0));
  SetCreatorCacheGuid(remote_tab.creator_cache_guid());
  SetLastUpdaterCacheGuid(remote_tab.last_updater_cache_guid());
  SetUpdatedByAttribution(remote_tab.shared_attribution().updated_by);
  SetUpdateTime(remote_tab.update_time());
  SetNavigationTime(remote_tab.navigation_time());
}

bool SavedTabGroupTab::IsSyncEquivalent(const SavedTabGroupTab& other) const {
  return saved_tab_guid() == other.saved_tab_guid() && url() == other.url() &&
         saved_group_guid() == other.saved_group_guid() &&
         title() == other.title() && position() == other.position();
}

bool SavedTabGroupTab::IsURLInRedirectChain(const GURL& url) const {
  for (const auto& redirect_url : redirect_url_chain_) {
    if (redirect_url.GetWithoutRef().spec() == url.GetWithoutRef().spec()) {
      return true;
    }
  }
  return false;
}

SavedTabGroupTabBuilder::SavedTabGroupTabBuilder() = default;

SavedTabGroupTabBuilder::~SavedTabGroupTabBuilder() = default;

SavedTabGroupTabBuilder::SavedTabGroupTabBuilder(
    const SavedTabGroupTabBuilder&) = default;

SavedTabGroupTabBuilder& SavedTabGroupTabBuilder::operator=(
    const SavedTabGroupTabBuilder&) = default;

SavedTabGroupTabBuilder& SavedTabGroupTabBuilder::SetPosition(size_t position) {
  position_ = position;
  has_position_ = true;
  return *this;
}

SavedTabGroupTabBuilder& SavedTabGroupTabBuilder::SetRedirectURLChain(
    const std::vector<GURL>& redirect_url_chain) {
  redirect_url_chain_ = redirect_url_chain;
  has_redirect_url_chain_ = true;
  return *this;
}


SavedTabGroupTab SavedTabGroupTabBuilder::Build(
    const SavedTabGroupTab& tab) const {
  SavedTabGroupTab updated_tab(tab);

  // Apply the updates from the builder.
  if (has_position_) {
    updated_tab.SetPosition(position_);
  }
  if (has_redirect_url_chain_) {
    updated_tab.SetRedirectURLChain(redirect_url_chain_);
  }

  return updated_tab;
}

}  // namespace tab_groups
