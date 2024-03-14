// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/birch/birch_keyed_service.h"

#include <memory>
#include <optional>

#include "ash/birch/birch_model.h"
#include "ash/shell.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/birch/birch_calendar_provider.h"
#include "chrome/browser/ui/ash/birch/birch_file_suggest_provider.h"
#include "chrome/browser/ui/ash/birch/birch_recent_tabs_provider.h"
#include "chrome/browser/ui/ash/birch/birch_release_notes_provider.h"
#include "chrome/browser/ui/ash/birch/refresh_token_waiter.h"

namespace ash {

BirchKeyedService::BirchKeyedService(Profile* profile)
    : calendar_provider_(std::make_unique<BirchCalendarProvider>(profile)),
      file_suggest_provider_(
          std::make_unique<BirchFileSuggestProvider>(profile)),
      recent_tabs_provider_(std::make_unique<BirchRecentTabsProvider>(profile)),
      release_notes_provider_(
          std::make_unique<BirchReleaseNotesProvider>(profile)),
      refresh_token_waiter_(std::make_unique<RefreshTokenWaiter>(profile)) {
  calendar_provider_->Initialize();
  Shell::Get()->birch_model()->SetClient(this);
  shell_observation_.Observe(Shell::Get());
}

BirchKeyedService::~BirchKeyedService() {
  ShutdownBirch();
}

void BirchKeyedService::OnShellDestroying() {
  ShutdownBirch();
}

BirchDataProvider* BirchKeyedService::GetCalendarProvider() {
  return calendar_provider_.get();
}

BirchDataProvider* BirchKeyedService::GetFileSuggestProvider() {
  return file_suggest_provider_.get();
}

BirchDataProvider* BirchKeyedService::GetRecentTabsProvider() {
  return recent_tabs_provider_.get();
}

BirchDataProvider* BirchKeyedService::GetReleaseNotesProvider() {
  return release_notes_provider_.get();
}

void BirchKeyedService::WaitForRefreshTokens(base::OnceClosure callback) {
  refresh_token_waiter_->Wait(std::move(callback));
}

void BirchKeyedService::ShutdownBirch() {
  if (is_shutdown_) {
    return;
  }
  is_shutdown_ = true;
  shell_observation_.Reset();
  Shell::Get()->birch_model()->SetClient(nullptr);
  calendar_provider_->Shutdown();
}

}  // namespace ash
