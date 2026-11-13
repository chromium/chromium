// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/public/articles_list_visible_sync_service.h"

#include "base/auto_reset.h"
#include "base/functional/bind.h"
#include "base/notreached.h"
#include "components/feed/core/shared_prefs/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/sync_preferences/pref_service_syncable.h"

namespace feed {

namespace {

const char* GetSyncedPrefName(
    ArticlesListVisibleSyncService::FormFactor form_factor) {
  switch (form_factor) {
    case ArticlesListVisibleSyncService::FormFactor::kPhone:
      return prefs::kArticlesListVisibleOnPhone;
    case ArticlesListVisibleSyncService::FormFactor::kTablet:
      return prefs::kArticlesListVisibleOnTablet;
  }
  NOTREACHED();
}

}  // namespace

// static
std::optional<ArticlesListVisibleSyncService::FormFactor>
ArticlesListVisibleSyncService::GetFormFactor(
    ui::DeviceFormFactor device_form_factor,
    bool is_foldable) {
  switch (device_form_factor) {
    case ui::DEVICE_FORM_FACTOR_PHONE:
    case ui::DEVICE_FORM_FACTOR_FOLDABLE:
      return FormFactor::kPhone;
    case ui::DEVICE_FORM_FACTOR_TABLET:
      // A foldable is reported as a tablet when it's unfolded, but it always
      // syncs with phones so that folding it doesn't change the feed. Legacy
      // foldables that `is_foldable` can't detect sync with whichever form
      // factor they're reported as at startup, which can change between
      // launches.
      return is_foldable ? FormFactor::kPhone : FormFactor::kTablet;
    case ui::DEVICE_FORM_FACTOR_DESKTOP:
    case ui::DEVICE_FORM_FACTOR_TV:
    case ui::DEVICE_FORM_FACTOR_AUTOMOTIVE:
    case ui::DEVICE_FORM_FACTOR_XR:
      return std::nullopt;
  }
  NOTREACHED();
}

// static
void ArticlesListVisibleSyncService::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(
      prefs::kArticlesListVisibleOnPhone, /*default_value=*/true,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kArticlesListVisibleOnTablet, /*default_value=*/true,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
}

ArticlesListVisibleSyncService::ArticlesListVisibleSyncService(
    sync_preferences::PrefServiceSyncable& profile_prefs,
    FormFactor form_factor)
    : profile_prefs_(profile_prefs),
      synced_pref_name_(GetSyncedPrefName(form_factor)) {
  pref_change_registrar_.Init(&profile_prefs_.get());
  pref_change_registrar_.Add(
      prefs::kArticlesListVisible,
      base::BindRepeating(
          &ArticlesListVisibleSyncService::OnArticlesListVisibleChanged,
          weak_ptr_factory_.GetWeakPtr()));
  profile_prefs_->AddSyncedPrefObserver(synced_pref_name_, this);

  // Values downloaded by sync are applied in `OnStartedSyncing()`. Here, only
  // mirror a choice that the user made on this device while it wasn't
  // mirrored, e.g. before this service existed. It's uploaded if preferences
  // sync is enabled; otherwise it stays on this device. A matching value isn't
  // written, so that it doesn't override other devices' choices on sync merge.
  if (profile_prefs_->HasPrefPath(prefs::kArticlesListVisible) &&
      profile_prefs_->GetBoolean(prefs::kArticlesListVisible) !=
          profile_prefs_->GetBoolean(synced_pref_name_)) {
    OnArticlesListVisibleChanged();
  }
}

ArticlesListVisibleSyncService::~ArticlesListVisibleSyncService() = default;

void ArticlesListVisibleSyncService::Shutdown() {
  pref_change_registrar_.RemoveAll();
  profile_prefs_->RemoveSyncedPrefObserver(synced_pref_name_, this);
}

void ArticlesListVisibleSyncService::OnSyncedPrefChanged(std::string_view path,
                                                         bool from_sync) {
  // This is received after we changed the value of `synced_pref_name_` locally
  // in `OnArticlesListVisibleChanged()`, ignore.
  if (!from_sync) {
    return;
  }

  OnSyncedArticlesListVisibleChanged();
}

void ArticlesListVisibleSyncService::OnStartedSyncing(
    std::string_view path,
    const base::Value& sync_value) {
  OnSyncedArticlesListVisibleChanged();
}

void ArticlesListVisibleSyncService::OnArticlesListVisibleChanged() {
  // These are changes originating from us, ignore.
  if (is_applying_synced_value_) {
    return;
  }

  // Writing the synced pref locally notifies `OnSyncedPrefChanged()` with
  // `from_sync` false, which is ignored.
  profile_prefs_->SetBoolean(
      synced_pref_name_,
      profile_prefs_->GetBoolean(prefs::kArticlesListVisible));
}

void ArticlesListVisibleSyncService::OnSyncedArticlesListVisibleChanged() {
  // Set flag to stop accepting the change notification of
  // `prefs::kArticlesListVisible` triggered by the write below, so that the
  // value downloaded by sync isn't written back to the synced pref, which would
  // also store it outside of the account storage.
  base::AutoReset<bool> applying_synced_value(&is_applying_synced_value_, true);
  profile_prefs_->SetBoolean(prefs::kArticlesListVisible,
                             profile_prefs_->GetBoolean(synced_pref_name_));
}

}  // namespace feed
