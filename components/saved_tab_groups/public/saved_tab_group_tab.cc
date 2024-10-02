// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/saved_tab_groups/public/saved_tab_group_tab.h"

#include "base/strings/utf_string_conversions.h"
#include "components/saved_tab_groups/public/features.h"
#include "components/saved_tab_groups/public/saved_tab_group.h"

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
    std::optional<base::Time> creation_time_windows_epoch_micros,
    std::optional<base::Time> update_time_windows_epoch_micros,
    std::optional<gfx::Image> favicon)
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
      creation_time_windows_epoch_micros_(
          creation_time_windows_epoch_micros.has_value()
              ? creation_time_windows_epoch_micros.value()
              : base::Time::Now()),
      update_time_windows_epoch_micros_(
          update_time_windows_epoch_micros.has_value()
              ? update_time_windows_epoch_micros.value()
              : base::Time::Now()),
      favicon_(favicon) {}

SavedTabGroupTab::SavedTabGroupTab(const SavedTabGroupTab& other) = default;
SavedTabGroupTab& SavedTabGroupTab::operator=(const SavedTabGroupTab& other) =
    default;
SavedTabGroupTab::SavedTabGroupTab(SavedTabGroupTab&& other) = default;
SavedTabGroupTab& SavedTabGroupTab::operator=(SavedTabGroupTab&& other) =
    default;
SavedTabGroupTab::~SavedTabGroupTab() = default;

bool SavedTabGroupTab::ShouldMergeTab(
    const SavedTabGroupTab& remote_tab) const {
  if (AlwaysAcceptServerDataInModel()) {
    return true;
  }

  return remote_tab.update_time_windows_epoch_micros() >=
         update_time_windows_epoch_micros();
}

void SavedTabGroupTab::MergeRemoteTab(const SavedTabGroupTab& remote_tab) {
  if (!ShouldMergeTab(remote_tab)) {
    return;
  }

  SetURL(remote_tab.url());
  SetTitle(remote_tab.title());
  // TODO(crbug.com/370714643): check that remote tab always contains position.
  SetPosition(remote_tab.position().value_or(0));
  SetCreatorCacheGuid(remote_tab.creator_cache_guid());
  SetLastUpdaterCacheGuid(remote_tab.last_updater_cache_guid());
  SetUpdateTimeWindowsEpochMicros(
      remote_tab.update_time_windows_epoch_micros());
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

SavedTabGroupTabBuilder& SavedTabGroupTabBuilder::SetURL(const GURL& url) {
  url_ = url;
  has_url_ = true;
  return *this;
}

SavedTabGroupTabBuilder& SavedTabGroupTabBuilder::SetTitle(
    const std::u16string& title) {
  title_ = title;
  has_title_ = true;
  return *this;
}

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
  if (has_url_) {
    updated_tab.SetURL(url_);
  }
  if (has_title_) {
    updated_tab.SetTitle(title_);
  }
  if (has_position_) {
    updated_tab.SetPosition(position_);
  }
  if (has_redirect_url_chain_) {
    updated_tab.SetRedirectURLChain(redirect_url_chain_);
  }

  return updated_tab;
}

}  // namespace tab_groups
