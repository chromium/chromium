// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/video_tutorials/internal/tutorial_service_impl.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "chrome/browser/video_tutorials/internal/config.h"
#include "chrome/browser/video_tutorials/internal/proto_conversions.h"
#include "chrome/browser/video_tutorials/prefs.h"

namespace video_tutorials {

TutorialServiceImpl::TutorialServiceImpl(
    std::unique_ptr<TutorialManager> tutorial_manager,
    std::unique_ptr<TutorialFetcher> tutorial_fetcher,
    PrefService* pref_service)
    : tutorial_manager_(std::move(tutorial_manager)),
      tutorial_fetcher_(std::move(tutorial_fetcher)),
      pref_service_(pref_service) {}

TutorialServiceImpl::~TutorialServiceImpl() = default;

void TutorialServiceImpl::GetTutorials(MultipleItemCallback callback) {
  tutorial_manager_->GetTutorials(std::move(callback));
}

void TutorialServiceImpl::GetTutorial(FeatureType feature_type,
                                      SingleItemCallback callback) {
  tutorial_manager_->GetTutorials(base::BindOnce(
      &TutorialServiceImpl::OnGetTutorials, weak_ptr_factory_.GetWeakPtr(),
      std::move(callback), feature_type));
}

void TutorialServiceImpl::OnGetTutorials(SingleItemCallback callback,
                                         FeatureType feature_type,
                                         std::vector<Tutorial> tutorials) {
  for (const Tutorial& tutorial : tutorials) {
    if (tutorial.feature == feature_type) {
      std::move(callback).Run(tutorial);
      return;
    }
  }

  std::move(callback).Run(base::nullopt);
}

void TutorialServiceImpl::StartFetchIfNecessary() {
  base::Time last_update_time = pref_service_->GetTime(kLastUpdatedTimeKey);
  bool needs_update =
      ((base::Time::Now() - last_update_time) > Config::GetFetchFrequency());
  if (needs_update) {
    tutorial_fetcher_->StartFetchForTutorials(base::BindOnce(
        &TutorialServiceImpl::OnFetchFinished, weak_ptr_factory_.GetWeakPtr()));
  }
}

void TutorialServiceImpl::OnFetchFinished(
    bool success,
    std::unique_ptr<std::string> response_body) {
  // TODO(shaktisahu): Save tutorials to the database.
  if (!success || !response_body)
    return;

  proto::ServerResponse response_proto;
  bool parse_success = response_proto.ParseFromString(*response_body.get());
  if (!parse_success)
    return;

  auto tutorial_groups = std::make_unique<std::vector<TutorialGroup>>();
  TutorialGroupsFromServerResponseProto(&response_proto, tutorial_groups.get());

  auto lambda = [](bool success) {};
  tutorial_manager_->SaveGroups(std::move(tutorial_groups),
                                base::BindOnce(std::move(lambda)));
}

const std::vector<Language>& TutorialServiceImpl::GetSupportedLanguages() {
  return tutorial_manager_->GetSupportedLanguages();
}

std::string TutorialServiceImpl::GetPreferredLocale() {
  return tutorial_manager_->GetPreferredLocale();
}

void TutorialServiceImpl::SetPreferredLocale(const std::string& locale) {
  tutorial_manager_->SetPreferredLocale(locale);
}

}  // namespace video_tutorials
