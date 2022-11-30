// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/saved_tab_groups/saved_tab_group_tab.h"

#include "components/saved_tab_groups/saved_tab_group.h"

SavedTabGroupTab::SavedTabGroupTab(
    const GURL& url,
    const base::GUID& group_guid,
    SavedTabGroup* group,
    absl::optional<base::GUID> guid,
    absl::optional<base::Time> creation_time_windows_epoch_micros,
    absl::optional<base::Time> update_time_windows_epoch_micros,
    absl::optional<std::u16string> title,
    absl::optional<gfx::Image> favicon)
    : guid_(guid.has_value() ? guid.value() : base::GUID::GenerateRandomV4()),
      group_guid_(group_guid),
      saved_tab_group_(group),
      url_(url),
      title_(title),
      favicon_(favicon),
      creation_time_windows_epoch_micros_(
          creation_time_windows_epoch_micros.has_value()
              ? creation_time_windows_epoch_micros.value()
              : base::Time::Now()),
      update_time_windows_epoch_micros_(
          update_time_windows_epoch_micros.has_value()
              ? update_time_windows_epoch_micros.value()
              : base::Time::Now()) {}
SavedTabGroupTab::SavedTabGroupTab(const SavedTabGroupTab& other) = default;
SavedTabGroupTab::~SavedTabGroupTab() = default;

bool SavedTabGroupTab::ShouldMergeTab(
    sync_pb::SavedTabGroupSpecifics* sync_specific) {
  bool sync_update_is_latest =
      sync_specific->update_time_windows_epoch_micros() >=
      update_time_windows_epoch_micros()
          .ToDeltaSinceWindowsEpoch()
          .InMicroseconds();
  return sync_update_is_latest;
}

std::unique_ptr<sync_pb::SavedTabGroupSpecifics> SavedTabGroupTab::MergeTab(
    std::unique_ptr<sync_pb::SavedTabGroupSpecifics> sync_specific) {
  if (ShouldMergeTab(sync_specific.get())) {
    SetURL(GURL(sync_specific->tab().url()));
    SetUpdateTimeWindowsEpochMicros(base::Time::FromDeltaSinceWindowsEpoch(
        base::Microseconds(sync_specific->update_time_windows_epoch_micros())));
  }

  return ToSpecifics();
}

// static
SavedTabGroupTab SavedTabGroupTab::FromSpecifics(
    const sync_pb::SavedTabGroupSpecifics& specific) {
  const base::GUID& group_guid =
      base::GUID::ParseLowercase(specific.tab().group_guid());
  const GURL& url = GURL(specific.tab().url());

  base::GUID guid = base::GUID::ParseLowercase(specific.guid());
  base::Time creation_time = base::Time::FromDeltaSinceWindowsEpoch(
      base::Microseconds(specific.creation_time_windows_epoch_micros()));
  base::Time update_time = base::Time::FromDeltaSinceWindowsEpoch(
      base::Microseconds(specific.update_time_windows_epoch_micros()));

  return SavedTabGroupTab(url, group_guid, nullptr, guid, creation_time,
                          update_time);
}

std::unique_ptr<sync_pb::SavedTabGroupSpecifics> SavedTabGroupTab::ToSpecifics()
    const {
  std::unique_ptr<sync_pb::SavedTabGroupSpecifics> pb_specific =
      std::make_unique<sync_pb::SavedTabGroupSpecifics>();
  pb_specific->set_guid(guid().AsLowercaseString());
  pb_specific->set_creation_time_windows_epoch_micros(
      creation_time_windows_epoch_micros()
          .ToDeltaSinceWindowsEpoch()
          .InMicroseconds());
  pb_specific->set_update_time_windows_epoch_micros(
      update_time_windows_epoch_micros()
          .ToDeltaSinceWindowsEpoch()
          .InMicroseconds());

  sync_pb::SavedTabGroupTab* pb_tab = pb_specific->mutable_tab();
  pb_tab->set_url(url().spec());
  pb_tab->set_group_guid(group_guid().AsLowercaseString());

  return pb_specific;
}
