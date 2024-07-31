// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/leveldb_proto/public/shared_proto_database_client_list.h"

#include <stddef.h>

#include <string>

#include "base/metrics/field_trial_params.h"

#include "base/notreached.h"
#include "components/leveldb_proto/internal/leveldb_proto_feature_list.h"

namespace leveldb_proto {

// static
std::string SharedProtoDatabaseClientList::ProtoDbTypeToString(
    ProtoDbType db_type) {
  // Please update the variant LevelDBClient in
  // //tools/metrics/histograms/metadata/leveldb_proto/histograms.xml
  // to match the strings returned here.
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
    case ProtoDbType::FEED_STREAM_DATABASE:
      return "FeedStreamDatabase";
    case ProtoDbType::PERSISTED_STATE_DATABASE:
      return "PersistedStateDatabase";
    case ProtoDbType::UPBOARDING_QUERY_TILE_STORE:
      return "UpboardingQueryTileStore";
    case ProtoDbType::NEARBY_SHARE_PUBLIC_CERTIFICATE_DATABASE:
      return "NearbySharePublicCertificateDatabase";
    case ProtoDbType::VIDEO_TUTORIALS_DATABASE:
      return "VideoTutorialsDatabase";
    case ProtoDbType::FEED_KEY_VALUE_DATABASE:
      return "FeedKeyValueDatabase";
    case ProtoDbType::CART_DATABASE:
      return "CartDatabase";
    case ProtoDbType::COMMERCE_SUBSCRIPTION_DATABASE:
      return "CommerceSubscriptionDatabase";
    case ProtoDbType::MERCHANT_TRUST_SIGNAL_DATABASE:
      return "MerchantTrustSignalEventDatabase";
    case ProtoDbType::SHARE_HISTORY_DATABASE:
      return "ShareHistoryDatabase";
    case ProtoDbType::SHARE_RANKING_DATABASE:
      return "ShareRankingDatabase";
    case ProtoDbType::SEGMENT_INFO_DATABASE:
      return "SegmentInfoDatabase";
    case ProtoDbType::SIGNAL_DATABASE:
      return "SignalDatabase";
    case ProtoDbType::SIGNAL_STORAGE_CONFIG_DATABASE:
      return "SignalStorageConfigDatabase";
    case ProtoDbType::VIDEO_TUTORIALS_V2_DATABASE:
      return "VideoTutorialsV2Database";
    case ProtoDbType::COUPON_DATABASE:
      return "CouponDatabase";
    case ProtoDbType::PAGE_ENTITY_METADATA_STORE:
      return "PageEntityMetadataDatabase";
    case ProtoDbType::WEBRTC_VIDEO_STATS_DB:
      return "WebrtcVideoStatsDB";
    case ProtoDbType::PERSISTENT_ORIGIN_TRIALS:
      return "PersistentOriginTrials";
    case ProtoDbType::NEARBY_PRESENCE_LOCAL_PUBLIC_CREDENTIAL_DATABASE:
      return "NearbyPresenceLocalPublicCredentialDatabase";
    case ProtoDbType::NEARBY_PRESENCE_PRIVATE_CREDENTIAL_DATABASE:
      return "NearbyPresencePrivateCredentialDatabase";
    case ProtoDbType::NEARBY_PRESENCE_REMOTE_PUBLIC_CREDENTIAL_DATABASE:
      return "NearbyPresenceRemotePublicCredentialDatabase";
    case ProtoDbType::DISCOUNTS_DATABASE:
      return "DiscountsDatabase";
    case ProtoDbType::COMMERCE_PARCEL_TRACKING_DATABASE:
      return "CommerceParcelTrackingDatabase";
    case ProtoDbType::CLIENT_CERTIFICATES_DATABASE:
      return "ClientCertificatesDatabase";
    case ProtoDbType::LAST:
      NOTREACHED_IN_MIGRATION();
      return std::string();
  }
}

// static
bool SharedProtoDatabaseClientList::ShouldUseSharedDB(ProtoDbType db_type) {
  for (size_t i = 0; kBlocklistedDbForSharedImpl[i] != ProtoDbType::LAST; ++i) {
    if (kBlocklistedDbForSharedImpl[i] == db_type)
      return false;
  }

  if (!base::FeatureList::IsEnabled(kProtoDBSharedMigration))
    return false;

  return true;
}

}  // namespace leveldb_proto
