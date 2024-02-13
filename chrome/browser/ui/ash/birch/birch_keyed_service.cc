// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/birch/birch_keyed_service.h"

#include <memory>
#include <optional>

#include "ash/birch/birch_model.h"
#include "ash/shell.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/birch/birch_client_impl.h"
#include "chrome/browser/ui/ash/birch/birch_file_suggest_provider.h"
#include "chrome/browser/ui/ash/birch/birch_recent_tabs_provider.h"

namespace ash {

BirchKeyedService::BirchKeyedService(Profile* profile)
    : file_suggest_provider_(
          std::make_unique<BirchFileSuggestProvider>(profile)),
      recent_tabs_provider_(
          std::make_unique<BirchRecentTabsProvider>(profile)) {
  birch_client_impl_ = std::make_unique<BirchClientImpl>(profile);
  Shell::Get()->birch_model()->SetClient(birch_client_impl_.get());
  shell_observation_.Observe(Shell::Get());
}

BirchKeyedService::~BirchKeyedService() {
  ShutdownBirch();
}

void BirchKeyedService::OnShellDestroying() {
  ShutdownBirch();
}

void BirchKeyedService::ShutdownBirch() {
  if (is_shutdown_) {
    return;
  }
  is_shutdown_ = true;
  shell_observation_.Reset();
  Shell::Get()->birch_model()->SetClient(nullptr);
}

void BirchKeyedService::RequestBirchDataFetch() {
  recent_tabs_provider_->GetRecentTabs();
  file_suggest_provider_->RequestDataFetch();
}

}  // namespace ash
