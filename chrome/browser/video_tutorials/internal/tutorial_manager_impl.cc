// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/video_tutorials/internal/tutorial_manager_impl.h"

#include <set>

#include "base/bind.h"
#include "base/stl_util.h"
#include "chrome/browser/video_tutorials/internal/config.h"
#include "chrome/browser/video_tutorials/prefs.h"
#include "components/prefs/pref_service.h"

namespace video_tutorials {

TutorialManagerImpl::TutorialManagerImpl(std::unique_ptr<TutorialStore> store,
                                         PrefService* prefs)
    : store_(std::move(store)), prefs_(prefs) {}

TutorialManagerImpl::~TutorialManagerImpl() = default;

void TutorialManagerImpl::Init(SuccessCallback callback) {
  store_->InitAndLoadKeys(base::BindOnce(&TutorialManagerImpl::OnInitCompleted,
                                         weak_ptr_factory_.GetWeakPtr(),
                                         std::move(callback)));
}

void TutorialManagerImpl::GetTutorials(GetTutorialsCallback callback) {
  // Find the data from cache.
  std::string locale = GetPreferredLocale();
  if (tutorial_group_ && tutorial_group_->locale == locale) {
    std::move(callback).Run(tutorial_group_->tutorials);
    return;
  }

  // The data doesn't exist in the cache. Probably the locale has changed. Load
  // it from the DB.
  store_->LoadEntries(
      {locale},
      base::BindOnce(&TutorialManagerImpl::OnTutorialsLoaded,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

const std::vector<std::string>& TutorialManagerImpl::GetSupportedLocales() {
  return supported_locales_;
}

std::string TutorialManagerImpl::GetPreferredLocale() {
  return prefs_->HasPrefPath(kPreferredLocaleKey)
             ? prefs_->GetString(kPreferredLocaleKey)
             : Config::GetDefaultPreferredLocale();
}

void TutorialManagerImpl::SetPreferredLocale(const std::string& locale) {
  prefs_->SetString(kPreferredLocaleKey, locale);
}

void TutorialManagerImpl::OnInitCompleted(
    SuccessCallback callback,
    bool success,
    std::unique_ptr<std::vector<std::string>> supported_locales) {
  if (!success) {
    std::move(callback).Run(success);
    return;
  }

  if (supported_locales)
    supported_locales_ = *supported_locales.get();

  std::move(callback).Run(true);
}

void TutorialManagerImpl::OnTutorialsLoaded(
    GetTutorialsCallback callback,
    bool success,
    std::vector<std::unique_ptr<TutorialGroup>> loaded_group) {
  if (!success || loaded_group.empty()) {
    std::move(callback).Run(std::vector<Tutorial>());
    return;
  }

  // We are loading tutorials only for the preferred locale.
  DCHECK(loaded_group.size() == 1u);
  tutorial_group_ = std::move(loaded_group.front());
  std::move(callback).Run(tutorial_group_->tutorials);
}

void TutorialManagerImpl::SaveGroups(
    std::vector<std::unique_ptr<TutorialGroup>> groups,
    SuccessCallback callback) {
  std::vector<std::string> new_locales;
  std::vector<std::pair<std::string, TutorialGroup>> key_entry_pairs;
  for (auto& group : groups) {
    new_locales.emplace_back(group->locale);
    key_entry_pairs.emplace_back(std::make_pair(group->locale, *group));
  }

  // Remove the locales that don't exist in the new data.
  std::vector<std::string> keys_to_delete;
  for (auto& old_locale : supported_locales_) {
    if (std::find(new_locales.begin(), new_locales.end(), old_locale) ==
        new_locales.end()) {
      keys_to_delete.emplace_back(old_locale);
    }
  }

  store_->UpdateAll(key_entry_pairs, keys_to_delete, std::move(callback));
}

}  // namespace video_tutorials
