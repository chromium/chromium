// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/birch/birch_self_share_provider.h"

#include "ash/birch/birch_item.h"
#include "ash/birch/birch_model.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/shell.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/send_tab_to_self_sync_service_factory.h"
#include "components/prefs/pref_service.h"
#include "components/send_tab_to_self/send_tab_to_self_entry.h"
#include "components/send_tab_to_self/send_tab_to_self_model.h"
#include "components/send_tab_to_self/target_device_info.h"

namespace ash {

namespace {

// The time before a `SendTabToSelfEntry` should be excluded from the
// `BirchModel`. This is the same expiration time for a device in
// `GetTargetDeviceInfoSortedList` for `SendTabToSelfBridge`.
constexpr base::TimeDelta kEntryExpiration = base::Days(10);

bool IsEntryExpired(base::Time shared_time) {
  return base::Time::Now() - shared_time > kEntryExpiration;
}

}  // namespace

BirchSelfShareProvider::BirchSelfShareProvider(Profile* profile)
    : profile_(profile),
      sync_service_(SendTabToSelfSyncServiceFactory::GetForProfile(profile)) {}

BirchSelfShareProvider::~BirchSelfShareProvider() = default;

void BirchSelfShareProvider::RequestBirchDataFetch() {
  const auto* const pref_service = profile_->GetPrefs();
  if (!pref_service ||
      !base::Contains(pref_service->GetList(
                          prefs::kContextualGoogleIntegrationsConfiguration),
                      prefs::kChromeSyncIntegrationName)) {
    // ChromeSync integration is disabled by policy.
    Shell::Get()->birch_model()->SetSelfShareItems({});
    return;
  }

  bool refresh = false;

  send_tab_to_self::SendTabToSelfModel* model =
      sync_service_->GetSendTabToSelfModel();

  std::set<std::string> cached_guids;
  for (const auto& item : items_) {
    cached_guids.insert(base::UTF16ToUTF8(item.guid()));
  }

  const std::vector<std::string> new_guids = model->GetAllGuids();

  // If there are any differences between cached guids and new guids, our
  // current list is dirty and we need to refresh.
  if (cached_guids.size() != new_guids.size() ||
      !std::equal(cached_guids.begin(), cached_guids.end(),
                  new_guids.begin())) {
    refresh = true;
  } else {
    // If there are no differences in guids, we check if any entries are opened.
    // Since we cannot determine if any cached entries are opened, we
    // have to iterate through the model entries.
    for (const std::string& guid : new_guids) {
      const send_tab_to_self::SendTabToSelfEntry* entry =
          model->GetEntryByGUID(guid);
      if (entry && entry->IsOpened()) {
        refresh = true;
      }
    }
  }

  if (!refresh) {
    Shell::Get()->birch_model()->SetSelfShareItems(std::move(items_));
    return;
  }

  items_.clear();

  for (std::string guid : new_guids) {
    const send_tab_to_self::SendTabToSelfEntry* entry =
        model->GetEntryByGUID(guid);
    if (entry && !entry->IsOpened() &&
        !IsEntryExpired(entry->GetSharedTime())) {
      const std::string entry_guid = entry->GetGUID();
      const std::string device_cache_guid =
          entry->GetTargetDeviceSyncCacheGuid();
      std::vector<send_tab_to_self::TargetDeviceInfo> device_info_list =
          model->GetTargetDeviceInfoSortedList();
      // Find the origin device that the entry was shared from using its
      // `target_device_sync_cache_guid_`.
      auto it = std::find_if(
          device_info_list.begin(), device_info_list.end(),
          [&device_cache_guid](
              const send_tab_to_self::TargetDeviceInfo& device_info) {
            return device_info.cache_guid == device_cache_guid;
          });

      // We set the `secondary_icon_type` of the birch item based on the origin
      // device's form factor.
      SecondaryIconType secondary_icon_type = SecondaryIconType::kNoIcon;
      if (it != device_info_list.end()) {
        send_tab_to_self::TargetDeviceInfo* matched_device_info = &(*it);
        switch (matched_device_info->form_factor) {
          case syncer::DeviceInfo::FormFactor::kDesktop:
            secondary_icon_type = SecondaryIconType::kTabFromDesktop;
            break;
          case syncer::DeviceInfo::FormFactor::kPhone:
            secondary_icon_type = SecondaryIconType::kTabFromPhone;
            break;
          case syncer::DeviceInfo::FormFactor::kTablet:
            secondary_icon_type = SecondaryIconType::kTabFromTablet;
            break;
          default:
            secondary_icon_type = SecondaryIconType::kNoIcon;
        }
      }
      items_.emplace_back(
          base::UTF8ToUTF16(entry_guid), base::UTF8ToUTF16(entry->GetTitle()),
          entry->GetURL(), entry->GetSharedTime(),
          base::UTF8ToUTF16(entry->GetDeviceName()), secondary_icon_type,
          base::BindRepeating(&BirchSelfShareProvider::OnItemPressed,
                              weak_factory_.GetWeakPtr(), entry_guid));
    }
  }
  Shell::Get()->birch_model()->SetSelfShareItems(std::move(items_));
}

void BirchSelfShareProvider::OnItemPressed(const std::string& guid) {
  send_tab_to_self::SendTabToSelfModel* model =
      sync_service_->GetSendTabToSelfModel();
  model->MarkEntryOpened(guid);
}

}  // namespace ash
