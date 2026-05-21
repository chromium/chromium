// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/google_update_settings.h"

#include "base/task/lazy_thread_pool_task_runner.h"
#include "build/build_config.h"
#include "chrome/installer/util/client_id_backup_file_manager.h"
#include "components/crash/core/app/crashpad.h"

namespace {

base::LazyThreadPoolSequencedTaskRunner g_collect_stats_consent_task_runner =
    LAZY_THREAD_POOL_SEQUENCED_TASK_RUNNER_INITIALIZER(
        base::TaskTraits(base::MayBlock(),
                         base::TaskPriority::USER_VISIBLE,
                         base::TaskShutdownBehavior::BLOCK_SHUTDOWN));

}  // namespace

// static
base::SequencedTaskRunner*
GoogleUpdateSettings::CollectStatsConsentTaskRunner() {
  // TODO(fdoray): Use LazyThreadPoolSequencedTaskRunner::GetRaw() here instead
  // of .Get().get() when it's added to the API, http://crbug.com/40524407.
  return g_collect_stats_consent_task_runner.Get().get();
}

// static
bool GoogleUpdateSettings::GetCollectStatsConsent() {
  auto& backup_client_id_file_manager =
      ClientIdBackupFileManager::GetInstance();
  return backup_client_id_file_manager.ClientIdFromCacheOrDisk().has_value();
}

// static
bool GoogleUpdateSettings::SetCollectStatsConsent(bool consented) {
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  crash_reporter::SetUploadConsent(consented);
#endif

  auto& backup_client_id_file_manager =
      ClientIdBackupFileManager::GetInstance();
  bool has_client_id_in_backup =
      backup_client_id_file_manager.ClientIdFromCacheOrDisk().has_value();

  if (consented == has_client_id_in_backup) {
    return true;
  }

  if (!consented) {
    return backup_client_id_file_manager.ClearClientId(
        base::PassKey<GoogleUpdateSettings>());
  }

  // An empty client ID means that `consented` is true, but the ID itself is not
  // yet known.
  return backup_client_id_file_manager.SetClientId(
      base::PassKey<GoogleUpdateSettings>(), {});
}

// static
metrics::MetricsReportingLevel
GoogleUpdateSettings::GetMetricsReportingLevel() {
  // TODO(crbug.com/483043192): Implement this method.
  return metrics::MetricsReportingLevel::kNone;
}

// static
bool GoogleUpdateSettings::SetMetricsReportingLevel(
    metrics::MetricsReportingLevel level) {
  // TODO(crbug.com/483043192): Implement this method.
  return false;
}

std::unique_ptr<metrics::ClientInfo>
GoogleUpdateSettings::LoadMetricsClientInfo() {
  auto& backup_client_id_file_manager =
      ClientIdBackupFileManager::GetInstance();

  std::optional<std::string> client_id =
      backup_client_id_file_manager.ClientIdFromCache();
  if (!client_id.has_value() || client_id->empty()) {
    return nullptr;
  }

  auto client_info = std::make_unique<metrics::ClientInfo>();
  client_info->client_id = *std::move(client_id);
  return client_info;
}

void GoogleUpdateSettings::StoreMetricsClientInfo(
    const metrics::ClientInfo& client_info) {
  auto& backup_client_id_file_manager =
      ClientIdBackupFileManager::GetInstance();

  // Make sure that the user has chosen to send metrics.
  if (!backup_client_id_file_manager.ClientIdFromCacheOrDisk()) {
    return;
  }

  backup_client_id_file_manager.SetClientId(
      base::PassKey<GoogleUpdateSettings>(), client_info.client_id);
}

// GetLastRunTime() and SetLastRunTime() are not implemented for posix. Their
// current return values signal failure which the caller is designed to
// handle.

// static
int GoogleUpdateSettings::GetLastRunTime() {
  return -1;
}

// static
bool GoogleUpdateSettings::SetLastRunTime() {
  return false;
}

// static
bool GoogleUpdateSettings::GetCollectStatsConsentDefault(
    bool* stats_consent_default) {
  // We never know the default status of the consent button on POSIX platforms.
  return false;
}
