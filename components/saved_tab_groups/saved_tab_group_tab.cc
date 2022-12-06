// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/saved_tab_groups/saved_tab_group_tab.h"

#include "base/strings/utf_string_conversions.h"
#include "components/saved_tab_groups/saved_tab_group.h"

SavedTabGroupTab::SavedTabGroupTab(
    const GURL& url,
    const std::u16string& title,
    const base::GUID& group_guid,
    SavedTabGroup* group,
    absl::optional<base::GUID> saved_tab_guid,
    absl::optional<base::Token> local_tab_id,
    absl::optional<base::Time> creation_time_windows_epoch_micros,
    absl::optional<base::Time> update_time_windows_epoch_micros,
    absl::optional<gfx::Image> favicon)
    : saved_tab_guid_(saved_tab_guid.has_value()
                          ? saved_tab_guid.value()
                          : base::GUID::GenerateRandomV4()),
      saved_group_guid_(group_guid),
      local_tab_id_(local_tab_id),
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
    const sync_pb::SavedTabGroupSpecifics& sync_specific) {
  bool sync_update_is_latest =
      sync_specific.update_time_windows_epoch_micros() >=
      update_time_windows_epoch_micros()
          .ToDeltaSinceWindowsEpoch()
          .InMicroseconds();
  return sync_update_is_latest;
}

std::unique_ptr<sync_pb::SavedTabGroupSpecifics> SavedTabGroupTab::MergeTab(
    const sync_pb::SavedTabGroupSpecifics& sync_specific) {
  if (ShouldMergeTab(sync_specific)) {
    SetURL(GURL(sync_specific.tab().url()));
    SetTitle(base::UTF8ToUTF16(sync_specific.tab().title()));
    SetUpdateTimeWindowsEpochMicros(base::Time::FromDeltaSinceWindowsEpoch(
        base::Microseconds(sync_specific.update_time_windows_epoch_micros())));
  }

  return ToSpecifics();
}

// static
SavedTabGroupTab SavedTabGroupTab::FromSpecifics(
    const sync_pb::SavedTabGroupSpecifics& specific) {
  const base::GUID& group_guid =
      base::GUID::ParseLowercase(specific.tab().group_guid());
  const GURL& url = GURL(specific.tab().url());
  const std::u16string title = base::UTF8ToUTF16(specific.tab().title());

  base::GUID guid = base::GUID::ParseLowercase(specific.guid());
  base::Time creation_time = base::Time::FromDeltaSinceWindowsEpoch(
      base::Microseconds(specific.creation_time_windows_epoch_micros()));
  base::Time update_time = base::Time::FromDeltaSinceWindowsEpoch(
      base::Microseconds(specific.update_time_windows_epoch_micros()));

  return SavedTabGroupTab(url, title, group_guid, nullptr, guid, absl::nullopt,
                          creation_time, update_time);
}

std::unique_ptr<sync_pb::SavedTabGroupSpecifics> SavedTabGroupTab::ToSpecifics()
    const {
  std::unique_ptr<sync_pb::SavedTabGroupSpecifics> pb_specific =
      std::make_unique<sync_pb::SavedTabGroupSpecifics>();
  pb_specific->set_guid(saved_tab_guid().AsLowercaseString());
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
  pb_tab->set_group_guid(saved_group_guid().AsLowercaseString());
  pb_tab->set_title(base::UTF16ToUTF8(title()));

  return pb_specific;
}
