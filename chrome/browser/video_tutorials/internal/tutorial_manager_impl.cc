// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/video_tutorials/internal/tutorial_manager_impl.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/prefs/pref_service.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace video_tutorials {
namespace {

void FilterOutSummaryCard(std::vector<Tutorial>& tutorials) {
  for (auto it = tutorials.begin(); it != tutorials.end();) {
    if (it->feature == FeatureType::kSummary) {
      it = tutorials.erase(it);
    } else {
      ++it;
    }
  }
}

bool FindTutorialGroupInLanguage(const proto::VideoTutorialGroups& groups,
                                 const std::string& language,
                                 proto::VideoTutorialGroup& out_group) {
  for (const proto::VideoTutorialGroup& group : groups.tutorial_groups()) {
    if (group.language() == language) {
      out_group = group;
      return true;
    }
  }
  return false;
}

void FindTutorialGroupInPreferredLanguageOrDefault(
    const proto::VideoTutorialGroups& groups,
    const std::string& language,
    proto::VideoTutorialGroup& out_group) {
  bool found = FindTutorialGroupInLanguage(groups, language, out_group);
  if (!found && groups.tutorial_groups_size() > 0)
    out_group = groups.tutorial_groups(0);
}

}  // namespace

TutorialManagerImpl::TutorialManagerImpl(std::unique_ptr<TutorialStore> store,
                                         PrefService* prefs)
    : store_(std::move(store)) {}

TutorialManagerImpl::~TutorialManagerImpl() = default;

void TutorialManagerImpl::Initialize(SuccessCallback callback) {
  DCHECK(!initialized_);

  // The data doesn't exist in the cache. Load it from the DB.
  store_->InitAndLoad(base::BindOnce(&TutorialManagerImpl::OnDataLoaded,
                                     weak_ptr_factory_.GetWeakPtr(),
                                     std::move(callback)));
}

bool TutorialManagerImpl::IsInitialized() {
  return initialized_;
}

void TutorialManagerImpl::GetTutorials(MultipleItemCallback callback) {
  DCHECK(initialized_);
  if (!tutorial_groups_) {
    std::move(callback).Run(std::vector<Tutorial>());
    return;
  }

  auto preferred_locale = tutorial_groups_->preferred_locale();
  proto::VideoTutorialGroup active_group;
  FindTutorialGroupInPreferredLanguageOrDefault(*tutorial_groups_,
                                                preferred_locale, active_group);
  auto tutorials = TutorialsFromProto(&active_group);

  FilterOutSummaryCard(tutorials);
  std::move(callback).Run(tutorials);
}

void TutorialManagerImpl::GetTutorial(FeatureType feature_type,
                                      SingleItemCallback callback) {
  DCHECK(initialized_);
  if (!tutorial_groups_) {
    std::move(callback).Run(absl::nullopt);
    return;
  }

  auto preferred_locale = tutorial_groups_->preferred_locale();
  proto::VideoTutorialGroup active_group;
  FindTutorialGroupInPreferredLanguageOrDefault(*tutorial_groups_,
                                                preferred_locale, active_group);
  auto tutorials = TutorialsFromProto(&active_group);
  for (const Tutorial& tutorial : tutorials) {
    if (tutorial.feature == feature_type) {
      std::move(callback).Run(tutorial);
      return;
    }
  }

  std::move(callback).Run(absl::nullopt);
}

std::vector<std::string> TutorialManagerImpl::GetSupportedLanguages() {
  DCHECK(initialized_);

  std::vector<std::string> supported_languages;
  if (!tutorial_groups_)
    return supported_languages;

  for (const proto::VideoTutorialGroup& group :
       tutorial_groups_->tutorial_groups()) {
    supported_languages.emplace_back(group.language());
  }

  return supported_languages;
}

const std::vector<std::string>&
TutorialManagerImpl::GetAvailableLanguagesForTutorial(
    FeatureType feature_type) {
  DCHECK(initialized_);
  return languages_for_tutorials_[feature_type];
}

absl::optional<std::string> TutorialManagerImpl::GetPreferredLocale() {
  DCHECK(initialized_);
  std::string preferred_locale;
  if (tutorial_groups_)
    preferred_locale = tutorial_groups_->preferred_locale();
  return preferred_locale.empty() ? absl::nullopt
                                  : absl::make_optional(preferred_locale);
}

void TutorialManagerImpl::SetPreferredLocale(const std::string& locale) {
  DCHECK(initialized_);
  if (!tutorial_groups_)
    return;

  tutorial_groups_->set_preferred_locale(locale);
  store_->Update(*tutorial_groups_,
                 base::BindOnce(&TutorialManagerImpl::OnGroupsSaved,
                                weak_ptr_factory_.GetWeakPtr()));
}

std::string TutorialManagerImpl::GetTextLocale() {
  DCHECK(initialized_);
  return tutorial_groups_ ? tutorial_groups_->text_locale() : std::string();
}

void TutorialManagerImpl::OnDataLoaded(
    SuccessCallback callback,
    bool success,
    std::unique_ptr<proto::VideoTutorialGroups> loaded_data) {
  tutorial_groups_ = std::move(loaded_data);

  if (tutorial_groups_) {
    languages_for_tutorials_.clear();
    for (const auto& group : tutorial_groups_->tutorial_groups()) {
      for (const auto& tutorial : group.tutorials()) {
        auto feature = ToFeatureType(tutorial.feature());
        languages_for_tutorials_[feature].emplace_back(group.language());
      }
    }
  }

  initialized_ = true;
  std::move(callback).Run(success);
}

void TutorialManagerImpl::SaveGroups(
    std::unique_ptr<proto::VideoTutorialGroups> groups) {
  DCHECK(initialized_);
  if (!groups) {
    OnGroupsSaved(true);
    return;
  }

  if (tutorial_groups_)
    groups->set_preferred_locale(tutorial_groups_->preferred_locale());

  store_->Update(*groups, base::BindOnce(&TutorialManagerImpl::OnGroupsSaved,
                                         weak_ptr_factory_.GetWeakPtr()));
}

void TutorialManagerImpl::OnGroupsSaved(bool success) {}

}  // namespace video_tutorials
