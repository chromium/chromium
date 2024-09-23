// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/birch/birch_keyed_service.h"

#include <memory>
#include <optional>

#include "ash/birch/birch_model.h"
#include "ash/shell.h"
#include "base/functional/bind.h"
#include "chrome/browser/ash/file_suggest/file_suggest_keyed_service_factory.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/birch/birch_calendar_provider.h"
#include "chrome/browser/ui/ash/birch/birch_file_suggest_provider.h"
#include "chrome/browser/ui/ash/birch/birch_last_active_provider.h"
#include "chrome/browser/ui/ash/birch/birch_lost_media_provider.h"
#include "chrome/browser/ui/ash/birch/birch_most_visited_provider.h"
#include "chrome/browser/ui/ash/birch/birch_recent_tabs_provider.h"
#include "chrome/browser/ui/ash/birch/birch_release_notes_provider.h"
#include "chrome/browser/ui/ash/birch/birch_self_share_provider.h"
#include "chrome/browser/ui/ash/birch/refresh_token_waiter.h"
#include "chrome/grit/chrome_unscaled_resources.h"
#include "components/favicon/core/favicon_service.h"
#include "components/favicon_base/favicon_types.h"
#include "ui/base/models/image_model.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"

namespace ash {

namespace {

// The file within the cryptohome to save removed items into.
constexpr char kRemovedBirchItemsFile[] = "birch/removed_items.pb";

// The minimum size to query for favicons.
constexpr int kMinimumFaviconSize = 32;

// Utility method to resize the raw favicon bitmap into a `gfx::Image`.
gfx::Image ResizeLargeIcon(
    const favicon_base::FaviconRawBitmapResult& db_result,
    int desired_size) {
  gfx::Image image = gfx::Image::CreateFrom1xPNGBytes(db_result.bitmap_data);

  SkBitmap resized = skia::ImageOperations::Resize(
      image.AsBitmap(), skia::ImageOperations::RESIZE_BEST, desired_size,
      desired_size);

  return gfx::Image::CreateFrom1xBitmap(resized);
}

// Callback for FaviconService icon lookup that uses the
// `favicon_base::FaviconRawBitmapResult`.
void OnGotFaviconImageRaw(
    base::OnceCallback<void(const ui::ImageModel&)> callback,
    const favicon_base::FaviconRawBitmapResult& image_result) {
  if (!image_result.is_valid()) {
    std::move(callback).Run(ui::ImageModel());
    return;
  }

  gfx::Image image = ResizeLargeIcon(image_result, kMinimumFaviconSize);

  std::move(callback).Run(ui::ImageModel::FromImage(image));
}

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
      lost_media_provider_(std::make_unique<BirchLostMediaProvider>(profile)),
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
  if (recent_tabs_provider_for_test_) {
    return recent_tabs_provider_for_test_;
  }
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
  if (release_notes_provider_for_test_) {
    return release_notes_provider_for_test_;
  }
  return release_notes_provider_.get();
}

BirchDataProvider* BirchKeyedService::GetSelfShareProvider() {
  if (self_share_provider_for_test_) {
    return self_share_provider_for_test_;
  }
  return self_share_provider_.get();
}

BirchDataProvider* BirchKeyedService::GetLostMediaProvider() {
  if (lost_media_provider_for_test_) {
    return lost_media_provider_for_test_;
  }
  return lost_media_provider_.get();
}

void BirchKeyedService::WaitForRefreshTokens(base::OnceClosure callback) {
  refresh_token_waiter_->Wait(std::move(callback));
}

base::FilePath BirchKeyedService::GetRemovedItemsFilePath() {
  return profile_->GetPath().AppendASCII(kRemovedBirchItemsFile);
}

void BirchKeyedService::RemoveFileItemFromLauncher(const base::FilePath& path) {
  std::vector<base::FilePath> file_paths;
  file_paths.push_back(path);
  auto* file_suggest_keyed_service =
      FileSuggestKeyedServiceFactory::GetInstance()->GetService(profile_);
  file_suggest_keyed_service->RemoveSuggestionsAndNotify(file_paths);
}

void BirchKeyedService::GetFaviconImage(
    const GURL& url,
    const bool is_page_url,
    base::OnceCallback<void(const ui::ImageModel&)> callback) {
  favicon::FaviconService* service =
      FaviconServiceFactory::GetInstance()->GetForProfile(
          profile_, ServiceAccessType::EXPLICIT_ACCESS);
  favicon_base::IconType icon_type = favicon_base::IconType::kFavicon;

  if (is_page_url) {
    const favicon_base::IconTypeSet icon_types = {icon_type};
    service->GetLargestRawFaviconForPageURL(
        url, {icon_types}, kMinimumFaviconSize,
        base::BindOnce(&OnGotFaviconImageRaw, std::move(callback)),
        &cancelable_task_tracker_);
  } else {
    service->GetRawFavicon(
        url, icon_type, kMinimumFaviconSize,
        base::BindOnce(&OnGotFaviconImageRaw, std::move(callback)),
        &cancelable_task_tracker_);
  }
}

ui::ImageModel BirchKeyedService::GetChromeBackupIcon() {
  return ui::ImageModel::FromImageSkia(
      *ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
          IDR_CHROME_APP_ICON_192));
}

void BirchKeyedService::ShutdownBirch() {
  if (is_shutdown_) {
    return;
  }
  is_shutdown_ = true;
  shell_observation_.Reset();
  Shell::Get()->birch_model()->SetClientAndInit(nullptr);
  calendar_provider_->Shutdown();
}

}  // namespace ash
