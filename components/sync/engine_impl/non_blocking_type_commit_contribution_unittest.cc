// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine_impl/non_blocking_type_commit_contribution.h"

#include <string>

#include "base/base64.h"
#include "base/sha1.h"
#include "components/sync/base/hash_util.h"
#include "components/sync/base/model_type.h"
#include "components/sync/base/unique_position.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

namespace {

using sync_pb::EntitySpecifics;
using sync_pb::SyncEntity;

const char kTag[] = "tag";
const char kValue[] = "value";
const char kURL[] = "url";
const char kTitle[] = "title";

EntitySpecifics GeneratePreferenceSpecifics(const std::string& tag,
                                            const std::string& value) {
  EntitySpecifics specifics;
  specifics.mutable_preference()->set_name(tag);
  specifics.mutable_preference()->set_value(value);
  return specifics;
}

EntitySpecifics GenerateBookmarkSpecifics(const std::string& url,
                                          const std::string& title) {
  EntitySpecifics specifics;
  specifics.mutable_bookmark()->set_url(url);
  specifics.mutable_bookmark()->set_title(title);
  return specifics;
}

TEST(NonBlockingTypeCommitContribution, PopulateCommitProtoDefault) {
  const int64_t kBaseVersion = 7;
  base::Time creation_time =
      base::Time::UnixEpoch() + base::TimeDelta::FromDays(1);
  base::Time modification_time =
      creation_time + base::TimeDelta::FromSeconds(1);

  EntityData data;

  data.client_tag_hash = kTag;
  data.specifics = GeneratePreferenceSpecifics(kTag, kValue);

  // These fields are not really used for much, but we set them anyway
  // to make this item look more realistic.
  data.creation_time = creation_time;
  data.modification_time = modification_time;
  data.non_unique_name = "Name:";

  CommitRequestData request_data;
  request_data.entity = data.PassToPtr();
  request_data.sequence_number = 2;
  request_data.base_version = kBaseVersion;
  base::Base64Encode(base::SHA1HashString(data.specifics.SerializeAsString()),
                     &request_data.specifics_hash);

  SyncEntity entity;
  NonBlockingTypeCommitContribution::PopulateCommitProto(request_data, &entity);

  // Exhaustively verify the populated SyncEntity.
  EXPECT_TRUE(entity.id_string().empty());
  EXPECT_EQ(7, entity.version());
  EXPECT_EQ(modification_time.ToJsTime(), entity.mtime());
  EXPECT_EQ(creation_time.ToJsTime(), entity.ctime());
  EXPECT_FALSE(entity.name().empty());
  EXPECT_FALSE(entity.client_defined_unique_tag().empty());
  EXPECT_EQ(kTag, entity.specifics().preference().name());
  EXPECT_FALSE(entity.deleted());
  EXPECT_EQ(kValue, entity.specifics().preference().value());
  EXPECT_TRUE(entity.parent_id_string().empty());
  EXPECT_FALSE(entity.unique_position().has_custom_compressed_v1());
  EXPECT_EQ(0, entity.position_in_parent());
}

TEST(NonBlockingTypeCommitContribution, PopulateCommitProtoBookmark) {
  const int64_t kBaseVersion = 7;
  base::Time creation_time =
      base::Time::UnixEpoch() + base::TimeDelta::FromDays(1);
  base::Time modification_time =
      creation_time + base::TimeDelta::FromSeconds(1);

  EntityData data;

  data.id = "bookmark";
  data.specifics = GenerateBookmarkSpecifics(kURL, kTitle);

  // These fields are not really used for much, but we set them anyway
  // to make this item look more realistic.
  data.creation_time = creation_time;
  data.modification_time = modification_time;
  data.non_unique_name = "Name:";
  data.parent_id = "ParentOf:";
  data.is_folder = true;
  syncer::UniquePosition uniquePosition = syncer::UniquePosition::FromInt64(
      10, syncer::UniquePosition::RandomSuffix());
  data.unique_position = uniquePosition.ToProto();

  CommitRequestData request_data;
  request_data.entity = data.PassToPtr();
  request_data.sequence_number = 2;
  request_data.base_version = kBaseVersion;
  base::Base64Encode(base::SHA1HashString(data.specifics.SerializeAsString()),
                     &request_data.specifics_hash);

  SyncEntity entity;
  NonBlockingTypeCommitContribution::PopulateCommitProto(request_data, &entity);

  // Exhaustively verify the populated SyncEntity.
  EXPECT_FALSE(entity.id_string().empty());
  EXPECT_EQ(7, entity.version());
  EXPECT_EQ(modification_time.ToJsTime(), entity.mtime());
  EXPECT_EQ(creation_time.ToJsTime(), entity.ctime());
  EXPECT_FALSE(entity.name().empty());
  EXPECT_TRUE(entity.client_defined_unique_tag().empty());
  EXPECT_EQ(kURL, entity.specifics().bookmark().url());
  EXPECT_FALSE(entity.deleted());
  EXPECT_EQ(kTitle, entity.specifics().bookmark().title());
  EXPECT_TRUE(entity.folder());
  EXPECT_FALSE(entity.parent_id_string().empty());
  EXPECT_TRUE(entity.unique_position().has_custom_compressed_v1());
  EXPECT_NE(0, entity.position_in_parent());
}

// Verifies how PASSWORDS protos are committed on the wire, making sure the data
// is properly encrypted except for password metadata.
TEST(NonBlockingTypeCommitContribution,
     PopulateCommitProtoPasswordWithoutCustomPassphrase) {
  const std::string kEncryptedPasswordBlob = "encryptedpasswordblob";
  const std::string kMetadataUrl = "http://foo.com";
  const int64_t kBaseVersion = 7;

  EntityData data;
  data.client_tag_hash = kTag;
  data.specifics.mutable_password()->mutable_encrypted()->set_blob(
      kEncryptedPasswordBlob);
  data.specifics.mutable_password()->mutable_unencrypted_metadata()->set_url(
      kMetadataUrl);

  CommitRequestData request_data;
  request_data.entity = data.PassToPtr();
  request_data.sequence_number = 2;
  request_data.base_version = kBaseVersion;
  base::Base64Encode(base::SHA1HashString(data.specifics.SerializeAsString()),
                     &request_data.specifics_hash);

  base::ObserverList<TypeDebugInfoObserver>::Unchecked observers;
  DataTypeDebugInfoEmitter debug_info_emitter(PASSWORDS, &observers);
  NonBlockingTypeCommitContribution contribution(
      PASSWORDS, sync_pb::DataTypeContext(), {request_data},
      /*worker=*/nullptr,
      /*cryptographer*/ nullptr, PassphraseType::IMPLICIT_PASSPHRASE,
      &debug_info_emitter,
      /*only_commit_specifics=*/false);

  sync_pb::ClientToServerMessage msg;
  contribution.AddToCommitMessage(&msg);
  contribution.CleanUp();

  ASSERT_EQ(1, msg.commit().entries().size());
  SyncEntity entity = msg.commit().entries(0);

  // Exhaustively verify the populated SyncEntity.
  EXPECT_TRUE(entity.id_string().empty());
  EXPECT_EQ(7, entity.version());
  EXPECT_EQ("encrypted", entity.name());
  EXPECT_EQ(kTag, entity.client_defined_unique_tag());
  EXPECT_FALSE(entity.deleted());
  EXPECT_FALSE(entity.specifics().has_encrypted());
  EXPECT_TRUE(entity.specifics().has_password());
  EXPECT_EQ(kEncryptedPasswordBlob,
            entity.specifics().password().encrypted().blob());
  EXPECT_EQ(kMetadataUrl,
            entity.specifics().password().unencrypted_metadata().url());
  EXPECT_TRUE(entity.parent_id_string().empty());
  EXPECT_FALSE(entity.unique_position().has_custom_compressed_v1());
  EXPECT_EQ(0, entity.position_in_parent());
}

// Same as above but uses CUSTOM_PASSPHRASE. In this case, field
// |unencrypted_metadata| should be cleared.
TEST(NonBlockingTypeCommitContribution,
     PopulateCommitProtoPasswordWithCustomPassphrase) {
  const std::string kEncryptedPasswordBlob = "encryptedpasswordblob";
  const std::string kMetadataUrl = "http://foo.com";
  const int64_t kBaseVersion = 7;

  EntityData data;
  data.client_tag_hash = kTag;
  data.specifics.mutable_password()->mutable_encrypted()->set_blob(
      kEncryptedPasswordBlob);
  data.specifics.mutable_password()->mutable_unencrypted_metadata()->set_url(
      kMetadataUrl);

  CommitRequestData request_data;
  request_data.entity = data.PassToPtr();
  request_data.sequence_number = 2;
  request_data.base_version = kBaseVersion;
  base::Base64Encode(base::SHA1HashString(data.specifics.SerializeAsString()),
                     &request_data.specifics_hash);

  base::ObserverList<TypeDebugInfoObserver>::Unchecked observers;
  DataTypeDebugInfoEmitter debug_info_emitter(PASSWORDS, &observers);
  NonBlockingTypeCommitContribution contribution(
      PASSWORDS, sync_pb::DataTypeContext(), {request_data},
      /*worker=*/nullptr,
      /*cryptographer*/ nullptr, PassphraseType::CUSTOM_PASSPHRASE,
      &debug_info_emitter,
      /*only_commit_specifics=*/false);

  sync_pb::ClientToServerMessage msg;
  contribution.AddToCommitMessage(&msg);
  contribution.CleanUp();

  ASSERT_EQ(1, msg.commit().entries().size());
  SyncEntity entity = msg.commit().entries(0);

  // Exhaustively verify the populated SyncEntity.
  EXPECT_TRUE(entity.id_string().empty());
  EXPECT_EQ(7, entity.version());
  EXPECT_EQ("encrypted", entity.name());
  EXPECT_EQ(kTag, entity.client_defined_unique_tag());
  EXPECT_FALSE(entity.deleted());
  EXPECT_FALSE(entity.specifics().has_encrypted());
  EXPECT_TRUE(entity.specifics().has_password());
  EXPECT_EQ(kEncryptedPasswordBlob,
            entity.specifics().password().encrypted().blob());
  EXPECT_FALSE(entity.specifics().password().has_unencrypted_metadata());
  EXPECT_TRUE(entity.parent_id_string().empty());
  EXPECT_FALSE(entity.unique_position().has_custom_compressed_v1());
  EXPECT_EQ(0, entity.position_in_parent());
}

}  // namespace

}  // namespace syncer
