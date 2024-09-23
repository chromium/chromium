// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/skyvault/local_files_migration_page_handler.h"

#include "base/functional/callback.h"
#include "base/i18n/time_formatting.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/timer/wall_clock_timer.h"
#include "chrome/browser/ash/policy/skyvault/policy_utils.h"
#include "chrome/browser/ui/webui/ash/skyvault/local_files_migration.mojom-forward.h"
#include "chrome/browser/ui/webui/ash/skyvault/local_files_migration.mojom-shared.h"
#include "chrome/browser/ui/webui/ash/skyvault/local_files_migration.mojom.h"

namespace policy::local_user_files {

namespace {

// Converts a CloudProvider enum value to its corresponding Mojo representation.
mojom::CloudProvider ConvertCloudProviderToMojo(CloudProvider cloud_provider) {
  switch (cloud_provider) {
    case CloudProvider::kNotSpecified:
      NOTREACHED_NORETURN()
          << "Case should not be reached, cloud provider must be specified.";
    case CloudProvider::kGoogleDrive:
      return mojom::CloudProvider::kGoogleDrive;
    case CloudProvider::kOneDrive:
      return mojom::CloudProvider::kOneDrive;
  }
}

// Rounds up the remaining time to the nearest displayable unit.
//
// If the remaining time is:
//  * >= 59.5 minutes: Rounds up to the nearest hour
//  * Otherwise: Rounds up to the nearest minute
base::TimeDelta RoundUpRemainingTime(base::TimeDelta remaining_time) {
  // Round to the nearest second for precise calculations.
  remaining_time = base::Seconds(std::round(remaining_time.InSecondsF()));

  // Round up to hours if at least 59.5 minutes remain
  if (remaining_time >= base::Seconds(59 * 60 + 30)) {
    return base::Hours((remaining_time + base::Minutes(30)).InHours());
  }

  // Otherwise, round up to minutes
  return base::Minutes((remaining_time + base::Seconds(30)).InMinutes());
}

// Calculates the time interval until the next UI update.
base::TimeDelta ComputeNextUIUpdateTime(const base::TimeDelta& remaining_time) {
  base::TimeDelta rounded_remaining_time = RoundUpRemainingTime(remaining_time);

  base::TimeDelta delta;
  if (rounded_remaining_time.InHours() > 1) {
    delta = base::Hours(rounded_remaining_time.InHours() - 1);
  } else {
    delta = base::Minutes(rounded_remaining_time.InMinutes() - 1);
  }

  return remaining_time - delta;
}

// Converts the remaining time to a Mojo representation, rounding up to the
// nearest appropriate unit (hour or minute).
mojom::TimeUnitAndValuePtr ConvertRemainingTimeToMojo(
    const base::TimeDelta& remaining_time) {
  const base::TimeDelta rounded_remaining_time =
      RoundUpRemainingTime(remaining_time);

  if (rounded_remaining_time.InHours() >= 1) {
    return mojom::TimeUnitAndValue::New(mojom::TimeUnit::kHours,
                                        rounded_remaining_time.InHours());
  } else {
    return mojom::TimeUnitAndValue::New(mojom::TimeUnit::kMinutes,
                                        rounded_remaining_time.InMinutes());
  }
}

}  // namespace

LocalFilesMigrationPageHandler::LocalFilesMigrationPageHandler(
    content::WebUI* web_ui,
    Profile* profile,
    CloudProvider cloud_provider,
    base::Time migration_start_time,
    UserActionCallback callback,
    mojo::PendingRemote<mojom::Page> page,
    mojo::PendingReceiver<mojom::PageHandler> receiver)
    : profile_(profile),
      web_ui_(web_ui),
      cloud_provider_(cloud_provider),
      migration_start_time_(migration_start_time),
      callback_(std::move(callback)),
      receiver_(this, std::move(receiver)),
      page_(std::move(page)) {
  base::TimeDelta remaining_time = migration_start_time - base::Time::Now();
  base::TimeDelta ui_update_interval = ComputeNextUIUpdateTime(remaining_time);
  ui_update_timer_.Start(
      FROM_HERE, base::Time::Now() + ui_update_interval,
      base::BindOnce(&LocalFilesMigrationPageHandler::UpdateRemainingTime,
                     weak_factory_.GetWeakPtr()));
}

LocalFilesMigrationPageHandler::~LocalFilesMigrationPageHandler() = default;

void LocalFilesMigrationPageHandler::GetInitialDialogInfo(
    GetInitialDialogInfoCallback callback) {
  base::TimeDelta remaining_time = migration_start_time_ - base::Time::Now();
  if (remaining_time <= base::TimeDelta()) {
    // TODO(aidazolic): Define error behavior.
    return;
  }

  mojom::CloudProvider cloud_provider =
      ConvertCloudProviderToMojo(cloud_provider_);
  mojom::TimeUnitAndValuePtr remaining_time_ptr =
      ConvertRemainingTimeToMojo(remaining_time);
  std::string formatted_date_time = base::UTF16ToUTF8(
      base::TimeFormatShortDateAndTimeWithTimeZone(migration_start_time_));

  std::move(callback).Run(cloud_provider, std::move(remaining_time_ptr),
                          formatted_date_time);
}

void LocalFilesMigrationPageHandler::UploadNow() {
  if (callback_) {
    std::move(callback_).Run(UserAction::kUploadNow);
  }
}

void LocalFilesMigrationPageHandler::Close() {
  if (callback_) {
    std::move(callback_).Run(UserAction::kDismiss);
  }
}

void LocalFilesMigrationPageHandler::UpdateRemainingTime() {
  base::Time time_now = base::Time::Now();
  base::TimeDelta remaining_time = migration_start_time_ - time_now;
  // Don't schedule an update if the time is up.
  if (remaining_time <= base::TimeDelta()) {
    return;
  }

  base::TimeDelta ui_update_interval = ComputeNextUIUpdateTime(remaining_time);
  ui_update_timer_.Start(
      FROM_HERE, time_now + ui_update_interval,
      base::BindOnce(&LocalFilesMigrationPageHandler::UpdateRemainingTime,
                     weak_factory_.GetWeakPtr()));

  page_->UpdateRemainingTime(ConvertRemainingTimeToMojo(remaining_time));
}

}  // namespace policy::local_user_files
