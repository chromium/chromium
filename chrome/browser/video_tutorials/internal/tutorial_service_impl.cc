// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/video_tutorials/internal/tutorial_service_impl.h"

#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "chrome/browser/video_tutorials/internal/config.h"
#include "chrome/browser/video_tutorials/internal/proto_conversions.h"
#include "chrome/browser/video_tutorials/prefs.h"
#include "chrome/browser/video_tutorials/switches.h"

namespace video_tutorials {

TutorialServiceImpl::TutorialServiceImpl(
    std::unique_ptr<TutorialManager> tutorial_manager,
    std::unique_ptr<TutorialFetcher> tutorial_fetcher,
    PrefService* pref_service)
    : tutorial_manager_(std::move(tutorial_manager)),
      tutorial_fetcher_(std::move(tutorial_fetcher)),
      pref_service_(pref_service) {
  StartFetchIfNecessary();
}

TutorialServiceImpl::~TutorialServiceImpl() = default;

void TutorialServiceImpl::GetTutorials(MultipleItemCallback callback) {
  tutorial_manager_->GetTutorials(std::move(callback));
}

void TutorialServiceImpl::GetTutorial(FeatureType feature_type,
                                      SingleItemCallback callback) {
  tutorial_manager_->GetTutorial(feature_type, std::move(callback));
}

void TutorialServiceImpl::StartFetchIfNecessary() {
  base::Time last_update_time = pref_service_->GetTime(kLastUpdatedTimeKey);
  bool needs_update =
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kVideoTutorialsInstantFetch) ||
      ((base::Time::Now() - last_update_time) > Config::GetFetchFrequency());
  if (needs_update) {
    tutorial_fetcher_->StartFetchForTutorials(base::BindOnce(
        &TutorialServiceImpl::OnFetchFinished, weak_ptr_factory_.GetWeakPtr()));
  }
}

void TutorialServiceImpl::OnFetchFinished(
    bool success,
    std::unique_ptr<std::string> response_body) {
  pref_service_->SetTime(kLastUpdatedTimeKey, base::Time::Now());

  if (!success || !response_body)
    return;

  proto::ServerResponse response_proto;
  bool parse_success = response_proto.ParseFromString(*response_body.get());
  if (!parse_success)
    return;

  auto tutorial_groups = std::make_unique<std::vector<TutorialGroup>>();
  TutorialGroupsFromServerResponseProto(&response_proto, tutorial_groups.get());

  tutorial_manager_->SaveGroups(std::move(tutorial_groups));
}

const std::vector<std::string>& TutorialServiceImpl::GetSupportedLanguages() {
  return tutorial_manager_->GetSupportedLanguages();
}

const std::vector<std::string>&
TutorialServiceImpl::GetAvailableLanguagesForTutorial(
    FeatureType feature_type) {
  return tutorial_manager_->GetAvailableLanguagesForTutorial(feature_type);
}

base::Optional<std::string> TutorialServiceImpl::GetPreferredLocale() {
  return tutorial_manager_->GetPreferredLocale();
}

void TutorialServiceImpl::SetPreferredLocale(const std::string& locale) {
  tutorial_manager_->SetPreferredLocale(locale);
}

}  // namespace video_tutorials
