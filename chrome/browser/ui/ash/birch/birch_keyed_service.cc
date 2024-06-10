// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/birch/birch_keyed_service.h"

#include <memory>
#include <optional>

#include "ash/birch/birch_item.h"
#include "ash/birch/birch_model.h"
#include "ash/shell.h"
#include "base/functional/bind.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/birch/birch_calendar_provider.h"
#include "chrome/browser/ui/ash/birch/birch_file_suggest_provider.h"
#include "chrome/browser/ui/ash/birch/birch_last_active_provider.h"
#include "chrome/browser/ui/ash/birch/birch_most_visited_provider.h"
#include "chrome/browser/ui/ash/birch/birch_recent_tabs_provider.h"
#include "chrome/browser/ui/ash/birch/birch_release_notes_provider.h"
#include "chrome/browser/ui/ash/birch/birch_self_share_provider.h"
#include "chrome/browser/ui/ash/birch/birch_weather_v2_provider.h"
#include "chrome/browser/ui/ash/birch/refresh_token_waiter.h"

namespace ash {

namespace {

// The file within the cryptohome to save removed items into.
constexpr char kRemovedBirchItemsFile[] = "birch/removed_items.pb";

}  // namespace

BirchKeyedService::BirchKeyedService(Profile* profile)
    : profile_(profile),
      calendar_provider_(std::make_unique<BirchCalendarProvider>(profile)),
      file_suggest_provider_(
          std::make_unique<BirchFileSuggestProvider>(profile)),
      recent_tabs_provider_(std::make_unique<BirchRecentTabsProvider>(profile)),
      last_active_provider_(std::make_unique<BirchLastActiveProvider>(profile)),
      most_visited_provider_(
          std::make_unique<BirchMostVisitedProvider>(profile)),
      release_notes_provider_(
          std::make_unique<BirchReleaseNotesProvider>(profile)),
      self_share_provider_(std::make_unique<BirchSelfShareProvider>(profile)),
      weather_v2_provider_(std::make_unique<BirchWeatherV2Provider>(
          profile,
          base::BindRepeating([](std::vector<BirchWeatherItem> items) {
            Shell::Get()->birch_model()->SetWeatherItems(std::move(items));
          }))),
      refresh_token_waiter_(std::make_unique<RefreshTokenWaiter>(profile)) {
  calendar_provider_->Initialize();
  Shell::Get()->birch_model()->SetClientAndInit(this);
  shell_observation_.Observe(Shell::Get());
}

BirchKeyedService::~BirchKeyedService() {
  ShutdownBirch();
}

void BirchKeyedService::OnShellDestroying() {
  ShutdownBirch();
}

BirchDataProvider* BirchKeyedService::GetCalendarProvider() {
  if (calendar_provider_for_test_) {
    return calendar_provider_for_test_;
  }
  return calendar_provider_.get();
}

BirchDataProvider* BirchKeyedService::GetFileSuggestProvider() {
  if (file_suggest_provider_for_test_) {
    return file_suggest_provider_for_test_;
  }
  return file_suggest_provider_.get();
}

BirchDataProvider* BirchKeyedService::GetRecentTabsProvider() {
  return recent_tabs_provider_.get();
}

BirchDataProvider* BirchKeyedService::GetLastActiveProvider() {
  if (last_active_provider_for_test_) {
    return last_active_provider_for_test_;
  }
  return last_active_provider_.get();
}

BirchDataProvider* BirchKeyedService::GetMostVisitedProvider() {
  if (most_visited_provider_for_test_) {
    return most_visited_provider_for_test_;
  }
  return most_visited_provider_.get();
}

BirchDataProvider* BirchKeyedService::GetReleaseNotesProvider() {
  return release_notes_provider_.get();
}

BirchDataProvider* BirchKeyedService::GetSelfShareProvider() {
  return self_share_provider_.get();
}

BirchDataProvider* BirchKeyedService::GetWeatherV2Provider() {
  return weather_v2_provider_.get();
}

void BirchKeyedService::WaitForRefreshTokens(base::OnceClosure callback) {
  refresh_token_waiter_->Wait(std::move(callback));
}

base::FilePath BirchKeyedService::GetRemovedItemsFilePath() {
  return profile_->GetPath().AppendASCII(kRemovedBirchItemsFile);
}

void BirchKeyedService::ShutdownBirch() {
  if (is_shutdown_) {
    return;
  }
  is_shutdown_ = true;
  shell_observation_.Reset();
  Shell::Get()->birch_model()->SetClientAndInit(nullptr);
  calendar_provider_->Shutdown();
  weather_v2_provider_->Shutdown();
}

}  // namespace ash
