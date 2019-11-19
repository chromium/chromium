// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/leveldb_proto/public/shared_proto_database_client_list.h"

#include <stddef.h>

#include <string>

#include "base/metrics/field_trial_params.h"
#include "base/stl_util.h"

#include "components/leveldb_proto/internal/leveldb_proto_feature_list.h"

namespace leveldb_proto {

namespace {
const char* const kDBNameParamPrefix = "migrate_";
}  // namespace

// static
std::string SharedProtoDatabaseClientList::ProtoDbTypeToString(
    ProtoDbType db_type) {
  // Please update the suffix LevelDBClients in histograms.xml to match the
  // strings returned here.
  switch (db_type) {
    case ProtoDbType::TEST_DATABASE0:
      return "TestDatabase0";
    case ProtoDbType::TEST_DATABASE1:
      return "TestDatabase1";
    case ProtoDbType::TEST_DATABASE2:
      return "TestDatabase2";
    case ProtoDbType::FEATURE_ENGAGEMENT_EVENT:
      return "FeatureEngagementTrackerEventStore";
    case ProtoDbType::FEATURE_ENGAGEMENT_AVAILABILITY:
      return "FeatureEngagementTrackerAvailabilityStore";
    case ProtoDbType::USAGE_STATS_WEBSITE_EVENT:
      return "UsageStatsWebsiteEvent";
    case ProtoDbType::USAGE_STATS_SUSPENSION:
      return "UsageStatsSuspension";
    case ProtoDbType::USAGE_STATS_TOKEN_MAPPING:
      return "UsageStatsTokenMapping";
    case ProtoDbType::DOM_DISTILLER_STORE:
      return "DomDistillerStore";
    case ProtoDbType::DOWNLOAD_STORE:
      return "DownloadService";
    case ProtoDbType::CACHED_IMAGE_METADATA_STORE:
      return "CachedImageFetcherDatabase";
    case ProtoDbType::FEED_CONTENT_DATABASE:
      return "FeedContentDatabase";
    case ProtoDbType::FEED_JOURNAL_DATABASE:
      return "FeedJournalDatabase";
    case ProtoDbType::REMOTE_SUGGESTIONS_DATABASE:
      return "NTPSnippets";
    case ProtoDbType::REMOTE_SUGGESTIONS_IMAGE_DATABASE:
      return "NTPSnippetImages";
    case ProtoDbType::NOTIFICATION_SCHEDULER_ICON_STORE:
      return "NotificationSchedulerIcons";
    case ProtoDbType::NOTIFICATION_SCHEDULER_IMPRESSION_STORE:
      return "NotificationSchedulerImpressions";
    case ProtoDbType::NOTIFICATION_SCHEDULER_NOTIFICATION_STORE:
      return "NotificationSchedulerNotifications";
    case ProtoDbType::BUDGET_DATABASE:
      return "BudgetManager";
    case ProtoDbType::STRIKE_DATABASE:
      return "StrikeService";
    case ProtoDbType::HINT_CACHE_STORE:
      return "PreviewsHintCacheStore";
    case ProtoDbType::DOWNLOAD_DB:
      return "DownloadDB";
    case ProtoDbType::VIDEO_DECODE_STATS_DB:
      return "VideoDecodeStatsDB";
    case ProtoDbType::GCM_KEY_STORE:
      return "GCMKeyStore";
    case ProtoDbType::SHARED_DB_METADATA:
      return "Metadata";
    case ProtoDbType::PRINT_JOB_DATABASE:
      return "PrintJobDatabase";
    case ProtoDbType::LAST:
      NOTREACHED();
      return std::string();
  }
}

// static
bool SharedProtoDatabaseClientList::ShouldUseSharedDB(ProtoDbType db_type) {
  for (size_t i = 0; kWhitelistedDbForSharedImpl[i] != ProtoDbType::LAST; ++i) {
    if (kWhitelistedDbForSharedImpl[i] == db_type)
      return true;
  }

  if (!base::FeatureList::IsEnabled(kProtoDBSharedMigration))
    return false;

  std::string name =
      SharedProtoDatabaseClientList::ProtoDbTypeToString(db_type);
  return base::GetFieldTrialParamByFeatureAsBool(
      kProtoDBSharedMigration, kDBNameParamPrefix + name, false);
}

}  // namespace leveldb_proto
