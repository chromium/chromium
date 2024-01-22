// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LEVELDB_PROTO_PUBLIC_SHARED_PROTO_DATABASE_CLIENT_LIST_H_
#define COMPONENTS_LEVELDB_PROTO_PUBLIC_SHARED_PROTO_DATABASE_CLIENT_LIST_H_

#include <stddef.h>

#include <string>

#include "base/component_export.h"

namespace leveldb_proto {

const char* const kFeatureEngagementName = "FeatureEngagement";

// The enum values are used to index into the shared database. Do not rearrange
// or reuse the integer values. Add new database types at the end of the enum,
// and update the string mapping in ProtoDbTypeToString(). Also update the
// variant LevelDBClient in
// //tools/metrics/histograms/metadata/leveldb_proto/histograms.xml to match the
// strings for the types.
enum class ProtoDbType {
  TEST_DATABASE0 = 0,
  TEST_DATABASE1 = 1,
  TEST_DATABASE2 = 2,
  FEATURE_ENGAGEMENT_EVENT = 3,
  FEATURE_ENGAGEMENT_AVAILABILITY = 4,
  USAGE_STATS_WEBSITE_EVENT = 5,
  USAGE_STATS_SUSPENSION = 6,
  USAGE_STATS_TOKEN_MAPPING = 7,
  DOM_DISTILLER_STORE = 8,
  DOWNLOAD_STORE = 9,
  CACHED_IMAGE_METADATA_STORE = 10,
  FEED_CONTENT_DATABASE = 11,
  FEED_JOURNAL_DATABASE = 12,
  REMOTE_SUGGESTIONS_DATABASE = 13,
  REMOTE_SUGGESTIONS_IMAGE_DATABASE = 14,
  NOTIFICATION_SCHEDULER_ICON_STORE = 15,
  NOTIFICATION_SCHEDULER_IMPRESSION_STORE = 16,
  NOTIFICATION_SCHEDULER_NOTIFICATION_STORE = 17,
  BUDGET_DATABASE = 18,
  STRIKE_DATABASE = 19,
  HINT_CACHE_STORE = 20,
  DOWNLOAD_DB = 21,
  VIDEO_DECODE_STATS_DB = 22,
  PRINT_JOB_DATABASE = 23,
  // DB is not tied to a profile, will always be unique.
  GCM_KEY_STORE = 24,
  // DB Used by shared database, will always be unique.
  SHARED_DB_METADATA = 25,
  FEED_STREAM_DATABASE = 26,
  PERSISTED_STATE_DATABASE = 27,
  UPBOARDING_QUERY_TILE_STORE = 28,
  NEARBY_SHARE_PUBLIC_CERTIFICATE_DATABASE = 29,
  VIDEO_TUTORIALS_DATABASE = 30,
  FEED_KEY_VALUE_DATABASE = 31,
  CART_DATABASE = 32,
  COMMERCE_SUBSCRIPTION_DATABASE = 33,
  MERCHANT_TRUST_SIGNAL_DATABASE = 34,
  SHARE_HISTORY_DATABASE = 35,
  SHARE_RANKING_DATABASE = 36,
  SEGMENT_INFO_DATABASE = 37,
  SIGNAL_DATABASE = 38,
  SIGNAL_STORAGE_CONFIG_DATABASE = 39,
  VIDEO_TUTORIALS_V2_DATABASE = 40,
  COUPON_DATABASE = 41,
  PAGE_ENTITY_METADATA_STORE = 42,
  WEBRTC_VIDEO_STATS_DB = 43,
  PERSISTENT_ORIGIN_TRIALS = 44,
  NEARBY_PRESENCE_LOCAL_PUBLIC_CREDENTIAL_DATABASE = 45,
  NEARBY_PRESENCE_PRIVATE_CREDENTIAL_DATABASE = 46,
  NEARBY_PRESENCE_REMOTE_PUBLIC_CREDENTIAL_DATABASE = 47,
  DISCOUNTS_DATABASE = 48,
  COMMERCE_PARCEL_TRACKING_DATABASE = 49,
  CLIENT_CERTIFICATES_DATABASE = 50,
  LAST,
};

// List of databases that need to keep using unique db instances. New databases
// shouldn't be here unless they have a good reason.
constexpr ProtoDbType kBlocklistedDbForSharedImpl[]{
    // DB is not tied to a profile, will always be unique.
    ProtoDbType::GCM_KEY_STORE,
    // DB Used by shared database, will always be unique.
    ProtoDbType::SHARED_DB_METADATA,
    // Marks the end of list.
    ProtoDbType::LAST,
};

// Add any obsolete databases in this list so that, if the data is no longer
// needed.
constexpr ProtoDbType kObsoleteSharedProtoDbTypeClients[] = {
    ProtoDbType::DOM_DISTILLER_STORE,
    ProtoDbType::FEED_CONTENT_DATABASE,
    ProtoDbType::FEED_JOURNAL_DATABASE,
    ProtoDbType::VIDEO_TUTORIALS_DATABASE,
    ProtoDbType::VIDEO_TUTORIALS_V2_DATABASE,
    ProtoDbType::LAST,  // Marks the end of list.
};

class COMPONENT_EXPORT(LEVELDB_PROTO) SharedProtoDatabaseClientList {
 public:
  // Determines if the given |db_type| should use a unique or shared DB.
  static bool ShouldUseSharedDB(ProtoDbType db_type);

  // Converts a ProtoDbType to a string, which is used for UMA metrics and field
  // trials.
  static std::string ProtoDbTypeToString(ProtoDbType db_type);
};

}  // namespace leveldb_proto

#endif  // COMPONENTS_LEVELDB_PROTO_PUBLIC_SHARED_PROTO_DATABASE_CLIENT_LIST_H_
