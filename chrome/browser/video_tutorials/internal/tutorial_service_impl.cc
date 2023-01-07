// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/video_tutorials/internal/tutorial_service_impl.h"

#include <utility>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/string_util.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/video_tutorials/internal/config.h"
#include "chrome/browser/video_tutorials/internal/proto_conversions.h"
#include "chrome/browser/video_tutorials/prefs.h"
#include "chrome/browser/video_tutorials/switches.h"
#include "components/language/core/browser/pref_names.h"

namespace video_tutorials {

TutorialServiceImpl::TutorialServiceImpl(
    std::unique_ptr<TutorialManager> tutorial_manager,
    std::unique_ptr<TutorialFetcher> tutorial_fetcher,
    PrefService* pref_service)
    : tutorial_manager_(std::move(tutorial_manager)),
      tutorial_fetcher_(std::move(tutorial_fetcher)),
      pref_service_(pref_service) {
  tutorial_manager_->Initialize(
      base::BindOnce(&TutorialServiceImpl::OnManagerInitialized,
                     weak_ptr_factory_.GetWeakPtr()));
  pref_change_registrar_.Init(pref_service);
  pref_change_registrar_.Add(
      language::prefs::kAcceptLanguages,
      base::BindRepeating(&TutorialServiceImpl::OnAcceptLanguagesChanged,
                          weak_ptr_factory_.GetWeakPtr()));
}

TutorialServiceImpl::~TutorialServiceImpl() = default;

void TutorialServiceImpl::GetTutorials(MultipleItemCallback callback) {
  if (!tutorial_manager_->IsInitialized()) {
    MaybeCacheApiCall(base::BindOnce(&TutorialServiceImpl::GetTutorials,
                                     weak_ptr_factory_.GetWeakPtr(),
                                     std::move(callback)));
    return;
  }

  tutorial_manager_->GetTutorials(std::move(callback));
}

void TutorialServiceImpl::GetTutorial(FeatureType feature_type,
                                      SingleItemCallback callback) {
  if (!tutorial_manager_->IsInitialized()) {
    MaybeCacheApiCall(base::BindOnce(&TutorialServiceImpl::GetTutorial,
                                     weak_ptr_factory_.GetWeakPtr(),
                                     feature_type, std::move(callback)));
    return;
  }

  tutorial_manager_->GetTutorial(feature_type, std::move(callback));
}

void TutorialServiceImpl::StartFetchIfNecessary() {
  base::Time last_update_time = pref_service_->GetTime(kLastUpdatedTimeKey);
  bool ttl_expired =
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kVideoTutorialsInstantFetch) ||
      ((base::Time::Now() - last_update_time) > Config::GetFetchFrequency());
  std::string accept_languages =
      pref_service_->GetString(language::prefs::kAcceptLanguages);
  std::string text_locale = tutorial_manager_->GetTextLocale();
  bool locale_match = base::StartsWith(accept_languages, text_locale,
                                       base::CompareCase::INSENSITIVE_ASCII);
  bool needs_update = ttl_expired || !locale_match || text_locale.empty();
  if (needs_update) {
    tutorial_fetcher_->StartFetchForTutorials(base::BindOnce(
        &TutorialServiceImpl::OnFetchFinished, weak_ptr_factory_.GetWeakPtr()));
  }
}

void TutorialServiceImpl::OnManagerInitialized(bool success) {
  StartFetchIfNecessary();
  FlushCachedApiCalls();
}

void TutorialServiceImpl::OnFetchFinished(
    bool success,
    std::unique_ptr<std::string> response_body) {
  pref_service_->SetTime(kLastUpdatedTimeKey, base::Time::Now());

  if (!success || !response_body)
    return;

  proto::VideoTutorialGroups response_proto;
  bool parse_success = response_proto.ParseFromString(*response_body.get());
  if (!parse_success)
    return;

  tutorial_manager_->SaveGroups(
      std::make_unique<proto::VideoTutorialGroups>(std::move(response_proto)));
}

void TutorialServiceImpl::OnAcceptLanguagesChanged() {
  if (!tutorial_manager_->IsInitialized())
    return;

  std::string accept_languages =
      pref_service_->GetString(language::prefs::kAcceptLanguages);
  tutorial_fetcher_->OnAcceptLanguagesChanged(accept_languages);
  StartFetchIfNecessary();
}

void TutorialServiceImpl::MaybeCacheApiCall(base::OnceClosure api_call) {
  DCHECK(!tutorial_manager_->IsInitialized())
      << "Only cache API calls before initialization.";
  cached_api_calls_.emplace_back(std::move(api_call));
}

void TutorialServiceImpl::FlushCachedApiCalls() {
  // Flush all cached calls in FIFO sequence.
  while (!cached_api_calls_.empty()) {
    auto api_call = std::move(cached_api_calls_.front());
    cached_api_calls_.pop_front();
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(api_call));
  }
}

std::vector<std::string> TutorialServiceImpl::GetSupportedLanguages() {
  return tutorial_manager_->GetSupportedLanguages();
}

const std::vector<std::string>&
TutorialServiceImpl::GetAvailableLanguagesForTutorial(
    FeatureType feature_type) {
  return tutorial_manager_->GetAvailableLanguagesForTutorial(feature_type);
}

absl::optional<std::string> TutorialServiceImpl::GetPreferredLocale() {
  return tutorial_manager_->GetPreferredLocale();
}

void TutorialServiceImpl::SetPreferredLocale(const std::string& locale) {
  tutorial_manager_->SetPreferredLocale(locale);
}

}  // namespace video_tutorials
