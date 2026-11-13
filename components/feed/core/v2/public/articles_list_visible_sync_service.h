// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_V2_PUBLIC_ARTICLES_LIST_VISIBLE_SYNC_SERVICE_H_
#define COMPONENTS_FEED_CORE_V2_PUBLIC_ARTICLES_LIST_VISIBLE_SYNC_SERVICE_H_

#include <optional>
#include <string_view>

#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/sync_preferences/synced_pref_observer.h"
#include "ui/base/device_form_factor.h"

namespace base {
class Value;
}  // namespace base

namespace sync_preferences {
class PrefServiceSyncable;
}  // namespace sync_preferences

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

namespace feed {

// Syncs whether the articles list is visible between Android devices of the
// same form factor. Foldables sync with phones.
//
// `prefs::kArticlesListVisible` is not synced. This service mirrors it into the
// synced pref of the device's form factor, `prefs::kArticlesListVisibleOnPhone`
// or `prefs::kArticlesListVisibleOnTablet`, and mirrors values that sync
// downloads for that synced pref back into `prefs::kArticlesListVisible`.
// Values removed from the synced pref when sync stops, e.g. on sign-out, are
// not mirrored back, so the articles list keeps its visibility on this device.
class ArticlesListVisibleSyncService
    : public KeyedService,
      public sync_preferences::SyncedPrefObserver {
 public:
  enum class FormFactor {
    kPhone,
    kTablet,
  };

  // Returns the form factor whose devices sync the articles list visibility
  // with a device of `device_form_factor`, or std::nullopt if such a device
  // doesn't sync it.
  static std::optional<FormFactor> GetFormFactor(
      ui::DeviceFormFactor device_form_factor,
      bool is_foldable);

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // `profile_prefs` must outlive this service.
  ArticlesListVisibleSyncService(
      sync_preferences::PrefServiceSyncable& profile_prefs,
      FormFactor form_factor);
  ArticlesListVisibleSyncService(const ArticlesListVisibleSyncService&) =
      delete;
  ArticlesListVisibleSyncService& operator=(
      const ArticlesListVisibleSyncService&) = delete;
  ~ArticlesListVisibleSyncService() override;

  // KeyedService:
  void Shutdown() override;

  // sync_preferences::SyncedPrefObserver:
  void OnSyncedPrefChanged(std::string_view path, bool from_sync) override;
  void OnStartedSyncing(std::string_view path,
                        const base::Value& sync_value) override;

 private:
  void OnArticlesListVisibleChanged();
  void OnSyncedArticlesListVisibleChanged();

  const raw_ref<sync_preferences::PrefServiceSyncable> profile_prefs_;
  // The synced pref of this device's form factor.
  const char* const synced_pref_name_;

  PrefChangeRegistrar pref_change_registrar_;
  // True while a synced value is written to `prefs::kArticlesListVisible`.
  bool is_applying_synced_value_ = false;

  base::WeakPtrFactory<ArticlesListVisibleSyncService> weak_ptr_factory_{this};
};

}  // namespace feed

#endif  // COMPONENTS_FEED_CORE_V2_PUBLIC_ARTICLES_LIST_VISIBLE_SYNC_SERVICE_H_
