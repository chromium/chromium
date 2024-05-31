// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/birch/birch_self_share_provider.h"

#include "ash/birch/birch_item.h"
#include "ash/birch/birch_model.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/shell.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/favicon/favicon_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/send_tab_to_self_sync_service_factory.h"
#include "components/favicon_base/favicon_types.h"
#include "components/prefs/pref_service.h"
#include "components/send_tab_to_self/send_tab_to_self_entry.h"
#include "components/send_tab_to_self/send_tab_to_self_model.h"

namespace ash {

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

  // Avoid the favicon service network call if we don't need to refresh.
  if (!refresh) {
    Shell::Get()->birch_model()->SetSelfShareItems(std::move(items_));
    return;
  }

  items_.clear();

  favicon::FaviconService* favicon_service =
      FaviconServiceFactory::GetForProfile(profile_,
                                           ServiceAccessType::EXPLICIT_ACCESS);
  if (!favicon_service) {
    return;
  }

  active_tasks_ = 0;
  for (std::string guid : new_guids) {
    const send_tab_to_self::SendTabToSelfEntry* entry =
        model->GetEntryByGUID(guid);
    if (entry && !entry->IsOpened()) {
      ++active_tasks_;
      const std::string entry_guid = entry->GetGUID();
      items_.emplace_back(
          base::UTF8ToUTF16(entry_guid), base::UTF8ToUTF16(entry->GetTitle()),
          entry->GetURL(), entry->GetSharedTime(),
          base::UTF8ToUTF16(entry->GetDeviceName()), GURL(),
          base::BindRepeating(&BirchSelfShareProvider::OnItemPressed,
                              weak_factory_.GetWeakPtr(), entry_guid));
      favicon_service->GetFaviconImageForPageURL(
          entry->GetURL(),
          base::BindOnce(&BirchSelfShareProvider::OnFavIconDataAvailable,
                         base::Unretained(this), entry_guid),
          &cancelable_task_tracker_);
    }
  }

  if (active_tasks_ == 0) {
    Shell::Get()->birch_model()->SetSelfShareItems(std::move(items_));
  }
}

void BirchSelfShareProvider::OnFavIconDataAvailable(
    const std::string& guid,
    const favicon_base::FaviconImageResult& image_result) {
  const std::u16string u16_guid = base::UTF8ToUTF16(guid);
  auto it = std::find_if(items_.begin(), items_.end(),
                         [&u16_guid](const BirchSelfShareItem& item) {
                           return item.guid() == u16_guid;
                         });

  if (it != items_.end() && !image_result.image.IsEmpty()) {
    // TODO(b/333412417): Investigate why empty image result for tabs shared
    // from a macbook.
    it->set_favicon_url(image_result.icon_url);
  }

  if (--(active_tasks_) == 0) {
    Shell::Get()->birch_model()->SetSelfShareItems(std::move(items_));
  }
}

void BirchSelfShareProvider::OnItemPressed(const std::string& guid) {
  send_tab_to_self::SendTabToSelfModel* model =
      sync_service_->GetSendTabToSelfModel();
  model->MarkEntryOpened(guid);
}

}  // namespace ash
