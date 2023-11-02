// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/service_worker/service_worker_database.h"

#include <stddef.h>
#include <stdint.h>

#include <string>

#include "base/containers/contains.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/unguessable_token.h"
#include "components/services/storage/service_worker/service_worker_database.pb.h"
#include "net/base/features.h"
#include "services/network/public/cpp/web_sandbox_flags.h"
#include "services/network/public/mojom/referrer_policy.mojom-shared.h"
#include "services/network/public/mojom/web_sandbox_flags.mojom-shared.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/frame/policy_container.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_object.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_registration.mojom.h"
#include "third_party/leveldatabase/src/include/leveldb/write_batch.h"
#include "url/origin.h"

namespace storage {

namespace {

using ::testing::ElementsAreArray;
using ::testing::IsEmpty;

using RegistrationData = mojom::ServiceWorkerRegistrationData;
using RegistrationDataPtr = mojom::ServiceWorkerRegistrationDataPtr;
using ResourceRecordPtr = mojom::ServiceWorkerResourceRecordPtr;

struct AvailableIds {
  int64_t reg_id;
  int64_t res_id;
  int64_t ver_id;

  AvailableIds() : reg_id(-1), res_id(-1), ver_id(-1) {}
  ~AvailableIds() {}
};

GURL URL(const GURL& origin, const std::string& path) {
  EXPECT_TRUE(origin.is_valid());
  EXPECT_EQ(origin, origin.DeprecatedGetOriginAsURL());
  GURL out(origin.spec() + path);
  EXPECT_TRUE(out.is_valid());
  return out;
}

ResourceRecordPtr CreateResource(int64_t resource_id,
                                 const GURL& url,
                                 uint64_t size_bytes) {
  EXPECT_TRUE(url.is_valid());
  return mojom::ServiceWorkerResourceRecord::New(resource_id, url, size_bytes);
}

ServiceWorkerDatabase* CreateDatabase(const base::FilePath& path) {
  return new ServiceWorkerDatabase(path);
}

ServiceWorkerDatabase* CreateDatabaseInMemory() {
  return new ServiceWorkerDatabase(base::FilePath());
}

void VerifyRegistrationData(const RegistrationData& expected,
                            const RegistrationData& actual) {
  EXPECT_EQ(expected.registration_id, actual.registration_id);
  EXPECT_EQ(expected.scope, actual.scope);
  EXPECT_EQ(expected.key, actual.key);
  EXPECT_EQ(expected.script, actual.script);
  EXPECT_EQ(expected.script_type, actual.script_type);
  EXPECT_EQ(expected.update_via_cache, actual.update_via_cache);
  EXPECT_EQ(expected.version_id, actual.version_id);
  EXPECT_EQ(expected.is_active, actual.is_active);
  EXPECT_EQ(expected.fetch_handler_type, actual.fetch_handler_type);
  EXPECT_EQ(expected.last_update_check, actual.last_update_check);
  EXPECT_EQ(expected.used_features, actual.used_features);
  EXPECT_EQ(expected.resources_total_size_bytes,
            actual.resources_total_size_bytes);
  EXPECT_EQ(expected.script_response_time, actual.script_response_time);
  EXPECT_EQ(expected.cross_origin_embedder_policy,
            actual.cross_origin_embedder_policy);
  EXPECT_EQ(expected.ancestor_frame_type, actual.ancestor_frame_type);
  if (expected.policy_container_policies) {
    EXPECT_EQ(expected.policy_container_policies,
              actual.policy_container_policies);
  } else {
    // Null policy container policies will be read as default policies because
    // there's always going to be a default Cross Origin Embedder Policy
    EXPECT_EQ(blink::mojom::PolicyContainerPolicies::New(),
              actual.policy_container_policies);
  }
}

void VerifyResourceRecords(const std::vector<ResourceRecordPtr>& expected,
                           const std::vector<ResourceRecordPtr>& actual) {
  ASSERT_EQ(expected.size(), actual.size());
  for (size_t i = 0; i < expected.size(); ++i) {
    EXPECT_EQ(expected[i]->resource_id, actual[i]->resource_id);
    EXPECT_EQ(expected[i]->url, actual[i]->url);
    EXPECT_EQ(expected[i]->size_bytes, actual[i]->size_bytes);
  }
}

network::CrossOriginEmbedderPolicy CrossOriginEmbedderPolicyNone() {
  return network::CrossOriginEmbedderPolicy();
}

network::CrossOriginEmbedderPolicy CrossOriginEmbedderPolicyRequireCorp() {
  network::CrossOriginEmbedderPolicy out;
  out.value = network::mojom::CrossOriginEmbedderPolicyValue::kRequireCorp;
  return out;
}

network::CrossOriginEmbedderPolicy CrossOriginEmbedderPolicyCredentialless() {
  network::CrossOriginEmbedderPolicy out;
  out.value = network::mojom::CrossOriginEmbedderPolicyValue::kCredentialless;
  return out;
}

std::vector<mojom::ServiceWorkerUserDataPtr> CreateUserData(
    int64_t registration_id,
    const std::vector<std::pair<std::string, std::string>>& key_value_pairs) {
  std::vector<mojom::ServiceWorkerUserDataPtr> out;
  for (auto& kv : key_value_pairs) {
    out.push_back(mojom::ServiceWorkerUserData::New(registration_id, kv.first,
                                                    kv.second));
  }
  return out;
}

}  // namespace

TEST(ServiceWorkerDatabaseTest, OpenDatabase) {
  base::ScopedTempDir database_dir;
  ASSERT_TRUE(database_dir.CreateUniqueTempDir());
  std::unique_ptr<ServiceWorkerDatabase> database(
      CreateDatabase(database_dir.GetPath()));

  // Should be false because the database does not exist at the path.
  EXPECT_EQ(ServiceWorkerDatabase::Status::kErrorNotFound,
            database->LazyOpen(false));

  EXPECT_EQ(ServiceWorkerDatabase::Status::kOk, database->LazyOpen(true));

  database.reset(CreateDatabase(database_dir.GetPath()));
  EXPECT_EQ(ServiceWorkerDatabase::Status::kOk, database->LazyOpen(false));
}

TEST(ServiceWorkerDatabaseTest, OpenDatabase_InMemory) {
  std::unique_ptr<ServiceWorkerDatabase> database(CreateDatabaseInMemory());

  // Should be false because the database does not exist in memory.
  EXPECT_EQ(ServiceWorkerDatabase::Status::kErrorNotFound,
            database->LazyOpen(false));

  EXPECT_EQ(ServiceWorkerDatabase::Status::kOk, database->LazyOpen(true));
  database.reset(CreateDatabaseInMemory());

  // Should be false because the database is not persistent.
  EXPECT_EQ(ServiceWorkerDatabase::Status::kErrorNotFound,
            database->LazyOpen(false));
}

TEST(ServiceWorkerDatabaseTest, DatabaseVersion_ValidSchemaVersion) {
  GURL origin("https://example.com");
  std::unique_ptr<ServiceWorkerDatabase> database(CreateDatabaseInMemory());
  EXPECT_EQ(ServiceWorkerDatabase::Status::kOk, database->LazyOpen(true));

  // Opening a new database does not write anything, so its schema version
  // should be 0.
  int64_t db_version = -1;
  EXPECT_EQ(ServiceWorkerDatabase::Status::kOk,
            database->ReadDatabaseVersion(&db_version));
  EXPECT_EQ(0u, db_version);

  // First writing triggers database initialization and bumps the schema
  // version.
  std::vector<ResourceRecordPtr> resources;
  resources.push_back(CreateResource(1, URL(origin, "/resource"), 10));
  ServiceWorkerDatabase::DeletedVersion deleted_version;
  RegistrationData data;
  data.scope = origin;
  data.key = blink::StorageKey(url::Origin::Create(data.scope));
  data.resources_total_size_bytes = 10;
  ASSERT_EQ(ServiceWorkerDatabase::Status::kOk,
            database->WriteRegistration(data, resources, &deleted_version));

  EXPECT_EQ(ServiceWorkerDatabase::Status::kOk,
            database->ReadDatabaseVersion(&db_version));
  EXPECT_LT(0, db_version);
}

TEST(ServiceWorkerDatabaseTest, DatabaseVersion_ObsoleteSchemaVersion) {
  base::ScopedTempDir database_dir;
  ASSERT_TRUE(database_dir.CreateUniqueTempDir());
  std::unique_ptr<ServiceWorkerDatabase> database(
      CreateDatabase(database_dir.GetPath()));
  EXPECT_EQ(ServiceWorkerDatabase::Status::kOk, database->LazyOpen(true));

  // First writing triggers database initialization and bumps the schema
  // version.
  GURL origin("https://example.com");
  std::vector<ResourceRecordPtr> resources;
  resources.push_back(CreateResource(1, URL(origin, "/resource"), 10));
  ServiceWorkerDatabase::DeletedVersion deleted_version;
  RegistrationData data;
  data.scope = origin;
  data.key = blink::StorageKey(url::Origin::Create(data.scope));
  data.resources_total_size_bytes = 10;
  ASSERT_EQ(ServiceWorkerDatabase::Status::kOk,
            database->WriteRegistration(data, resources, &deleted_version));
  int64_t db_version = -1;
  ASSERT_EQ(ServiceWorkerDatabase::Status::kOk,
            database->ReadDatabaseVersion(&db_version));
  ASSERT_LT(0, db_version);

  // Emulate an obsolete schema version.
  int64_t old_db_version = 1;
  leveldb::WriteBatch batch;
  batch.Put("INITDATA_DB_VERSION", base::NumberToString(old_db_version));
  ASSERT_EQ(ServiceWorkerDatabase::Status::kOk, database->WriteBatch(&batch));
  db_version = -1;
  EXPECT_EQ(ServiceWorkerDatabase::Status::kOk,
            database->ReadDatabaseVersion(&db_version));
  ASSERT_EQ(old_db_version, db_version);

  // Opening the database whose schema version is obsolete should fail.
  database.reset(CreateDatabase(database_dir.GetPath()));
  EXPECT_EQ(ServiceWorkerDatabase::Status::kErrorFailed,
            database->LazyOpen(true));
}

TEST(ServiceWorkerDatabaseTest, DatabaseVersion_CorruptedSchemaVersion) {
  base::ScopedTempDir database_dir;
  ASSERT_TRUE(database_dir.CreateUniqueTempDir());
  std::unique_ptr<ServiceWorkerDatabase> database(
      CreateDatabase(database_dir.GetPath()));
  EXPECT_EQ(ServiceWorkerDatabase::Status::kOk, database->LazyOpen(true));

  // First writing triggers database initialization and bumps the schema
  // version.
  GURL origin("https://example.com");
  std::vector<ResourceRecordPtr> resources;
  resources.push_back(CreateResource(1, URL(origin, "/resource"), 10));
  ServiceWorkerDatabase::DeletedVersion deleted_version;
  RegistrationData data;
  data.scope = origin;
  data.key = blink::StorageKey(url::Origin::Create(data.scope));
  data.resources_total_size_bytes = 10;
  ASSERT_EQ(ServiceWorkerDatabase::Status::kOk,
            database->WriteRegistration(data, resources, &deleted_version));
  int64_t db_version = -1;
  ASSERT_EQ(ServiceWorkerDatabase::Status::kOk,
            database->ReadDatabaseVersion(&db_version));
  ASSERT_LT(0, db_version);

  // Emulate a corrupted schema version.
  int64_t corrupted_db_version = -10;
  leveldb::WriteBatch batch;
  batch.Put("INITDATA_DB_VERSION", base::NumberToString(corrupted_db_version));
  ASSERT_EQ(ServiceWorkerDatabase::Status::kOk, database->WriteBatch(&batch));
  db_version = -1;
  EXPECT_EQ(ServiceWorkerDatabase::Status::kErrorCorrupted,
            database->ReadDatabaseVersion(&db_version));

  // Opening the database whose schema version is corrupted should fail.
  database.reset(CreateDatabase(database_dir.GetPath()));
  EXPECT_EQ(ServiceWorkerDatabase::Status::kErrorCorrupted,
            database->LazyOpen(true));
}

TEST(ServiceWorkerDatabaseTest, GetNextAvailableIds) {
  base::ScopedTempDir database_dir;
  ASSERT_TRUE(database_dir.CreateUniqueTempDir());
  std::unique_ptr<ServiceWorkerDatabase> database(
      CreateDatabase(database_dir.GetPath()));

  GURL origin("https://example.com");

  // The database has never been used, so returns initial values.
  AvailableIds ids;
  EXPECT_EQ(
      ServiceWorkerDatabase::Status::kOk,
      database->GetNextAvailableIds(&ids.reg_id, &ids.ver_id, &ids.res_id));
  EXPECT_EQ(0, ids.reg_id);
  EXPECT_EQ(0, ids.ver_id);
  EXPECT_EQ(0, ids.res_id);

  ASSERT_EQ(ServiceWorkerDatabase::Status::kOk, database->LazyOpen(true));
  EXPECT_EQ(
      ServiceWorkerDatabase::Status::kOk,
      database->GetNextAvailableIds(&ids.reg_id, &ids.ver_id, &ids.res_id));
  EXPECT_EQ(0, ids.reg_id);
  EXPECT_EQ(0, ids.ver_id);
  EXPECT_EQ(0, ids.res_id);

  // Writing uncommitted resources bumps the next available resource id.
  const std::vector<int64_t> kUncommittedIds = {0, 1, 3, 5, 6, 10};
  EXPECT_EQ(ServiceWorkerDatabase::Status::kOk,
            database->WriteUncommittedResourceIds(kUncommittedIds));
  EXPECT_EQ(
      ServiceWorkerDatabase::Status::kOk,
      database->GetNextAvailableIds(&ids.reg_id, &ids.ver_id, &ids.res_id));
  EXPECT_EQ(0, ids.reg_id);
  EXPECT_EQ(0, ids.ver_id);
  EXPECT_EQ(11, ids.res_id);

  // Writing purgeable resources bumps the next available id.
  const std::vector<int64_t> kPurgeableIds = {4, 12, 16, 17, 20};
  EXPECT_EQ(ServiceWorkerDatabase::Status::kOk,
            database->WriteUncommittedResourceIds(kPurgeableIds));
  EXPECT_EQ(
      ServiceWorkerDatabase::Status::kOk,
      database->GetNextAvailableIds(&ids.reg_id, &ids.ver_id, &ids.res_id));
  EXPECT_EQ(0, ids.reg_id);
  EXPECT_EQ(0, ids.ver_id);
  EXPECT_EQ(21, ids.res_id);

  // Writing a registration bumps the next available registration and version
  // ids.
  std::vector<ResourceRecordPtr> resources1;
  RegistrationData data1;
  ServiceWorkerDatabase::DeletedVersion deleted_version;
  data1.registration_id = 100;
  data1.scope = URL(origin, "/foo");
  data1.key = blink::StorageKey(url::Origin::Create(data1.scope));
  data1.script = URL(origin, "/script1.js");
  data1.version_id = 200;
  data1.resources_total_size_bytes = 300;
  resources1.push_back(CreateResource(1, data1.script, 300));
  ASSERT_EQ(ServiceWorkerDatabase::Status::kOk,
            database->WriteRegistration(data1, resources1, &deleted_version));

  EXPECT_EQ(
      ServiceWorkerDatabase::Status::kOk,
      database->GetNextAvailableIds(&ids.reg_id, &ids.ver_id, &ids.res_id));
  EXPECT_EQ(101, ids.reg_id);
  EXPECT_EQ(201, ids.ver_id);
  EXPECT_EQ(21, ids.res_id);

  // Writing a registration whose ids are lower than the stored ones should not
  // bump the next available ids.
  RegistrationData data2;
  data2.registration_id = 10;
  data2.scope = URL(origin, "/bar");
  data2.key = blink::StorageKey(url::Origin::Create(data2.scope));
  data2.script = URL(origin, "/script2.js");
  data2.version_id = 20;
  data2.resources_total_size_bytes = 400;
  std::vector<ResourceRecordPtr> resources2;
  resources2.push_back(CreateResource(2, data2.script, 400));
  ASSERT_EQ(ServiceWorkerDatabase::Status::kOk,
            database->WriteRegistration(data2, resources2, &deleted_version));

  // Same with resources.
  int64_t kLowResourceId = 15;
  std::vector<int64_t> resource_ids = {kLowResourceId};
  EXPECT_EQ(ServiceWorkerDatabase::Status::kOk,
            database->WriteUncommittedResourceIds(resource_ids));

  // Close and reopen the database to verify the stored values.
  database.reset(CreateDatabase(database_dir.GetPath()));

  EXPECT_EQ(
      ServiceWorkerDatabase::Status::kOk,
      database->GetNextAvailableIds(&ids.reg_id, &ids.ver_id, &ids.res_id));
  EXPECT_EQ(101, ids.reg_id);
  EXPECT_EQ(201, ids.ver_id);
  EXPECT_EQ(21, ids.res_id);
}

TEST(ServiceWorkerDatabaseTest, GetStorageKeysWithRegistrations) {
  std::unique_ptr<ServiceWorkerDatabase> database(CreateDatabaseInMemory());

  std::set<blink::StorageKey> keys;
  EXPECT_EQ(ServiceWorkerDatabase::Status::kOk,
            database->GetStorageKeysWithRegistrations(&keys));
  EXPECT_TRUE(keys.empty());

  ServiceWorkerDatabase::DeletedVersion deleted_version;

  GURL origin1 = GURL("https://example.com");
  blink::StorageKey key1(url::Origin::Create(origin1));
  RegistrationData data1;
  data1.registration_id = 123;
  data1.scope = URL(origin1, "/foo");
  data1.key = key1;
  data1.script = URL(origin1, "/script1.js");
  data1.version_id = 456;
  data1.resources_total_size_bytes = 100;
  std::vector<ResourceRecordPtr> resources1;
  resources1.push_back(CreateResource(1, data1.script, 100));
  ASSERT_EQ(ServiceWorkerDatabase::Status::kOk,
            database->WriteRegistration(data1, resources1, &deleted_version));

  GURL origin2 = GURL("https://www.example.com");
  blink::StorageKey key2(url::Origin::Create(origin2));
  RegistrationData data2;
  data2.registration_id = 234;
  data2.scope = URL(origin2, "/bar");
  data2.key = key2;
  data2.script = URL(origin2, "/script2.js");
  data2.version_id = 567;
  data2.resources_total_size_bytes = 200;
  std::vector<ResourceRecordPtr> resources2;
  resources2.push_back(CreateResource(2, data2.script, 200));
  ASSERT_EQ(ServiceWorkerDatabase::Status::kOk,
            database->WriteRegistration(data2, resources2, &deleted_version));

  GURL origin3 = GURL("https://example.org");
  blink::StorageKey key3(url::Origin::Create(origin3));
  RegistrationData data3;
  data3.registration_id = 345;
  data3.scope = URL(origin3, "/hoge");
  data3.key = key3;
  data3.script = URL(origin3, "/script3.js");
  data3.version_id = 678;
  data3.resources_total_size_bytes = 300;
  std::vector<ResourceRecordPtr> resources3;
  resources3.push_back(CreateResource(3, data3.script, 300));
  ASSERT_EQ(ServiceWorkerDatabase::Status::kOk,
            database->WriteRegistration(data3, resources3, &deleted_version));

  // |key3| has two registrations.
  RegistrationData data4;
  data4.registration_id = 456;
  data4.scope = URL(origin3, "/fuga");
  data4.key = key3;
  data4.script = URL(origin3, "/script4.js");
  data4.version_id = 789;
  data4.resources_total_size_bytes = 400;
  std::vector<ResourceRecordPtr> resources4;
  resources4.push_back(CreateResource(4, data4.script, 400));
  ASSERT_EQ(ServiceWorkerDatabase::Status::kOk,
            database->WriteRegistration(data4, resources4, &deleted_version));

  // Add some partitioned keys.
  // Make sure kThirdPartyStoragePartitioning is enabled so the keys are
  // inserted as partitioned.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      net::features::kThirdPartyStoragePartitioning);

  GURL origin5 = GURL("https://example.org");
  net::SchemefulSite top_level_site1(GURL("https://toplevel.com"));
  blink::StorageKey key5 = blink::StorageKey::CreateForTesting(
      url::Origin::Create(origin5), top_level_site1);
  RegistrationData data5;
  data5.registration_id = 567;
  data5.scope = URL(origin5, "/hoge");
  data5.key = key5;
  data5.script = URL(origin5, "/script5.js");
  data5.version_id = 890;
  data5.resources_total_size_bytes = 500;
  std::vector<ResourceRecordPtr> resources5;
  resources5.push_back(CreateResource(5, data5.script, 500));
  ASSERT_EQ(ServiceWorkerDatabase::Status::kOk,
            database->WriteRegistration(data5, resources5, &deleted_version));

  GURL origin6 = GURL("https://example.org");
  net::SchemefulSite top_level_site2(GURL("https://toplevel2.com"));
  blink::StorageKey key6 = blink::StorageKey::CreateForTesting(
      url::Origin::Create(origin6), top_level_site2);
  RegistrationData data6;
  data6.registration_id = 678;
  data6.scope = URL(origin6, "/hoge");
  data6.key = key6;
  data6.script = URL(origin6, "/script6.js");
  data6.version_id = 8910;
  data6.resources_total_size_bytes = 600;
  std::vector<ResourceRecordPtr> resources6;
  resources6.push_back(CreateResource(6, data6.script, 600));
  ASSERT_EQ(ServiceWorkerDatabase::Status::kOk,
            database->WriteRegistration(data6, resources6, &deleted_version));

  scoped_feature_list.Reset();
  // Because kThirdPartyStoragePartitioning is disabled now we shouldn't get the
  // partitioned keys in the following checks.

  // Keys with nonces should always be gettable.
  GURL origin7 = GURL("https://example.org");
  base::UnguessableToken token = base::UnguessableToken::Create();
  blink::StorageKey key7 =
      blink::StorageKey::CreateWithNonce(url::Origin::Create(origin7), token);
  RegistrationData data7;
  data7.registration_id = 789;
  data7.scope = URL(origin7, "/hoge");
  data7.key = key7;
  data7.script = URL(origin7, "/script7.js");
  data7.version_id = 91011;
  data7.resources_total_size_bytes = 700;
  std::vector<ResourceRecordPtr> resources7;
  resources7.push_back(CreateResource(7, data7.script, 700));
  ASSERT_EQ(ServiceWorkerDatabase::Status::kOk,
            database->WriteRegistration(data7, resources7, &deleted_version));

  keys.clear();
  EXPECT_EQ(ServiceWorkerDatabase::Status::kOk,
            database->GetStorageKeysWithRegistrations(&keys));
  EXPECT_EQ(4U, keys.size());
  EXPECT_TRUE(base::Contains(keys, key1));
  EXPECT_TRUE(base::Contains(keys, key2));
  EXPECT_TRUE(base::Contains(keys, key3));
  EXPECT_TRUE(base::Contains(keys, key7));

  // |key3| has another registration, so should not remove it from the
  // unique origin list.
  ASSERT_EQ(ServiceWorkerDatabase::Status::kOk,
            database->DeleteRegistration(data4.registration_id, key3,
                                         &deleted_version));
  EXPECT_EQ(data4.registration_id, deleted_version.registration_id);

  keys.clear();
  EXPECT_EQ(ServiceWorkerDatabase::Status::kOk,
            database->GetStorageKeysWithRegistrations(&keys));
  EXPECT_EQ(4U, keys.size());
  EXPECT_TRUE(base::Contains(keys, key1));
  EXPECT_TRUE(base::Contains(keys, key2));
  EXPECT_TRUE(base::Contains(keys, key3));
  EXPECT_TRUE(base::Contains(keys, key7));

  // |key3| should be removed from the unique origin list.
  ASSERT_EQ(ServiceWorkerDatabase::Status::kOk,
            database->DeleteRegistration(data3.registration_id, key3,
                                         &deleted_version));
  EXPECT_EQ(data3.registration_id, deleted_version.registration_id);

  keys.clear();
  EXPECT_EQ(ServiceWorkerDatabase::Status::kOk,
            database->GetStorageKeysWithRegistrations(&keys));
  EXPECT_EQ(3U, keys.size());
  EXPECT_TRUE(base::Contains(keys, key1));
  EXPECT_TRUE(base::Contains(keys, key2));
  EXPECT_TRUE(base::Contains(keys, key7));

  // Now re-enable kThirdPartyStoragePartitioning and check for the partitioned
  // keys.
  scoped_feature_list.InitAndEnableFeature(
      net::features::kThirdPartyStoragePartitioning);

  keys.clear();
  EXPECT_EQ(ServiceWorkerDatabase::Status::kOk,
            database->GetStorageKeysWithRegistrations(&keys));
  EXPECT_EQ(5U, keys.size());
  EXPECT_TRUE(base::Contains(keys, key1));
  EXPECT_TRUE(base::Contains(keys, key2));
  EXPECT_TRUE(base::Contains(keys, key5));
  EXPECT_TRUE(base::Contains(keys, key6));
  EXPECT_TRUE(base::Contains(keys, key7));
}

TEST(ServiceWorkerDatabaseTest, GetRegistrationsForStorageKey) {
  std::unique_ptr<ServiceWorkerDatabase> database(CreateDatabaseInMemory());

  GURL origin1("https://example.com");
  GURL origin2("https://www.example.com");
  GURL origin3("https://example.org");

  blink::StorageKey key1(url::Origin::Create(origin1));
  blink::StorageKey key2(url::Origin::Create(origin2));
  blink::StorageKey key3(url::Origin::Create(origin3));

  std::vector<mojom::ServiceWorkerRegistrationDataPtr> registrations;
  std::vector<std::vector<ResourceRecordPtr>> resources_list;
  EXPECT_EQ(ServiceWorkerDatabase::Status::kOk,
            database->GetRegistrationsForStorageKey(key1, &registrations,
                                                    &resources_list));
  EXPECT_TRUE(registrations.empty());
  EXPECT_TRUE(resources_list.empty());

  ServiceWorkerDatabase::DeletedVersion deleted_version;

  RegistrationData data1;
  data1.registration_id = 100;
  data1.scope = URL(origin1, "/foo");
  data1.key = key1;
  data1.script = URL(origin1, "/script1.js");
  data1.version_id = 1000;
  data1.resources_total_size_bytes = 100;
  data1.script_response_time = base::Time::FromJsTime(0);
  data1.cross_origin_embedder_policy = CrossOriginEmbedderPolicyNone();
  data1.ancestor_frame_type = blink::mojom::AncestorFrameType::kNormalFrame;
  data1.policy_container_policies =
      blink::mojom::PolicyContainerPolicies::New();
  data1.policy_container_policies->cross_origin_embedder_policy =
      data1.cross_origin_embedder_policy.value;
  std::vector<ResourceRecordPtr> resources1;
  resources1.push_back(CreateResource(1, data1.script, 100));
  ASSERT_EQ(ServiceWorkerDatabase::Status::kOk,
            database->WriteRegistration(data1, resources1, &deleted_version));

  registrations.clear();
  resources_list.clear();
  EXPECT_EQ(ServiceWorkerDatabase::Status::kOk,
            database->GetRegistrationsForStorageKey(key1, &registrations,
                                                    &resources_list));
  EXPECT_EQ(1U, registrations.size());
  VerifyRegistrationData(data1, *registrations[0]);
  EXPECT_EQ(1U, resources_list.size());
  VerifyResourceRecords(resources1, resources_list[0]);

  RegistrationData data2;
  data2.registration_id = 200;
  data2.scope = URL(origin2, "/bar");
  data2.key = key2;
  data2.script = URL(origin2, "/script2.js");
  data2.version_id = 2000;
  data2.resources_total_size_bytes = 200;
  data2.script_response_time = base::Time::FromJsTime(42);
  data2.cross_origin_embedder_policy = CrossOriginEmbedderPolicyRequireCorp();
  data2.ancestor_frame_type = blink::mojom::AncestorFrameType::kFencedFrame;
  data2.policy_container_policies =
      blink::mojom::PolicyContainerPolicies::New();
  data2.policy_container_policies->cross_origin_embedder_policy =
      data2.cross_origin_embedder_policy.value;
  std::vector<ResourceRecordPtr> resources2;
  resources2.push_back(CreateResource(2, data2.script, 200));
  ASSERT_EQ(ServiceWorkerDatabase::Status::kOk,
            database->WriteRegistration(data2, resources2, &deleted_version));

  registrations.clear();
  resources_list.clear();
  EXPECT_EQ(ServiceWorkerDatabase::Status::kOk,
            database->GetRegistrationsForStorageKey(key2, &registrations,
                                                    &resources_list));
  EXPECT_EQ(1U, registrations.size());
  VerifyRegistrationData(data2, *registrations[0]);
  EXPECT_EQ(1U, resources_list.size());
  VerifyResourceRecords(resources2, resources_list[0]);

  RegistrationData data3;
  data3.registration_id = 300;
  data3.scope = URL(origin3, "/hoge");
  data3.key = key3;
  data3.script = URL(origin3, "/script3.js");
  data3.version_id = 3000;
  data3.resources_total_size_bytes = 300;
  data3.script_response_time = base::Time::FromJsTime(420);
  data3.cross_origin_embedder_policy = CrossOriginEmbedderPolicyNone();
  data3.policy_container_policies =
      blink::mojom::PolicyContainerPolicies::New();
  data3.policy_container_policies->cross_origin_embedder_policy =
      data3.cross_origin_embedder_policy.value;
  std::vector<ResourceRecordPtr> resources3;
  resources3.push_back(CreateResource(3, data3.script, 300));
  ASSERT_EQ(ServiceWorkerDatabase::Status::kOk,
            database->WriteRegistration(data3, resources3, &deleted_version));

  // |key3| has two registrations.
  RegistrationData data4;
  data4.registration_id = 400;
  data4.scope = URL(origin3, "/fuga");
  data4.key = key3;
  data4.script = URL(origin3, "/script4.js");
  data4.version_id = 4000;
  data4.resources_total_size_bytes = 400;
  data4.script_response_time = base::Time::FromJsTime(4200);
  data4.cross_origin_embedder_policy =
      CrossOriginEmbedderPolicyCredentialless();
  data4.policy_container_policies =
      blink::mojom::PolicyContainerPolicies::New();
  data4.policy_container_policies->cross_origin_embedder_policy =
      data4.cross_origin_embedder_policy.value;
  std::vector<ResourceRecordPtr> resources4;
  resources4.push_back(CreateResource(4, data4.script, 400));
  ASSERT_EQ(ServiceWorkerDatabase::Status::kOk,
            database->WriteRegistration(data4, resources4, &deleted_version));

  registrations.clear();
  resources_list.clear();
  EXPECT_EQ(ServiceWorkerDatabase::Status::kOk,
            database->GetRegistrationsForStorageKey(key3, &registrations,
                                                    &resources_list));
  EXPECT_EQ(2U, registrations.size());
  VerifyRegistrationData(data3, *registrations[0]);
  VerifyRegistrationData(data4, *registrations[1]);
  EXPECT_EQ(2U, resources_list.size());
  VerifyResourceRecords(resources3, resources_list[0]);
  VerifyResourceRecords(resources4, resources_list[1]);

  // The third parameter |opt_resources_list| to GetRegistrationsForStorageKey()
  // is optional. So, nullptr should be acceptable.
  registrations.clear();
  EXPECT_EQ(
      ServiceWorkerDatabase::Status::kOk,
      database->GetRegistrationsForStorageKey(key1, &registrations, nullptr));
  EXPECT_EQ(1U, registrations.size());
  VerifyRegistrationData(data1, *registrations[0]);
}

TEST(ServiceWorkerDatabaseTest, GetAllRegistrations) {
  std::unique_ptr<ServiceWorkerDatabase> database(CreateDatabaseInMemory());

  std::vector<mojom::ServiceWorkerRegistrationDataPtr> registrations;
  EXPECT_EQ(ServiceWorkerDatabase::Status::kOk,
            database->GetAllRegistrations(&registrations));
  EXPECT_TRUE(registrations.empty());

  ServiceWorkerDatabase::DeletedVersion deleted_version;

  GURL origin1("https://www1.example.com");
  RegistrationData data1;
  data1.registration_id = 100;
  data1.scope = URL(origin1, "/foo");
  data1.key = blink::StorageKey(url::Origin::Create(data1.scope));
  data1.script = URL(origin1, "/script1.js");
  data1.version_id = 1000;
  data1.resources_total_size_bytes = 100;
  data1.cross_origin_embedder_policy = CrossOriginEmbedderPolicyNone();
  data1.ancestor_frame_type = blink::mojom::AncestorFrameType::kNormalFrame;
  data1.policy_container_policies =
      blink::mojom::PolicyContainerPolicies::New();
  data1.policy_container_policies->cross_origin_embedder_policy =
      data1.cross_origin_embedder_policy.value;
  std::vector<ResourceRecordPtr> resources1;
  resources1.push_back(CreateResource(1, data1.script, 100));
  ASSERT_EQ(ServiceWorkerDatabase::Status::kOk,
            database->WriteRegistration(data1, resources1, &deleted_version));

  GURL origin2("https://www2.example.com");
  RegistrationData data2;
  data2.registration_id = 200;
  data2.scope = URL(origin2, "/bar");
  data2.key = blink::StorageKey(url::Origin::Create(data2.scope));
  data2.script = URL(origin2, "/script2.js");
  data2.version_id = 2000;
  data2.resources_total_size_bytes = 200;
  data2.update_via_cache = blink::mojom::ServiceWorkerUpdateViaCache::kNone;
  data2.cross_origin_embedder_policy = CrossOriginEmbedderPolicyRequireCorp();
  data2.ancestor_frame_type = blink::mojom::AncestorFrameType::kFencedFrame;
  data2.policy_container_policies =
      blink::mojom::PolicyContainerPolicies::New();
  data2.policy_container_policies->cross_origin_embedder_policy =
      data2.cross_origin_embedder_policy.value;
  std::vector<ResourceRecordPtr> resources2;
  resources2.push_back(CreateResource(2, data2.script, 200));
  ASSERT_EQ(ServiceWorkerDatabase::Status::kOk,
            database->WriteRegistration(data2, resources2, &deleted_version));

  GURL origin3("https://www3.example.com");
  RegistrationData data3;
  data3.registration_id = 300;
  data3.scope = URL(origin3, "/hoge");
  data3.key = blink::StorageKey(url::Origin::Create(data3.scope));
  data3.script = URL(origin3, "/script3.js");
  data3.version_id = 3000;
  data3.resources_total_size_bytes = 300;
  data3.cross_origin_embedder_policy =
      CrossOriginEmbedderPolicyCredentialless();
  data3.policy_container_policies =
      blink::mojom::PolicyContainerPolicies::New();
  data3.policy_container_policies->cross_origin_embedder_policy =
      data3.cross_origin_embedder_policy.value;
  std::vector<ResourceRecordPtr> resources3;
  resources3.push_back(CreateResource(3, data3.script, 300));
  ASSERT_EQ(ServiceWorkerDatabase::Status::kOk,
            database->WriteRegistration(data3, resources3, &deleted_version));

  // |origin3| has two registrations.
  RegistrationData data4;
  data4.registration_id = 400;
  data4.scope = URL(origin3, "/fuga");
  data4.key = blink::StorageKey(url::Origin::Create(data4.scope));
  data4.script = URL(origin3, "/script4.js");
  data4.version_id = 4000;
  data4.resources_total_size_bytes = 400;
  std::vector<ResourceRecordPtr> resources4;
  resources4.push_back(CreateResource(4, data4.script, 400));
  ASSERT_EQ(ServiceWorkerDatabase::Status::kOk,
            database->WriteRegistration(data4, resources4, &deleted_version));

  // Add partitioned keys.
  // Make sure kThirdPartyStoragePartitioning is enabled so the keys are
  // inserted as partitioned.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      net::features::kThirdPartyStoragePartitioning);

  GURL origin5("https://www5.example.com");
  net::SchemefulSite top_level_site1(GURL("https://toplevel.com"));
  RegistrationData data5;
  data5.registration_id = 500;
  data5.scope = URL(origin5, "/hoge");
  data5.key = blink::StorageKey::CreateForTesting(
      url::Origin::Create(data5.scope), top_level_site1);
  data5.script = URL(origin5, "/script5.js");
  data5.version_id = 5000;
  data5.resources_total_size_bytes = 500;
  data5.cross_origin_embedder_policy =
      CrossOriginEmbedderPolicyCredentialless();
  std::vector<ResourceRecordPtr> resources5;
  data5.policy_container_policies =
      blink::mojom::PolicyContainerPolicies::New();
  data5.policy_container_policies->cross_origin_embedder_policy =
      data5.cross_origin_embedder_policy.value;
  resources5.push_back(CreateResource(5, data5.script, 500));
  ASSERT_EQ(ServiceWorkerDatabase::Status::kOk,
            database->WriteRegistration(data5, resources5, &deleted_version));

  GURL origin6("https://www6.example.com");
  net::SchemefulSite top_level_site2(GURL("https://toplevel2.com"));
  RegistrationData data6;
  data6.registration_id = 600;
  data6.scope = URL(origin6, "/hoge");
  data6.key = blink::StorageKey::CreateForTesting(
      url::Origin::Create(data6.scope), top_level_site2);
  data6.script = URL(origin6, "/script6.js");
  data6.version_id = 6000;
  data6.resources_total_size_bytes = 600;
  data6.cross_origin_embedder_policy =
      CrossOriginEmbedderPolicyCredentialless();
  data6.policy_container_policies =
      blink::mojom::PolicyContainerPolicies::New();
  data6.policy_container_policies->cross_origin_embedder_policy =
      data6.cross_origin_embedder_policy.value;
  std::vector<ResourceRecordPtr> resources6;
  resources6.push_back(CreateResource(6, data6.script, 600));
  ASSERT_EQ(ServiceWorkerDatabase::Status::kOk,
            database->WriteRegistration(data6, resources6, &deleted_version));

  // Disable partitioning to ensure the partitioned keys are not found.
  scoped_feature_list.Reset();

  // Keys with nonces should always be gettable.
  GURL origin7 = GURL("https://www7.example.com");
  base::UnguessableToken token = base::UnguessableToken::Create();
  RegistrationData data7;
  data7.registration_id = 700;
  data7.scope = URL(origin7, "/hoge");
  data7.key = blink::StorageKey::CreateWithNonce(
      url::Origin::Create(data7.scope), token);
  data7.script = URL(origin7, "/script7.js");
  data7.version_id = 7000;
  data7.resources_total_size_bytes = 700;
  data7.cross_origin_embedder_policy =
      CrossOriginEmbedderPolicyCredentialless();
  data7.policy_container_policies =
      blink::mojom::PolicyContainerPolicies::New();
  data7.policy_container_policies->cross_origin_embedder_policy =
      data7.cross_origin_embedder_policy.value;
  std::vector<ResourceRecordPtr> resources7;
  resources7.push_back(CreateResource(7, data7.script, 700));
  ASSERT_EQ(ServiceWorkerDatabase::Status::kOk,
            database->WriteRegistration(data7, resources7, &deleted_version));

  registrations.clear();
  EXPECT_EQ(ServiceWorkerDatabase::Status::kOk,
            database->GetAllRegistrations(&registrations));
  EXPECT_EQ(5U, registrations.size());

  VerifyRegistrationData(data1, *registrations[0]);
  VerifyRegistrationData(data2, *registrations[1]);
  VerifyRegistrationData(data3, *registrations[2]);
  VerifyRegistrationData(data4, *registrations[3]);
  VerifyRegistrationData(data7, *registrations[4]);

  // Re-enable partitioning and check for the partitioned keys.
  scoped_feature_list.InitAndEnableFeature(
      net::features::kThirdPartyStoragePartitioning);

  registrations.clear();
  EXPECT_EQ(ServiceWorkerDatabase::Status::kOk,
            database->GetAllRegistrations(&registrations));
  EXPECT_EQ(7U, registrations.size());

  VerifyRegistrationData(data5, *registrations[4]);
  VerifyRegistrationData(data6, *registrations[5]);
}

TEST(ServiceWorkerDatabaseTest, Registration_Basic) {
  std::unique_ptr<ServiceWorkerDatabase> database(CreateDatabaseInMemory());

  GURL origin("https://example.com");
  blink::StorageKey key(url::Origin::Create(origin));
  RegistrationData data;
  data.registration_id = 100;
  data.scope = URL(origin, "/foo");
  data.key = key;
  data.script = URL(origin, "/resource1");
  data.version_id = 200;
  data.resources_total_size_bytes = 10939 + 200;
  data.used_features = {blink::mojom::WebFeature::kNavigatorVendor,
                        blink::mojom::WebFeature::kLinkRelPreload,
                        blink::mojom::WebFeature::kCSSFilterInvert};

  std::vector<ResourceRecordPtr> resources;
  resources.push_back(CreateResource(1, URL(origin, "/resource1"), 10939));
  resources.push_back(CreateResource(2, URL(origin, "/resource2"), 200));

  // Write a resource to the uncommitted list to make sure that writing
  // registration removes resource ids associated with the registration from
  // the uncommitted list.
  std::vector<int64_t> uncommitted_ids;
  uncommitted_ids.push_back(resources[0]->resource_id);
  EXPECT_EQ(ServiceWorkerDatabase::Status::kOk,
            database->WriteUncommittedResourceIds(uncommitted_ids));
  std::vector<int64_t> uncommitted_ids_out;
  EXPECT_EQ(ServiceWorkerDatabase::Status::kOk,
            database->GetUncommittedResourceIds(&uncommitted_ids_out));
  EXPECT_EQ(uncommitted_ids, uncommitted_ids_out);

  ServiceWorkerDatabase::DeletedVersion deleted_version;
  deleted_version.version_id = 222;  // Dummy initial value

  EXPECT_EQ(ServiceWorkerDatabase::Status::kOk,
            database->WriteRegistration(data, resources, &deleted_version));
  EXPECT_EQ(blink::mojom::kInvalidServiceWorkerVersionId,
            deleted_version.version_id);
  EXPECT_TRUE(deleted_version.newly_purgeable_resources.empty());

  // Make sure that the registration and resource records are stored.
  RegistrationDataPtr data_out;
  std::vector<ResourceRecordPtr> resources_out;
  EXPECT_EQ(ServiceWorkerDatabase::Status::kOk,
            database->ReadRegistration(data.registration_id, key, &data_out,
                                       &resources_out));
  VerifyRegistrationData(data, *data_out);
  VerifyResourceRecords(resources, resources_out);
  blink::StorageKey key_out;
  EXPECT_EQ(
      ServiceWorkerDatabase::Status::kOk,
      database->ReadRegistrationStorageKey(data.registration_id, &key_out));
  EXPECT_EQ(key, key_out);

  // Make sure that the resource is removed from the uncommitted list.
  uncommitted_ids_out.clear();
  EXPECT_EQ(ServiceWorkerDatabase::Status::kOk,
            database->GetUncommittedResourceIds(&uncommitted_ids_out));
  EXPECT_TRUE(uncommitted_ids_out.empty());

  EXPECT_EQ(ServiceWorkerDatabase::Status::kOk,
            database->DeleteRegistration(data.registration_id, key,
                                         &deleted_version));
  EXPECT_EQ(data.version_id, deleted_version.version_id);
  ASSERT_EQ(resources.size(), deleted_version.newly_purgeable_resources.size());
  for (size_t i = 0; i < resources.size(); ++i)
    EXPECT_EQ(deleted_version.newly_purgeable_resources[i],
              resources[i]->resource_id);

  // Make sure that the registration and resource records are gone.
  resources_out.clear();
  EXPECT_EQ(ServiceWorkerDatabase::Status::kErrorNotFound,
            database->ReadRegistration(data.registration_id, key, &data_out,
                                       &resources_out));
  EXPECT_TRUE(resources_out.empty());
  EXPECT_EQ(
      ServiceWorkerDatabase::Status::kErrorNotFound,
      database->ReadRegistrationStorageKey(data.registration_id, &key_out));

  // Resources should be purgeable because these are no longer referred.
  std::vector<int64_t> purgeable_ids_out;
  EXPECT_EQ(ServiceWorkerDatabase::Status::kOk,
            database->GetPurgeableResourceIds(&purgeable_ids_out));
  EXPECT_EQ(2u, purgeable_ids_out.size());
  EXPECT_TRUE(base::Contains(purgeable_ids_out, resources[0]->resource_id));
  EXPECT_TRUE(base::Contains(purgeable_ids_out, resources[1]->resource_id));
}

TEST(ServiceWorkerDatabaseTest, DeleteNonExistentRegistration) {
  std::unique_ptr<ServiceWorkerDatabase> database(CreateDatabaseInMemory());

  GURL origin("https://example.com");
  RegistrationData data;
  data.registration_id = 100;
  data.scope = URL(origin, "/foo");
  data.key = blink::StorageKey(url::Origin::Create(data.scope));
  data.script = URL(origin, "/resource1");
  data.version_id = 200;
  data.resources_total_size_bytes = 19 + 29129;

  std::vector<ResourceRecordPtr> resources;
  resources.push_back(CreateResource(1, URL(origin, "/resource1"), 19));
  resources.push_back(CreateResource(2, URL(origin, "/resource2"), 29129));

  const int64_t kNonExistentRegistrationId = 999;
  const int64_t kArbitraryVersionId = 222;  // Used as a dummy initial value

  ServiceWorkerDatabase::DeletedVersion deleted_version;
  deleted_version.version_id = kArbitraryVersionId;
  EXPECT_EQ(ServiceWorkerDatabase::Status::kOk,
            database->WriteRegistration(data, resources, &deleted_version));
  EXPECT_EQ(blink::mojom::kInvalidServiceWorkerVersionId,
            deleted_version.version_id);
  EXPECT_TRUE(deleted_version.newly_purgeable_resources.empty());

  // Delete from an origin that has a registration.
  deleted_version.version_id = kArbitraryVersionId;
  deleted_version.newly_purgeable_resources.clear();
  EXPECT_EQ(
      ServiceWorkerDatabase::Status::kOk,
      database->DeleteRegistration(
          kNonExistentRegistrationId,
          blink::StorageKey(url::Origin::Create(origin)), &deleted_version));
  EXPECT_EQ(blink::mojom::kInvalidServiceWorkerVersionId,
            deleted_version.version_id);
  EXPECT_TRUE(deleted_version.newly_purgeable_resources.empty());

  // Delete from an origin that has no registration.
  deleted_version.version_id = kArbitraryVersionId;
  deleted_version.newly_purgeable_resources.clear();
  EXPECT_EQ(
      ServiceWorkerDatabase::Status::kOk,
      database->DeleteRegistration(
          kNonExistentRegistrationId,
          blink::StorageKey::CreateFromStringForTesting("https://example.net"),
          &deleted_version));
  EXPECT_EQ(blink::mojom::kInvalidServiceWorkerVersionId,
            deleted_version.version_id);
  EXPECT_TRUE(deleted_version.newly_purgeable_resources.empty());
}

TEST(ServiceWorkerDatabaseTest, Registration_Overwrite) {
  std::unique_ptr<ServiceWorkerDatabase> database(CreateDatabaseInMemory());

  GURL origin("https://example.com");
  blink::StorageKey key(url::Origin::Create(origin));
  RegistrationData data;
  data.registration_id = 100;
  data.scope = URL(origin, "/foo");
  data.key = key;
  data.script = URL(origin, "/resource1");
  data.version_id = 200;
  data.resources_total_size_bytes = 10 + 11;
  data.used_features = {blink::mojom::WebFeature::kNavigatorVendor,
                        blink::mojom::WebFeature::kLinkRelPreload,
                        blink::mojom::WebFeature::kCSSFilterInvert};

  std::vector<ResourceRecordPtr> resources1;
  resources1.push_back(CreateResource(1, URL(origin, "/resource1"), 10));
  resources1.push_back(CreateResource(2, URL(origin, "/resource2"), 11));

  ServiceWorkerDatabase::DeletedVersion deleted_version;
  deleted_version.version_id = 222;  // Dummy initial value

  EXPECT_EQ(ServiceWorkerDatabase::Status::kOk,
            database->WriteRegistration(data, resources1, &deleted_version));
  EXPECT_EQ(blink::mojom::kInvalidServiceWorkerVersionId,
            deleted_version.version_id);
  EXPECT_TRUE(deleted_version.newly_purgeable_resources.empty());

  // Make sure that the registration and resource records are stored.
  RegistrationDataPtr data_out;
  std::vector<ResourceRecordPtr> resources_out;
  EXPECT_EQ(ServiceWorkerDatabase::Status::kOk,
            database->ReadRegistration(data.registration_id, key, &data_out,
                                       &resources_out));
  VerifyRegistrationData(data, *data_out);
  VerifyResourceRecords(resources1, resources_out);

  // Update the registration.
  mojom::ServiceWorkerRegistrationDataPtr updated_data = data.Clone();
  updated_data->script = URL(origin, "/resource3");
  updated_data->version_id = data.version_id + 1;
  updated_data->resources_total_size_bytes = 12 + 13;
  updated_data->used_features = {
      blink::mojom::WebFeature::kFormElement,
      blink::mojom::WebFeature::kDocumentExitPointerLock,
      blink::mojom::WebFeature::kAdClick};
  updated_data->script_type = blink::mojom::ScriptType::kModule;
  updated_data->update_via_cache =
      blink::mojom::ServiceWorkerUpdateViaCache::kAll;
  std::vector<ResourceRecordPtr> resources2;
  resources2.push_back(CreateResource(3, URL(origin, "/resource3"), 12));
  resources2.push_back(CreateResource(4, URL(origin, "/resource4"), 13));

  EXPECT_EQ(
      ServiceWorkerDatabase::Status::kOk,
      database->WriteRegistration(*updated_data, resources2, &deleted_version));
  EXPECT_EQ(data.version_id, deleted_version.version_id);
  ASSERT_EQ(resources1.size(),
            deleted_version.newly_purgeable_resources.size());
  for (size_t i = 0; i < resources1.size(); ++i)
    EXPECT_EQ(deleted_version.newly_purgeable_resources[i],
              resources1[i]->resource_id);

  // Make sure that |updated_data| is stored and resources referred from |data|
  // is moved to the purgeable list.
  resources_out.clear();
  EXPECT_EQ(ServiceWorkerDatabase::Status::kOk,
            database->ReadRegistration(updated_data->registration_id, key,
                                       &data_out, &resources_out));
  VerifyRegistrationData(*updated_data, *data_out);
  VerifyResourceRecords(resources2, resources_out);

  std::vector<int64_t> purgeable_ids_out;
  EXPECT_EQ(ServiceWorkerDatabase::Status::kOk,
            database->GetPurgeableResourceIds(&purgeable_ids_out));
  EXPECT_EQ(2u, purgeable_ids_out.size());
  EXPECT_TRUE(base::Contains(purgeable_ids_out, resources1[0]->resource_id));
  EXPECT_TRUE(base::Contains(purgeable_ids_out, resources1[1]->resource_id));
}

TEST(ServiceWorkerDatabaseTest, Registration_Multiple) {
  std::unique_ptr<ServiceWorkerDatabase> database(CreateDatabaseInMemory());
  GURL origin("https://example.com");
  blink::StorageKey key(url::Origin::Create(origin));

  ServiceWorkerDatabase::DeletedVersion deleted_version;

  // Add registration1.
  RegistrationData data1;
  data1.registration_id = 100;
  data1.scope = URL(origin, "/foo");
  data1.key = key;
  data1.script = URL(origin, "/resource1");
  data1.version_id = 200;
  data1.resources_total_size_bytes = 1451 + 15234;

  std::vector<ResourceRecordPtr> resources1;
  resources1.push_back(CreateResource(1, URL(origin, "/resource1"), 1451));
  resources1.push_back(CreateResource(2, URL(origin, "/resource2"), 15234));
  EXPECT_EQ(ServiceWorkerDatabase::Status::kOk,
            database->WriteRegistration(data1, resources1, &deleted_version));

  // Add registration2.
  RegistrationData data2;
  data2.registration_id = 101;
  data2.scope = URL(origin, "/bar");
  data2.key = key;
  data2.script = URL(origin, "/resource3");
  data2.version_id = 201;
  data2.resources_total_size_bytes = 5 + 6;

  std::vector<ResourceRecordPtr> resources2;
  resources2.push_back(CreateResource(3, URL(origin, "/resource3"), 5));
  resources2.push_back(CreateResource(4, URL(origin, "/resource4"), 6));
  EXPECT_EQ(ServiceWorkerDatabase::Status::kOk,
            database->WriteRegistration(data2, resources2, &deleted_version));

  // Make sure that registration1 is stored.
  RegistrationDataPtr data_out;
  std::vector<ResourceRecordPtr> resources_out;
  EXPECT_EQ(ServiceWorkerDatabase::Status::kOk,
            database->ReadRegistration(data1.registration_id, key, &data_out,
                                       &resources_out));
  VerifyRegistrationData(data1, *data_out);
  VerifyResourceRecords(resources1, resources_out);
  blink::StorageKey key_out;
  EXPECT_EQ(
      ServiceWorkerDatabase::Status::kOk,
      database->ReadRegistrationStorageKey(data1.registration_id, &key_out));
  EXPECT_EQ(key, key_out);

  // Make sure that registration2 is also stored.
  resources_out.clear();
  EXPECT_EQ(ServiceWorkerDatabase::Status::kOk,
            database->ReadRegistration(data2.registration_id, key, &data_out,
                                       &resources_out));
  VerifyRegistrationData(data2, *data_out);
  VerifyResourceRecords(resources2, resources_out);
  EXPECT_EQ(
      ServiceWorkerDatabase::Status::kOk,
      database->ReadRegistrationStorageKey(data2.registration_id, &key_out));
  EXPECT_EQ(key, key_out);

  std::vector<int64_t> purgeable_ids_out;
  EXPECT_EQ(ServiceWorkerDatabase::Status::kOk,
            database->GetPurgeableResourceIds(&purgeable_ids_out));
  EXPECT_TRUE(purgeable_ids_out.empty());

  // Delete registration1.
  EXPECT_EQ(ServiceWorkerDatabase::Status::kOk,
            database->DeleteRegistration(data1.registration_id, key,
                                         &deleted_version));
  EXPECT_EQ(data1.registration_id, deleted_version.registration_id);

  // Make sure that registration1 is gone.
  resources_out.clear();
  EXPECT_EQ(ServiceWorkerDatabase::Status::kErrorNotFound,
            database->ReadRegistration(data1.registration_id, key, &data_out,
                                       &resources_out));
  EXPECT_TRUE(resources_out.empty());
  EXPECT_EQ(
      ServiceWorkerDatabase::Status::kErrorNotFound,
      database->ReadRegistrationStorageKey(data1.registration_id, &key_out));

  purgeable_ids_out.clear();
  EXPECT_EQ(ServiceWorkerDatabase::Status::kOk,
            database->GetPurgeableResourceIds(&purgeable_ids_out));
  EXPECT_EQ(2u, purgeable_ids_out.size());
  EXPECT_TRUE(base::Contains(purgeable_ids_out, resources1[0]->resource_id));
  EXPECT_TRUE(base::Contains(purgeable_ids_out, resources1[1]->resource_id));

  // Make sure that registration2 is still alive.
  resources_out.clear();
  EXPECT_EQ(ServiceWorkerDatabase::Status::kOk,
            database->ReadRegistration(data2.registration_id, key, &data_out,
                                       &resources_out));
  VerifyRegistrationData(data2, *data_out);
  VerifyResourceRecords(resources2, resources_out);
  EXPECT_EQ(
      ServiceWorkerDatabase::Status::kOk,
      database->ReadRegistrationStorageKey(data2.registration_id, &key_out));
  EXPECT_EQ(key, key_out);
}

TEST(ServiceWorkerDatabaseTest, Registration_UninitializedDatabase) {
  std::unique_ptr<ServiceWorkerDatabase> database(CreateDatabaseInMemory());
  const GURL origin("https://example.com");
  blink::StorageKey key(url::Origin::Create(origin));

  // Should be failed because the database does not exist.
  RegistrationDataPtr data_out;
  std::vector<ResourceRecordPtr> resources_out;
  EXPECT_EQ(ServiceWorkerDatabase::Status::kErrorNotFound,
            database->ReadRegistration(100, key, &data_out, &resources_out));
  EXPECT_TRUE(data_out.is_null());
  EXPECT_TRUE(resources_out.empty());
  blink::StorageKey key_out;
  EXPECT_EQ(ServiceWorkerDatabase::Status::kErrorNotFound,
            database->ReadRegistrationStorageKey(100, &key_out));

  // Deleting non-existent registration should succeed.
  ServiceWorkerDatabase::DeletedVersion deleted_version;
  EXPECT_EQ(ServiceWorkerDatabase::Status::kOk,
            database->DeleteRegistration(100, key, &deleted_version));
  EXPECT_EQ(blink::mojom::kInvalidServiceWorkerVersionId,
            deleted_version.version_id);
  EXPECT_TRUE(deleted_version.newly_purgeable_resources.empty());

  // Actually create a new database, but not initialized yet.
  database->LazyOpen(true);

  // Should be failed because the database is not initialized.
  ASSERT_EQ(ServiceWorkerDatabase::DATABASE_STATE_UNINITIALIZED,
            database->state_);
  EXPECT_EQ(ServiceWorkerDatabase::Status::kErrorNotFound,
            database->ReadRegistration(100, key, &data_out, &resources_out));
  EXPECT_TRUE(data_out.is_null());
  EXPECT_TRUE(resources_out.empty());
  EXPECT_EQ(ServiceWorkerDatabase::Status::kErrorNotFound,
            database->ReadRegistrationStorageKey(100, &key_out));

  // Deleting non-existent registration should succeed.
  EXPECT_EQ(ServiceWorkerDatabase::Status::kOk,
            database->DeleteRegistration(100, key, &deleted_version));
  EXPECT_EQ(blink::mojom::kInvalidServiceWorkerVersionId,
            deleted_version.version_id);
  EXPECT_TRUE(deleted_version.newly_purgeable_resources.empty());
}

TEST(ServiceWorkerDatabaseTest, Registration_ScriptType) {
  std::unique_ptr<ServiceWorkerDatabase> database(CreateDatabaseInMemory());

  ServiceWorkerDatabase::DeletedVersion deleted_version;

  // Default script type.
  GURL origin1("https://www1.example.com");
  RegistrationData data1;
  data1.registration_id = 100;
  data1.scope = URL(origin1, "/foo");
  data1.key = blink::StorageKey(url::Origin::Create(data1.scope));
  data1.script = URL(origin1, "/resource1");
  data1.version_id = 100;
  data1.resources_total_size_bytes = 10 + 10000;
  EXPECT_EQ(blink::mojom::ScriptType::kClassic, data1.script_type);
  std::vector<ResourceRecordPtr> resources1;
  resources1.push_back(CreateResource(1, URL(origin1, "/resource1"), 10));
  resources1.push_back(CreateResource(2, URL(origin1, "/resource2"), 10000));
  EXPECT_EQ(ServiceWorkerDatabase::Status::kOk,
            database->WriteRegistration(data1, resources1, &deleted_version));

  // Classic script type.
  GURL origin2("https://www2.example.com");
  RegistrationData data2;
  data2.registration_id = 200;
  data2.scope = URL(origin2, "/bar");
  data2.key = blink::StorageKey(url::Origin::Create(data2.scope));
  data2.script = URL(origin2, "/resource3");
  data2.version_id = 200;
  data2.resources_total_size_bytes = 20 + 20000;
  data2.script_type = blink::mojom::ScriptType::kClassic;
  std::vector<ResourceRecordPtr> resources2;
  resources2.push_back(CreateResource(3, URL(origin2, "/resource3"), 20));
  resources2.push_back(CreateResource(4, URL(origin2, "/resource4"), 20000));
  EXPECT_EQ(ServiceWorkerDatabase::Status::kOk,
            database->WriteRegistration(data2, resources2, &deleted_version));

  // Module script type.
  GURL origin3("https://www3.example.com");
  RegistrationData data3;
  data3.registration_id = 300;
  data3.scope = URL(origin3, "/baz");
  data3.key = blink::StorageKey(url::Origin::Create(data3.scope));
  data3.script = URL(origin3, "/resource5");
  data3.version_id = 300;
  data3.resources_total_size_bytes = 30 + 30000;
  data3.script_type = blink::mojom::ScriptType::kModule;
  std::vector<ResourceRecordPtr> resources3;
  resources3.push_back(CreateResource(5, URL(origin3, "/resource5"), 30));
  resources3.push_back(CreateResource(6, URL(origin3, "/resource6"), 30000));
  EXPECT_EQ(ServiceWorkerDatabase::Status::kOk,
            database->WriteRegistration(data3, resources3, &deleted_version));

  RegistrationDataPtr data;
  std::vector<ResourceRecordPtr> resources;
  EXPECT_EQ(
      ServiceWorkerDatabase::Status::kOk,
      database->ReadRegistration(
          data1.registration_id,
          blink::StorageKey(url::Origin::Create(origin1)), &data, &resources));
  VerifyRegistrationData(data1, *data);
  VerifyResourceRecords(resources1, resources);
  EXPECT_EQ(2U, resources.size());
  resources.clear();

  EXPECT_EQ(
      ServiceWorkerDatabase::Status::kOk,
      database->ReadRegistration(
          data2.registration_id,
          blink::StorageKey(url::Origin::Create(origin2)), &data, &resources));
  VerifyRegistrationData(data2, *data);
  VerifyResourceRecords(resources2, resources);
  EXPECT_EQ(2U, resources.size());
  resources.clear();

  EXPECT_EQ(
      ServiceWorkerDatabase::Status::kOk,
      database->ReadRegistration(
          data3.registration_id,
          blink::StorageKey(url::Origin::Create(origin3)), &data, &resources));
  VerifyRegistrationData(data3, *data);
  VerifyResourceRecords(resources3, resources);
  EXPECT_EQ(2U, resources.size());
  resources.clear();
}

TEST(ServiceWorkerDatabaseTest, UserData_Basic) {
  std::unique_ptr<ServiceWorkerDatabase> database(CreateDatabaseInMemory());
  const url::Origin kOrigin = url::Origin::Create(GURL("https://example.com"));
  blink::StorageKey key(kOrigin);

  // Add a registration.
  RegistrationData data;
  data.registration_id = 100;
  data.scope = URL(kOrigin.GetURL(), "/foo");
  data.key = key;
  data.script = URL(kOrigin.GetURL(), "/script.js");
  data.version_id = 200;
  data.resources_total_size_bytes = 100;
  std::vector<ResourceRecordPtr> resources;
  resources.push_back(CreateResource(1, data.script, 100));
  ServiceWorkerDatabase::DeletedVersion deleted_version;
  ASSERT_EQ(ServiceWorkerDatabase::Status::kOk,
            database->WriteRegistration(data, resources, &deleted_version));

  // Write user data associated with the stored registration.
  std::vector<std::string> user_data_out;
  EXPECT_EQ(ServiceWorkerDatabase::Status::kOk,
            database->WriteUserData(
                data.registration_id, key,
                CreateUserData(data.registration_id, {{"key1", "data"}})));
  EXPECT_EQ(
      ServiceWorkerDatabase::Status::kOk,
      database->ReadUserData(data.registration_id, {"key1"}, &user_data_out));
  ASSERT_EQ(1u, user_data_out.size());
  EXPECT_EQ("data", user_data_out[0]);

  // Writing user data not associated with the stored registration should be
  // failed.
  EXPECT_EQ(
      ServiceWorkerDatabase::Status::kErrorNotFound,
      database->WriteUserData(
          300, key, CreateUserData(data.registration_id, {{"key1", "data"}})));

  // Write empty user data for a different key.
  EXPECT_EQ(ServiceWorkerDatabase::Status::kOk,
            database->WriteUserData(data.registration_id, key,
                                    CreateUserData(data.registration_id,
                                                   {{"key2", std::string()}})));
  EXPECT_EQ(
      ServiceWorkerDatabase::Status::kOk,
      database->ReadUserData(data.registration_id, {"key2"}, &user_data_out));
  ASSERT_EQ(1u, user_data_out.size());
  EXPECT_EQ(std::string(), user_data_out[0]);
  EXPECT_EQ(
      ServiceWorkerDatabase::Status::kOk,
      database->ReadUserData(data.registration_id, {"key1"}, &user_data_out));
  ASSERT_EQ(1u, user_data_out.size());
  EXPECT_EQ("data", user_data_out[0]);

  // Overwrite the existing user data.
  EXPECT_EQ(ServiceWorkerDatabase::Status::kOk,
            database->WriteUserData(
                data.registration_id, key,
                CreateUserData(data.registration_id, {{"key1", "overwrite"}})));
  EXPECT_EQ(
      ServiceWorkerDatabase::Status::kOk,
      database->ReadUserData(data.registration_id, {"key1"}, &user_data_out));
  ASSERT_EQ(1u, user_data_out.size());
  EXPECT_EQ("overwrite", user_data_out[0]);

  // Delete the user data.
  EXPECT_EQ(ServiceWorkerDatabase::Status::kOk,
            database->DeleteUserData(data.registration_id, {"key1"}));
  EXPECT_EQ(
      ServiceWorkerDatabase::Status::kErrorNotFound,
      database->ReadUserData(data.registration_id, {"key1"}, &user_data_out));
  EXPECT_TRUE(user_data_out.empty());
  EXPECT_EQ(
      ServiceWorkerDatabase::Status::kOk,
      database->ReadUserData(data.registration_id, {"key2"}, &user_data_out));
  ASSERT_EQ(1u, user_data_out.size());
  EXPECT_EQ(std::string(), user_data_out[0]);

  // Write/overwrite multiple user data keys.
  EXPECT_EQ(ServiceWorkerDatabase::Status::kOk,
            database->WriteUserData(
                data.registration_id, key,
                CreateUserData(data.registration_id, {{"key2", "overwrite2"},
                                                      {"key3", "data3"},
                                                      {"key4", "data4"}})));
  EXPECT_EQ(
      ServiceWorkerDatabase::Status::kErrorNotFound,
      database->ReadUserData(data.registration_id, {"key1"}, &user_data_out));
  EXPECT_EQ(ServiceWorkerDatabase::Status::kOk,
            database->ReadUserData(data.registration_id,
                                   {"key2", "key3", "key4"}, &user_data_out));
  ASSERT_EQ(3u, user_data_out.size());
  EXPECT_EQ("overwrite2", user_data_out[0]);
  EXPECT_EQ("data3", user_data_out[1]);
  EXPECT_EQ("data4", user_data_out[2]);
  // Multiple reads fail if one is not found.
  EXPECT_EQ(ServiceWorkerDatabase::Status::kErrorNotFound,
            database->ReadUserData(data.registration_id, {"key2", "key1"},
                                   &user_data_out));
  EXPECT_TRUE(user_data_out.empty());

  // Delete multiple user data keys, even if some are not found.
  EXPECT_EQ(
      ServiceWorkerDatabase::Status::kOk,
      database->DeleteUserData(data.registration_id, {"key1", "key2", "key3"}));
  EXPECT_EQ(
      ServiceWorkerDatabase::Status::kErrorNotFound,
      database->ReadUserData(data.registration_id, {"key1"}, &user_data_out));
  EXPECT_EQ(
      ServiceWorkerDatabase::Status::kErrorNotFound,
      database->ReadUserData(data.registration_id, {"key2"}, &user_data_out));
  EXPECT_EQ(
      ServiceWorkerDatabase::Status::kErrorNotFound,
      database->ReadUserData(data.registration_id, {"key3"}, &user_data_out));
  EXPECT_EQ(
      ServiceWorkerDatabase::Status::kOk,
      database->ReadUserData(data.registration_id, {"key4"}, &user_data_out));
  ASSERT_EQ(1u, user_data_out.size());
  EXPECT_EQ("data4", user_data_out[0]);
}

TEST(ServiceWorkerDatabaseTest,
     UserData_ReadUserDataForAllRegistrationsByKeyPrefix) {
  std::unique_ptr<ServiceWorkerDatabase> database(CreateDatabaseInMemory());
  const url::Origin kOrigin = url::Origin::Create(GURL("https://example.com"));
  const blink::StorageKey kKey(kOrigin);

  // Add registration 1.
  RegistrationData data1;
  data1.registration_id = 100;
  data1.scope = URL(kOrigin.GetURL(), "/foo");
  data1.key = kKey;
  data1.script = URL(kOrigin.GetURL(), "/script1.js");
  data1.version_id = 200;
  data1.resources_total_size_bytes = 100;
  std::vector<ResourceRecordPtr> resources1;
  resources1.push_back(CreateResource(1, data1.script, 100));

  // Add registration 2.
  RegistrationData data2;
  data2.registration_id = 101;
  data2.scope = URL(kOrigin.GetURL(), "/bar");
  data2.key = kKey;
  data2.script = URL(kOrigin.GetURL(), "/script2.js");
  data2.version_id = 201;
  data2.resources_total_size_bytes = 200;
  std::vector<ResourceRecordPtr> resources2;
  resources2.push_back(CreateResource(2, data2.script, 200));

  ServiceWorkerDatabase::DeletedVersion deleted_version;
  ASSERT_EQ(ServiceWorkerDatabase::Status::kOk,
            database->WriteRegistration(data1, resources1, &deleted_version));
  ASSERT_EQ(ServiceWorkerDatabase::Status::kOk,
            database->WriteRegistration(data2, resources2, &deleted_version));

  // Write user data associated with the registration1.
  ASSERT_EQ(
      ServiceWorkerDatabase::Status::kOk,
      database->WriteUserData(data1.registration_id, kKey,
                              CreateUserData(data1.registration_id,
                                             {{"key_prefix:key1", "value1"}})));
  ASSERT_EQ(
      ServiceWorkerDatabase::Status::kOk,
      database->WriteUserData(data1.registration_id, kKey,
                              CreateUserData(data1.registration_id,
                                             {{"key_prefix:key2", "value2"}})));
  ASSERT_EQ(
      ServiceWorkerDatabase::Status::kOk,
      database->WriteUserData(data1.registration_id, kKey,
                              CreateUserData(data1.registration_id,
                                             {{"key_prefix:key3", "value3"}})));

  // Write user data associated with the registration2.
  ASSERT_EQ(
      ServiceWorkerDatabase::Status::kOk,
      database->WriteUserData(data2.registration_id, kKey,
                              CreateUserData(data2.registration_id,
                                             {{"key_prefix:key1", "value1"}})));
  ASSERT_EQ(
      ServiceWorkerDatabase::Status::kOk,
      database->WriteUserData(data2.registration_id, kKey,
                              CreateUserData(data2.registration_id,
                                             {{"key_prefix:key2", "value2"}})));
  ASSERT_EQ(ServiceWorkerDatabase::Status::kOk,
            database->WriteUserData(
                data2.registration_id, kKey,
                CreateUserData(data2.registration_id,
                               {{"another_key_prefix:key1", "value1"}})));
  ASSERT_EQ(ServiceWorkerDatabase::Status::kOk,
            database->WriteUserData(
                data2.registration_id, kKey,
                CreateUserData(data2.registration_id,
                               {{"another_key_prefix:key2", "value2"}})));

  // Get all registrations with user data by key prefix.
  std::vector<mojom::ServiceWorkerUserDataPtr> user_data_list;
  ASSERT_EQ(ServiceWorkerDatabase::Status::kOk,
            database->ReadUserDataForAllRegistrationsByKeyPrefix(
                "key_prefix:", &user_data_list));
  ASSERT_EQ(5u, user_data_list.size());

  EXPECT_EQ(data1.registration_id, user_data_list[0]->registration_id);
  EXPECT_EQ("key_prefix:key1", user_data_list[0]->key);
  EXPECT_EQ("value1", user_data_list[0]->value);
  EXPECT_EQ(data2.registration_id, user_data_list[1]->registration_id);
  EXPECT_EQ("key_prefix:key1", user_data_list[1]->key);
  EXPECT_EQ("value1", user_data_list[1]->value);
  EXPECT_EQ(data1.registration_id, user_data_list[2]->registration_id);
  EXPECT_EQ("key_prefix:key2", user_data_list[2]->key);
  EXPECT_EQ("value2", user_data_list[2]->value);
  EXPECT_EQ(data2.registration_id, user_data_list[3]->registration_id);
  EXPECT_EQ("key_prefix:key2", user_data_list[3]->key);
  EXPECT_EQ("value2", user_data_list[3]->value);
  EXPECT_EQ(data1.registration_id, user_data_list[4]->registration_id);
  EXPECT_EQ("key_prefix:key3", user_data_list[4]->key);
  EXPECT_EQ("value3", user_data_list[4]->value);
}

TEST(ServiceWorkerDatabaseTest, ReadUserDataByKeyPrefix) {
  std::unique_ptr<ServiceWorkerDatabase> database(CreateDatabaseInMemory());
  const url::Origin kOrigin = url::Origin::Create(GURL("https://example.com"));
  const blink::StorageKey kKey(kOrigin);

  // Add a registration.
  RegistrationData data;
  data.registration_id = 100;
  data.scope = URL(kOrigin.GetURL(), "/foo");
  data.key = kKey;
  data.script = URL(kOrigin.GetURL(), "/script.js");
  data.version_id = 200;
  data.resources_total_size_bytes = 100;
  std::vector<ResourceRecordPtr> resources;
  resources.push_back(CreateResource(1, data.script, 100));
  ServiceWorkerDatabase::DeletedVersion deleted_version;
  ASSERT_EQ(ServiceWorkerDatabase::Status::kOk,
            database->WriteRegistration(data, resources, &deleted_version));

  // Write user data associated with the registration.
  ASSERT_EQ(ServiceWorkerDatabase::Status::kOk,
            database->WriteUserData(
                data.registration_id, kKey,
                CreateUserData(data.registration_id,
                               {{"key_prefix:key1", "value_c1"},
                                {"key_prefix:key2", "value_c2"},
                                {"other_key_prefix:k1", "value_d1"},
                                {"other_key_prefix:k2", "value_d2"}})));

  std::vector<std::string> user_data;
  EXPECT_EQ(ServiceWorkerDatabase::Status::kOk,
            database->ReadUserDataByKeyPrefix(data.registration_id,
                                              "bogus_prefix:", &user_data));
  EXPECT_THAT(user_data, IsEmpty());

  user_data.clear();
  EXPECT_EQ(ServiceWorkerDatabase::Status::kOk,
            database->ReadUserDataByKeyPrefix(data.registration_id,
                                              "key_prefix:", &user_data));
  EXPECT_THAT(user_data, ElementsAreArray({"value_c1", "value_c2"}));

  user_data.clear();
  EXPECT_EQ(ServiceWorkerDatabase::Status::kOk,
            database->ReadUserDataByKeyPrefix(data.registration_id,
                                              "other_key_prefix:", &user_data));
  EXPECT_THAT(user_data, ElementsAreArray({"value_d1", "value_d2"}));
}

TEST(ServiceWorkerDatabaseTest, ReadUserKeysAndDataByKeyPrefix) {
  std::unique_ptr<ServiceWorkerDatabase> database(CreateDatabaseInMemory());
  const url::Origin kOrigin = url::Origin::Create(GURL("https://example.com"));
  const blink::StorageKey kKey(kOrigin);

  // Add a registration.
  RegistrationData data;
  data.registration_id = 100;
  data.scope = URL(kOrigin.GetURL(), "/foo");
  data.key = kKey;
  data.script = URL(kOrigin.GetURL(), "/script.js");
  data.version_id = 200;
  data.resources_total_size_bytes = 100;
  std::vector<ResourceRecordPtr> resources;
  resources.push_back(CreateResource(1, data.script, 100));
  ServiceWorkerDatabase::DeletedVersion deleted_version;
  ASSERT_EQ(ServiceWorkerDatabase::Status::kOk,
            database->WriteRegistration(data, resources, &deleted_version));

  // Write user data associated with the registration.
  ASSERT_EQ(ServiceWorkerDatabase::Status::kOk,
            database->WriteUserData(
                data.registration_id, kKey,
                CreateUserData(data.registration_id,
                               {{"key_prefix:key1", "value_c1"},
                                {"key_prefix:key2", "value_c2"},
                                {"other_key_prefix:k1", "value_d1"},
                                {"other_key_prefix:k2", "value_d2"}})));

  base::flat_map<std::string, std::string> user_data_map;
  EXPECT_EQ(ServiceWorkerDatabase::Status::kOk,
            database->ReadUserKeysAndDataByKeyPrefix(
                data.registration_id, "bogus_prefix:", &user_data_map));
  EXPECT_THAT(user_data_map, IsEmpty());

  user_data_map.clear();
  EXPECT_EQ(ServiceWorkerDatabase::Status::kOk,
            database->ReadUserKeysAndDataByKeyPrefix(
                data.registration_id, "key_prefix:", &user_data_map));
  EXPECT_THAT(user_data_map,
              ElementsAreArray(std::vector<std::pair<std::string, std::string>>{
                  {"key1", "value_c1"}, {"key2", "value_c2"}}));

  user_data_map.clear();
  EXPECT_EQ(ServiceWorkerDatabase::Status::kOk,
            database->ReadUserKeysAndDataByKeyPrefix(
                data.registration_id, "other_key_prefix:", &user_data_map));
  EXPECT_THAT(user_data_map,
              ElementsAreArray(std::vector<std::pair<std::string, std::string>>{
                  {"k1", "value_d1"}, {"k2", "value_d2"}}));
}

TEST(ServiceWorkerDatabaseTest, UserData_DeleteUserDataByKeyPrefixes) {
  std::unique_ptr<ServiceWorkerDatabase> database(CreateDatabaseInMemory());
  const url::Origin kOrigin = url::Origin::Create(GURL("https://example.com"));
  blink::StorageKey key(kOrigin);

  // Add registration 1.
  RegistrationData data1;
  data1.registration_id = 100;
  data1.scope = URL(kOrigin.GetURL(), "/foo");
  data1.key = key;
  data1.script = URL(kOrigin.GetURL(), "/script1.js");
  data1.version_id = 200;
  data1.resources_total_size_bytes = 100;
  std::vector<ResourceRecordPtr> resources1;
  resources1.push_back(CreateResource(1, data1.script, 100));

  // Add registration 2.
  RegistrationData data2;
  data2.registration_id = 101;
  data2.scope = URL(kOrigin.GetURL(), "/bar");
  data2.key = key;
  data2.script = URL(kOrigin.GetURL(), "/script2.js");
  data2.version_id = 201;
  data2.resources_total_size_bytes = 200;
  std::vector<ResourceRecordPtr> resources2;
  resources2.push_back(CreateResource(2, data2.script, 200));

  ServiceWorkerDatabase::DeletedVersion deleted_version;
  ASSERT_EQ(ServiceWorkerDatabase::Status::kOk,
            database->WriteRegistration(data1, resources1, &deleted_version));
  ASSERT_EQ(ServiceWorkerDatabase::Status::kOk,
            database->WriteRegistration(data2, resources2, &deleted_version));

  // Write user data associated with registration 1.
  ASSERT_EQ(ServiceWorkerDatabase::Status::kOk,
            database->WriteUserData(
                data1.registration_id, key,
                CreateUserData(data1.registration_id,
                               {{"key_prefix:key1", "value_a1"},
                                {"key_prefix:key2", "value_a2"},
                                {"key_prefix:key3", "value_a3"},
                                {"kept_key_prefix:key1", "value_b1"}})));

  // Write user data associated with registration 2.
  ASSERT_EQ(ServiceWorkerDatabase::Status::kOk,
            database->WriteUserData(
                data2.registration_id, key,
                CreateUserData(data2.registration_id,
                               {{"key_prefix:key1", "value_c1"},
                                {"key_prefix:key2", "value_c2"},
                                {"other_key_prefix:key1", "value_d1"},
                                {"other_key_prefix:key2", "value_d2"},
                                {"kept_key_prefix:key1", "value_e1"},
                                {"kept_key_prefix:key2", "value_e2"}})));

  // Deleting user data by key prefixes should return Status::kOk (rather than
  // Status::kErrorNotFound) even if no keys match the prefixes and so nothing
  // is deleted.
  EXPECT_EQ(ServiceWorkerDatabase::Status::kOk,
            database->DeleteUserDataByKeyPrefixes(
                data2.registration_id,
                {"not_found_key_prefix1:", "not_found_key_prefix2:"}));

  // Actually delete user data by key prefixes for registration 2.
  ASSERT_EQ(ServiceWorkerDatabase::Status::kOk,
            database->DeleteUserDataByKeyPrefixes(
                data2.registration_id,
                {"key_prefix:", "other_key_prefix:", "not_found_key_prefix:"}));

  // User data with deleted "key_prefix:" should only remain for registration 1.
  std::vector<mojom::ServiceWorkerUserDataPtr> user_data_list;
  ASSERT_EQ(ServiceWorkerDatabase::Status::kOk,
            database->ReadUserDataForAllRegistrationsByKeyPrefix(
                "key_prefix:", &user_data_list));
  ASSERT_EQ(3u, user_data_list.size());
  EXPECT_EQ(data1.registration_id, user_data_list[0]->registration_id);
  EXPECT_EQ("key_prefix:key1", user_data_list[0]->key);
  EXPECT_EQ("value_a1", user_data_list[0]->value);
  EXPECT_EQ(data1.registration_id, user_data_list[1]->registration_id);
  EXPECT_EQ("key_prefix:key2", user_data_list[1]->key);
  EXPECT_EQ("value_a2", user_data_list[1]->value);
  EXPECT_EQ(data1.registration_id, user_data_list[2]->registration_id);
  EXPECT_EQ("key_prefix:key3", user_data_list[2]->key);
  EXPECT_EQ("value_a3", user_data_list[2]->value);

  // User data for second deleted key prefix should also have been deleted.
  user_data_list.clear();
  ASSERT_EQ(ServiceWorkerDatabase::Status::kOk,
            database->ReadUserDataForAllRegistrationsByKeyPrefix(
                "other_key_prefix:", &user_data_list));
  ASSERT_EQ(0u, user_data_list.size());

  // User data with "kept_key_prefix:" that was not deleted should remain on
  // both registrations.
  user_data_list.clear();
  ASSERT_EQ(ServiceWorkerDatabase::Status::kOk,
            database->ReadUserDataForAllRegistrationsByKeyPrefix(
                "kept_key_prefix:", &user_data_list));
  ASSERT_EQ(3u, user_data_list.size());
  EXPECT_EQ(data1.registration_id, user_data_list[0]->registration_id);
  EXPECT_EQ("kept_key_prefix:key1", user_data_list[0]->key);
  EXPECT_EQ("value_b1", user_data_list[0]->value);
  EXPECT_EQ(data2.registration_id, user_data_list[1]->registration_id);
  EXPECT_EQ("kept_key_prefix:key1", user_data_list[1]->key);
  EXPECT_EQ("value_e1", user_data_list[1]->value);
  EXPECT_EQ(data2.registration_id, user_data_list[2]->registration_id);
  EXPECT_EQ("kept_key_prefix:key2", user_data_list[2]->key);
  EXPECT_EQ("value_e2", user_data_list[2]->value);
}

TEST(ServiceWorkerDatabaseTest,
     UserData_DeleteUserDataForAllRegistrationsByKeyPrefix) {
  std::unique_ptr<ServiceWorkerDatabase> database(CreateDatabaseInMemory());
  const url::Origin kOrigin = url::Origin::Create(GURL("https://example.com"));
  const blink::StorageKey kKey(kOrigin);

  // Add registration 1.
  RegistrationData data1;
  data1.registration_id = 100;
  data1.scope = URL(kOrigin.GetURL(), "/foo");
  data1.key = kKey;
  data1.script = URL(kOrigin.GetURL(), "/script1.js");
  data1.version_id = 200;
  data1.resources_total_size_bytes = 100;
  std::vector<ResourceRecordPtr> resources1;
  resources1.push_back(CreateResource(1, data1.script, 100));

  // Add registration 2.
  RegistrationData data2;
  data2.registration_id = 101;
  data2.scope = URL(kOrigin.GetURL(), "/bar");
  data2.key = kKey;
  data2.script = URL(kOrigin.GetURL(), "/script2.js");
  data2.version_id = 201;
  data2.resources_total_size_bytes = 200;
  std::vector<ResourceRecordPtr> resources2;
  resources2.push_back(CreateResource(2, data2.script, 200));

  ServiceWorkerDatabase::DeletedVersion deleted_version;
  ASSERT_EQ(ServiceWorkerDatabase::Status::kOk,
            database->WriteRegistration(data1, resources1, &deleted_version));
  ASSERT_EQ(ServiceWorkerDatabase::Status::kOk,
            database->WriteRegistration(data2, resources2, &deleted_version));

  // Write user data associated with registration 1.
  ASSERT_EQ(ServiceWorkerDatabase::Status::kOk,
            database->WriteUserData(
                data1.registration_id, kKey,
                CreateUserData(data1.registration_id,
                               {{"key_prefix:key1", "value_a1"},
                                {"key_prefix:key2", "value_a2"},
                                {"key_prefix:key3", "value_a3"},
                                {"kept_key_prefix:key1", "value_b1"}})));

  // Write user data associated with registration 2.
  ASSERT_EQ(ServiceWorkerDatabase::Status::kOk,
            database->WriteUserData(
                data2.registration_id, kKey,
                CreateUserData(data2.registration_id,
                               {{"key_prefix:key1", "value_c1"},
                                {"key_prefix:key2", "value_c2"},
                                {"kept_key_prefix:key1", "value_d1"},
                                {"kept_key_prefix:key2", "value_d2"}})));

  // Deleting user data by key prefixes should return Status::kOk (rather than
  // Status::kErrorNotFound) even if no keys match the prefixes and so nothing
  // is deleted.
  EXPECT_EQ(ServiceWorkerDatabase::Status::kOk,
            database->DeleteUserDataForAllRegistrationsByKeyPrefix(
                "not_found_key_prefix:"));

  // Actually delete user data by key prefixes.
  ASSERT_EQ(
      ServiceWorkerDatabase::Status::kOk,
      database->DeleteUserDataForAllRegistrationsByKeyPrefix("key_prefix:"));

  // User data with deleted "key_prefix:" should be deleted.
  std::vector<mojom::ServiceWorkerUserDataPtr> user_data_list;
  ASSERT_EQ(ServiceWorkerDatabase::Status::kOk,
            database->ReadUserDataForAllRegistrationsByKeyPrefix(
                "key_prefix:", &user_data_list));
  EXPECT_TRUE(user_data_list.empty());

  // User data with "kept_key_prefix:" should remain on both registrations.
  user_data_list.clear();
  ASSERT_EQ(ServiceWorkerDatabase::Status::kOk,
            database->ReadUserDataForAllRegistrationsByKeyPrefix(
                "kept_key_prefix:", &user_data_list));
  ASSERT_EQ(3u, user_data_list.size());

  EXPECT_EQ(data1.registration_id, user_data_list[0]->registration_id);
  EXPECT_EQ("kept_key_prefix:key1", user_data_list[0]->key);
  EXPECT_EQ("value_b1", user_data_list[0]->value);
  EXPECT_EQ(data2.registration_id, user_data_list[1]->registration_id);
  EXPECT_EQ("kept_key_prefix:key1", user_data_list[1]->key);
  EXPECT_EQ("value_d1", user_data_list[1]->value);
  EXPECT_EQ(data2.registration_id, user_data_list[2]->registration_id);
  EXPECT_EQ("kept_key_prefix:key2", user_data_list[2]->key);
  EXPECT_EQ("value_d2", user_data_list[2]->value);
}

TEST(ServiceWorkerDatabaseTest, UserData_DataIsolation) {
  std::unique_ptr<ServiceWorkerDatabase> database(CreateDatabaseInMemory());
  const url::Origin kOrigin = url::Origin::Create(GURL("https://example.com"));
  const blink::StorageKey kKey(kOrigin);

  // Add registration 1.
  RegistrationData data1;
  data1.registration_id = 100;
  data1.scope = URL(kOrigin.GetURL(), "/foo");
  data1.key = kKey;
  data1.script = URL(kOrigin.GetURL(), "/script1.js");
  data1.version_id = 200;
  data1.resources_total_size_bytes = 100;
  std::vector<ResourceRecordPtr> resources1;
  resources1.push_back(CreateResource(1, data1.script, 100));

  // Add registration 2.
  RegistrationData data2;
  data2.registration_id = 101;
  data2.scope = URL(kOrigin.GetURL(), "/bar");
  data2.key = kKey;
  data2.script = URL(kOrigin.GetURL(), "/script2.js");
  data2.version_id = 201;
  data2.resources_total_size_bytes = 200;
  data2.update_via_cache = blink::mojom::ServiceWorkerUpdateViaCache::kImports;
  std::vector<ResourceRecordPtr> resources2;
  resources2.push_back(CreateResource(2, data2.script, 200));

  ServiceWorkerDatabase::DeletedVersion deleted_version;
  ASSERT_EQ(ServiceWorkerDatabase::Status::kOk,
            database->WriteRegistration(data1, resources1, &deleted_version));
  ASSERT_EQ(ServiceWorkerDatabase::Status::kOk,
            database->WriteRegistration(data2, resources2, &deleted_version));

  // Write user data associated with the registration1.
  std::vector<std::string> user_data_out;
  ASSERT_EQ(ServiceWorkerDatabase::Status::kOk,
            database->WriteUserData(
                data1.registration_id, kKey,
                CreateUserData(data1.registration_id, {{"key", "value1"}})));
  EXPECT_EQ(
      ServiceWorkerDatabase::Status::kOk,
      database->ReadUserData(data1.registration_id, {"key"}, &user_data_out));
  ASSERT_EQ(1u, user_data_out.size());
  EXPECT_EQ("value1", user_data_out[0]);
  EXPECT_EQ(
      ServiceWorkerDatabase::Status::kErrorNotFound,
      database->ReadUserData(data2.registration_id, {"key"}, &user_data_out));

  // Write user data associated with the registration2. This shouldn't overwrite
  // the data associated with registration1.
  ASSERT_EQ(ServiceWorkerDatabase::Status::kOk,
            database->WriteUserData(
                data2.registration_id, kKey,
                CreateUserData(data2.registration_id, {{"key", "value2"}})));
  EXPECT_EQ(
      ServiceWorkerDatabase::Status::kOk,
      database->ReadUserData(data1.registration_id, {"key"}, &user_data_out));
  ASSERT_EQ(1u, user_data_out.size());
  EXPECT_EQ("value1", user_data_out[0]);
  EXPECT_EQ(
      ServiceWorkerDatabase::Status::kOk,
      database->ReadUserData(data2.registration_id, {"key"}, &user_data_out));
  ASSERT_EQ(1u, user_data_out.size());
  EXPECT_EQ("value2", user_data_out[0]);

  // Get all registrations with user data.
  std::vector<mojom::ServiceWorkerUserDataPtr> user_data_list;
  ASSERT_EQ(ServiceWorkerDatabase::Status::kOk,
            database->ReadUserDataForAllRegistrations("key", &user_data_list));
  EXPECT_EQ(2u, user_data_list.size());
  EXPECT_EQ(data1.registration_id, user_data_list[0]->registration_id);
  EXPECT_EQ("key", user_data_list[0]->key);
  EXPECT_EQ("value1", user_data_list[0]->value);
  EXPECT_EQ(data2.registration_id, user_data_list[1]->registration_id);
  EXPECT_EQ("key", user_data_list[1]->key);
  EXPECT_EQ("value2", user_data_list[1]->value);

  // Delete the data associated with the registration2. This shouldn't delete
  // the data associated with registration1.
  ASSERT_EQ(ServiceWorkerDatabase::Status::kOk,
            database->DeleteUserData(data2.registration_id, {"key"}));
  EXPECT_EQ(
      ServiceWorkerDatabase::Status::kOk,
      database->ReadUserData(data1.registration_id, {"key"}, &user_data_out));
  ASSERT_EQ(1u, user_data_out.size());
  EXPECT_EQ("value1", user_data_out[0]);
  EXPECT_EQ(
      ServiceWorkerDatabase::Status::kErrorNotFound,
      database->ReadUserData(data2.registration_id, {"key"}, &user_data_out));

  // And again get all registrations with user data.
  user_data_list.clear();
  ASSERT_EQ(ServiceWorkerDatabase::Status::kOk,
            database->ReadUserDataForAllRegistrations("key", &user_data_list));
  EXPECT_EQ(1u, user_data_list.size());
  EXPECT_EQ(data1.registration_id, user_data_list[0]->registration_id);
  EXPECT_EQ("key", user_data_list[0]->key);
  EXPECT_EQ("value1", user_data_list[0]->value);
}

TEST(ServiceWorkerDatabaseTest, UserData_DeleteRegistration) {
  std::unique_ptr<ServiceWorkerDatabase> database(CreateDatabaseInMemory());
  const url::Origin kOrigin = url::Origin::Create(GURL("https://example.com"));
  const blink::StorageKey kKey(kOrigin);

  // Add registration 1.
  RegistrationData data1;
  data1.registration_id = 100;
  data1.scope = URL(kOrigin.GetURL(), "/foo");
  data1.key = kKey;
  data1.script = URL(kOrigin.GetURL(), "/script1.js");
  data1.version_id = 200;
  data1.resources_total_size_bytes = 100;
  std::vector<ResourceRecordPtr> resources1;
  resources1.push_back(CreateResource(1, data1.script, 100));

  // Add registration 2.
  RegistrationData data2;
  data2.registration_id = 101;
  data2.scope = URL(kOrigin.GetURL(), "/bar");
  data2.key = kKey;
  data2.script = URL(kOrigin.GetURL(), "/script2.js");
  data2.version_id = 201;
  data2.resources_total_size_bytes = 200;
  std::vector<ResourceRecordPtr> resources2;
  resources2.push_back(CreateResource(2, data2.script, 200));

  ServiceWorkerDatabase::DeletedVersion deleted_version;
  ASSERT_EQ(ServiceWorkerDatabase::Status::kOk,
            database->WriteRegistration(data1, resources1, &deleted_version));
  ASSERT_EQ(ServiceWorkerDatabase::Status::kOk,
            database->WriteRegistration(data2, resources2, &deleted_version));

  // Write user data associated with the registration1.
  std::vector<std::string> user_data_out;
  ASSERT_EQ(ServiceWorkerDatabase::Status::kOk,
            database->WriteUserData(
                data1.registration_id, kKey,
                CreateUserData(data1.registration_id, {{"key1", "value1"}})));
  ASSERT_EQ(ServiceWorkerDatabase::Status::kOk,
            database->WriteUserData(
                data1.registration_id, kKey,
                CreateUserData(data1.registration_id, {{"key2", "value2"}})));
  ASSERT_EQ(
      ServiceWorkerDatabase::Status::kOk,
      database->ReadUserData(data1.registration_id, {"key1"}, &user_data_out));
  ASSERT_EQ(1u, user_data_out.size());
  ASSERT_EQ("value1", user_data_out[0]);
  ASSERT_EQ(
      ServiceWorkerDatabase::Status::kOk,
      database->ReadUserData(data1.registration_id, {"key2"}, &user_data_out));
  ASSERT_EQ(1u, user_data_out.size());
  ASSERT_EQ("value2", user_data_out[0]);

  // Write user data associated with the registration2.
  ASSERT_EQ(ServiceWorkerDatabase::Status::kOk,
            database->WriteUserData(
                data2.registration_id, kKey,
                CreateUserData(data2.registration_id, {{"key3", "value3"}})));
  ASSERT_EQ(
      ServiceWorkerDatabase::Status::kOk,
      database->ReadUserData(data2.registration_id, {"key3"}, &user_data_out));
  ASSERT_EQ(1u, user_data_out.size());
  ASSERT_EQ("value3", user_data_out[0]);

  // Delete all data associated with the registration1. This shouldn't delete
  // the data associated with registration2.
  ASSERT_EQ(ServiceWorkerDatabase::Status::kOk,
            database->DeleteRegistration(data1.registration_id, kKey,
                                         &deleted_version));
  EXPECT_EQ(
      ServiceWorkerDatabase::Status::kErrorNotFound,
      database->ReadUserData(data1.registration_id, {"key1"}, &user_data_out));
  EXPECT_EQ(
      ServiceWorkerDatabase::Status::kErrorNotFound,
      database->ReadUserData(data1.registration_id, {"key2"}, &user_data_out));
  EXPECT_EQ(
      ServiceWorkerDatabase::Status::kOk,
      database->ReadUserData(data2.registration_id, {"key3"}, &user_data_out));
  ASSERT_EQ(1u, user_data_out.size());
  EXPECT_EQ("value3", user_data_out[0]);
}

TEST(ServiceWorkerDatabaseTest, UserData_UninitializedDatabase) {
  std::unique_ptr<ServiceWorkerDatabase> database(CreateDatabaseInMemory());
  const url::Origin kOrigin = url::Origin::Create(GURL("https://example.com"));
  blink::StorageKey key(kOrigin);

  // Should be failed because the database does not exist.
  std::vector<std::string> user_data_out;
  EXPECT_EQ(ServiceWorkerDatabase::Status::kErrorNotFound,
            database->ReadUserData(100, {"key"}, &user_data_out));

  // Should be failed because the associated registration does not exist.
  EXPECT_EQ(ServiceWorkerDatabase::Status::kErrorNotFound,
            database->WriteUserData(100, key,
                                    CreateUserData(100, {{"key", "value"}})));

  // Deleting non-existent entry should succeed.
  EXPECT_EQ(ServiceWorkerDatabase::Status::kOk,
            database->DeleteUserData(100, {"key"}));

  // Actually create a new database, but not initialized yet.
  database->LazyOpen(true);

  // Should be failed because the database is not initialized.
  ASSERT_EQ(ServiceWorkerDatabase::DATABASE_STATE_UNINITIALIZED,
            database->state_);
  EXPECT_EQ(ServiceWorkerDatabase::Status::kErrorNotFound,
            database->ReadUserData(100, {"key"}, &user_data_out));
  EXPECT_EQ(ServiceWorkerDatabase::Status::kErrorNotFound,
            database->WriteUserData(100, key,
                                    CreateUserData(100, {{"key", "value"}})));

  // Deleting non-existent entry should succeed.
  EXPECT_EQ(ServiceWorkerDatabase::Status::kOk,
            database->DeleteUserData(100, {"key"}));
}

TEST(ServiceWorkerDatabaseTest, UpdateVersionToActive) {
  std::unique_ptr<ServiceWorkerDatabase> database(CreateDatabaseInMemory());
  GURL origin("https://example.com");
  blink::StorageKey key(url::Origin::Create(origin));

  ServiceWorkerDatabase::DeletedVersion deleted_version;

  // Should be false because a registration does not exist.
  EXPECT_EQ(ServiceWorkerDatabase::Status::kErrorNotFound,
            database->UpdateVersionToActive(0, key));

  // Add a registration.
  RegistrationData data;
  data.registration_id = 100;
  data.scope = URL(origin, "/foo");
  data.key = key;
  data.script = URL(origin, "/script.js");
  data.version_id = 200;
  data.is_active = false;
  data.resources_total_size_bytes = 100;
  std::vector<ResourceRecordPtr> resources;
  resources.push_back(CreateResource(1, data.script, 100));
  EXPECT_EQ(ServiceWorkerDatabase::Status::kOk,
            database->WriteRegistration(data, resources, &deleted_version));

  // Make sure that the registration is stored.
  RegistrationDataPtr data_out;
  std::vector<ResourceRecordPtr> resources_out;
  EXPECT_EQ(ServiceWorkerDatabase::Status::kOk,
            database->ReadRegistration(data.registration_id, key, &data_out,
                                       &resources_out));
  VerifyRegistrationData(data, *data_out);
  EXPECT_EQ(1u, resources_out.size());

  // Activate the registration.
  EXPECT_EQ(ServiceWorkerDatabase::Status::kOk,
            database->UpdateVersionToActive(data.registration_id, key));

  // Make sure that the registration is activated.
  resources_out.clear();
  EXPECT_EQ(ServiceWorkerDatabase::Status::kOk,
            database->ReadRegistration(data.registration_id, key, &data_out,
                                       &resources_out));
  mojom::ServiceWorkerRegistrationDataPtr expected_data = data.Clone();
  expected_data->is_active = true;
  VerifyRegistrationData(*expected_data, *data_out);
  EXPECT_EQ(1u, resources_out.size());

  // Delete the registration.
  EXPECT_EQ(ServiceWorkerDatabase::Status::kOk,
            database->DeleteRegistration(data.registration_id, key,
                                         &deleted_version));
  EXPECT_EQ(data.registration_id, deleted_version.registration_id);

  // Should be false because the registration is gone.
  EXPECT_EQ(ServiceWorkerDatabase::Status::kErrorNotFound,
            database->UpdateVersionToActive(data.registration_id, key));
}

TEST(ServiceWorkerDatabaseTest, UpdateLastCheckTime) {
  std::unique_ptr<ServiceWorkerDatabase> database(CreateDatabaseInMemory());
  GURL origin("https://example.com");
  blink::StorageKey key(url::Origin::Create(origin));
  ServiceWorkerDatabase::DeletedVersion deleted_version;

  // Should be false because a registration does not exist.
  EXPECT_EQ(ServiceWorkerDatabase::Status::kErrorNotFound,
            database->UpdateLastCheckTime(0, key, base::Time::Now()));

  // Add a registration.
  RegistrationData data;
  data.registration_id = 100;
  data.scope = URL(origin, "/foo");
  data.key = key;
  data.script = URL(origin, "/script.js");
  data.version_id = 200;
  data.last_update_check = base::Time::Now();
  data.resources_total_size_bytes = 100;
  std::vector<ResourceRecordPtr> resources;
  resources.push_back(CreateResource(1, data.script, 100));
  EXPECT_EQ(ServiceWorkerDatabase::Status::kOk,
            database->WriteRegistration(data, resources, &deleted_version));

  // Make sure that the registration is stored.
  RegistrationDataPtr data_out;
  std::vector<ResourceRecordPtr> resources_out;
  EXPECT_EQ(ServiceWorkerDatabase::Status::kOk,
            database->ReadRegistration(data.registration_id, key, &data_out,
                                       &resources_out));
  VerifyRegistrationData(data, *data_out);
  EXPECT_EQ(1u, resources_out.size());

  // Update the last check time.
  base::Time updated_time = base::Time::Now();
  EXPECT_EQ(
      ServiceWorkerDatabase::Status::kOk,
      database->UpdateLastCheckTime(data.registration_id, key, updated_time));

  // Make sure that the registration is updated.
  resources_out.clear();
  EXPECT_EQ(ServiceWorkerDatabase::Status::kOk,
            database->ReadRegistration(data.registration_id, key, &data_out,
                                       &resources_out));
  mojom::ServiceWorkerRegistrationDataPtr expected_data = data.Clone();
  expected_data->last_update_check = updated_time;
  VerifyRegistrationData(*expected_data, *data_out);
  EXPECT_EQ(1u, resources_out.size());

  // Delete the registration.
  EXPECT_EQ(ServiceWorkerDatabase::Status::kOk,
            database->DeleteRegistration(data.registration_id, key,
                                         &deleted_version));
  EXPECT_EQ(data.registration_id, deleted_version.registration_id);

  // Should be false because the registration is gone.
  EXPECT_EQ(ServiceWorkerDatabase::Status::kErrorNotFound,
            database->UpdateLastCheckTime(data.registration_id, key,
                                          base::Time::Now()));
}

TEST(ServiceWorkerDatabaseTest, UpdateFetchHandlerType) {
  std::unique_ptr<ServiceWorkerDatabase> database(CreateDatabaseInMemory());
  GURL origin("https://example.com");
  blink::StorageKey key(url::Origin::Create(origin));
  ServiceWorkerDatabase::DeletedVersion deleted_version;

  // Should be false because a registration does not exist.
  EXPECT_EQ(ServiceWorkerDatabase::Status::kErrorNotFound,
            database->UpdateLastCheckTime(0, key, base::Time::Now()));

  // Add a registration.
  RegistrationData data;
  data.registration_id = 100;
  data.scope = URL(origin, "/foo");
  data.key = key;
  data.script = URL(origin, "/script.js");
  data.version_id = 200;
  data.last_update_check = base::Time::Now();
  data.fetch_handler_type =
      blink::mojom::ServiceWorkerFetchHandlerType::kNotSkippable;
  data.resources_total_size_bytes = 100;
  std::vector<ResourceRecordPtr> resources;
  resources.push_back(CreateResource(1, data.script, 100));
  EXPECT_EQ(ServiceWorkerDatabase::Status::kOk,
            database->WriteRegistration(data, resources, &deleted_version));

  // Make sure that the registration is stored.
  RegistrationDataPtr data_out;
  std::vector<ResourceRecordPtr> resources_out;
  EXPECT_EQ(ServiceWorkerDatabase::Status::kOk,
            database->ReadRegistration(data.registration_id, key, &data_out,
                                       &resources_out));
  VerifyRegistrationData(data, *data_out);
  EXPECT_EQ(1u, resources_out.size());

  // Update the fetch handler type.
  EXPECT_EQ(ServiceWorkerDatabase::Status::kOk,
            database->UpdateFetchHandlerType(
                data.registration_id, key,
                blink::mojom::ServiceWorkerFetchHandlerType::kNoHandler));

  // Make sure that the registration is updated.
  resources_out.clear();
  EXPECT_EQ(ServiceWorkerDatabase::Status::kOk,
            database->ReadRegistration(data.registration_id, key, &data_out,
                                       &resources_out));
  mojom::ServiceWorkerRegistrationDataPtr expected_data = data.Clone();
  expected_data->fetch_handler_type =
      blink::mojom::ServiceWorkerFetchHandlerType::kNoHandler;
  VerifyRegistrationData(*expected_data, *data_out);
  EXPECT_EQ(1u, resources_out.size());

  // Delete the registration.
  EXPECT_EQ(ServiceWorkerDatabase::Status::kOk,
            database->DeleteRegistration(data.registration_id, key,
                                         &deleted_version));
  EXPECT_EQ(data.registration_id, deleted_version.registration_id);

  // Should be false because the registration is gone.
  EXPECT_EQ(ServiceWorkerDatabase::Status::kErrorNotFound,
            database->UpdateFetchHandlerType(
                data.registration_id, key,
                blink::mojom::ServiceWorkerFetchHandlerType::kNotSkippable));
}

TEST(ServiceWorkerDatabaseTest, UncommittedAndPurgeableResourceIds) {
  std::unique_ptr<ServiceWorkerDatabase> database(CreateDatabaseInMemory());

  // Write {1, 2, 3} into the uncommitted list.
  std::vector<int64_t> ids1 = {1, 2, 3};
  EXPECT_EQ(ServiceWorkerDatabase::Status::kOk,
            database->WriteUncommittedResourceIds(ids1));

  std::vector<int64_t> ids_out;
  EXPECT_EQ(ServiceWorkerDatabase::Status::kOk,
            database->GetUncommittedResourceIds(&ids_out));
  EXPECT_EQ(ids1, ids_out);

  // Write {2, 4} into the uncommitted list.
  std::vector<int64_t> ids2 = {2, 4};
  EXPECT_EQ(ServiceWorkerDatabase::Status::kOk,
            database->WriteUncommittedResourceIds(ids2));

  ids_out.clear();
  EXPECT_EQ(ServiceWorkerDatabase::Status::kOk,
            database->GetUncommittedResourceIds(&ids_out));
  std::vector<int64_t> expected = {1, 2, 3, 4};
  EXPECT_EQ(expected, ids_out);

  // Move {2, 4} from the uncommitted list to the purgeable list.
  EXPECT_EQ(ServiceWorkerDatabase::Status::kOk,
            database->PurgeUncommittedResourceIds(ids2));
  ids_out.clear();
  EXPECT_EQ(ServiceWorkerDatabase::Status::kOk,
            database->GetPurgeableResourceIds(&ids_out));
  EXPECT_EQ(ids2, ids_out);

  // Delete {2, 4} from the purgeable list.
  EXPECT_EQ(ServiceWorkerDatabase::Status::kOk,
            database->ClearPurgeableResourceIds(ids2));
  ids_out.clear();
  EXPECT_EQ(ServiceWorkerDatabase::Status::kOk,
            database->GetPurgeableResourceIds(&ids_out));
  EXPECT_TRUE(ids_out.empty());

  // {1, 3} should be still in the uncommitted list.
  ids_out.clear();
  EXPECT_EQ(ServiceWorkerDatabase::Status::kOk,
            database->GetUncommittedResourceIds(&ids_out));
  expected = {1, 3};
  EXPECT_EQ(expected, ids_out);
}

TEST(ServiceWorkerDatabaseTest, DeleteAllDataForStorageKey) {
  std::unique_ptr<ServiceWorkerDatabase> database(CreateDatabaseInMemory());
  ServiceWorkerDatabase::DeletedVersion deleted_version;

  // Data associated with |key1| will be removed.
  GURL url1("https://example.com");
  GURL url2("https://example.org");
  url::Origin origin1 = url::Origin::Create(url1);
  url::Origin origin2 = url::Origin::Create(url2);
  blink::StorageKey key1(origin1);
  blink::StorageKey key2(origin2);

  // |key1| has two registrations (registration1 and registration2).
  RegistrationData data1;
  data1.registration_id = 10;
  data1.scope = URL(url1, "/foo");
  data1.key = key1;
  data1.script = URL(url1, "/resource1");
  data1.version_id = 100;
  data1.resources_total_size_bytes = 2013 + 512;

  std::vector<ResourceRecordPtr> resources1;
  resources1.push_back(CreateResource(1, URL(url1, "/resource1"), 2013));
  resources1.push_back(CreateResource(2, URL(url1, "/resource2"), 512));
  ASSERT_EQ(ServiceWorkerDatabase::Status::kOk,
            database->WriteRegistration(data1, resources1, &deleted_version));
  ASSERT_EQ(ServiceWorkerDatabase::Status::kOk,
            database->WriteUserData(
                data1.registration_id, key1,
                CreateUserData(data1.registration_id, {{"key1", "value1"}})));
  ASSERT_EQ(ServiceWorkerDatabase::Status::kOk,
            database->WriteUserData(
                data1.registration_id, key1,
                CreateUserData(data1.registration_id, {{"key2", "value2"}})));

  RegistrationData data2;
  data2.registration_id = 11;
  data2.scope = URL(url1, "/bar");
  data2.key = key1;
  data2.script = URL(url1, "/resource3");
  data2.version_id = 101;
  data2.resources_total_size_bytes = 4 + 5;

  std::vector<ResourceRecordPtr> resources2;
  resources2.push_back(CreateResource(3, URL(url1, "/resource3"), 4));
  resources2.push_back(CreateResource(4, URL(url1, "/resource4"), 5));
  ASSERT_EQ(ServiceWorkerDatabase::Status::kOk,
            database->WriteRegistration(data2, resources2, &deleted_version));
  ASSERT_EQ(ServiceWorkerDatabase::Status::kOk,
            database->WriteUserData(
                data2.registration_id, key1,
                CreateUserData(data2.registration_id, {{"key3", "value3"}})));
  ASSERT_EQ(ServiceWorkerDatabase::Status::kOk,
            database->WriteUserData(
                data2.registration_id, key1,
                CreateUserData(data2.registration_id, {{"key4", "value4"}})));

  // |key2| has one registration (registration3).
  RegistrationData data3;
  data3.registration_id = 12;
  data3.scope = URL(url2, "/hoge");
  data3.key = key2;
  data3.script = URL(url2, "/resource5");
  data3.version_id = 102;
  data3.resources_total_size_bytes = 6 + 7;

  std::vector<ResourceRecordPtr> resources3;
  resources3.push_back(CreateResource(5, URL(url2, "/resource5"), 6));
  resources3.push_back(CreateResource(6, URL(url2, "/resource6"), 7));
  ASSERT_EQ(ServiceWorkerDatabase::Status::kOk,
            database->WriteRegistration(data3, resources3, &deleted_version));
  ASSERT_EQ(ServiceWorkerDatabase::Status::kOk,
            database->WriteUserData(
                data3.registration_id, key2,
                CreateUserData(data3.registration_id, {{"key5", "value5"}})));
  ASSERT_EQ(ServiceWorkerDatabase::Status::kOk,
            database->WriteUserData(
                data3.registration_id, key2,
                CreateUserData(data3.registration_id, {{"key6", "value6"}})));

  std::set<blink::StorageKey> keys_to_delete;
  std::vector<int64_t> newly_purgeable_resources;
  keys_to_delete.insert(key1);
  EXPECT_EQ(ServiceWorkerDatabase::Status::kOk,
            database->DeleteAllDataForStorageKeys(keys_to_delete,
                                                  &newly_purgeable_resources));

  // |key1| should be removed from the unique origin list.
  std::set<blink::StorageKey> unique_keys;
  EXPECT_EQ(ServiceWorkerDatabase::Status::kOk,
            database->GetStorageKeysWithRegistrations(&unique_keys));
  EXPECT_EQ(1u, unique_keys.size());
  EXPECT_TRUE(base::Contains(unique_keys, key2));

  // The registrations for |key1| should be removed.
  std::vector<mojom::ServiceWorkerRegistrationDataPtr> registrations;
  EXPECT_EQ(
      ServiceWorkerDatabase::Status::kOk,
      database->GetRegistrationsForStorageKey(key1, &registrations, nullptr));
  EXPECT_TRUE(registrations.empty());
  blink::StorageKey key_out;
  EXPECT_EQ(
      ServiceWorkerDatabase::Status::kErrorNotFound,
      database->ReadRegistrationStorageKey(data1.registration_id, &key_out));

  // The registration for |key2| should not be removed.
  RegistrationDataPtr data_out;
  std::vector<ResourceRecordPtr> resources_out;
  EXPECT_EQ(ServiceWorkerDatabase::Status::kOk,
            database->ReadRegistration(data3.registration_id, key2, &data_out,
                                       &resources_out));
  VerifyRegistrationData(data3, *data_out);
  VerifyResourceRecords(resources3, resources_out);
  EXPECT_EQ(
      ServiceWorkerDatabase::Status::kOk,
      database->ReadRegistrationStorageKey(data3.registration_id, &key_out));
  EXPECT_EQ(key2, key_out);

  // The resources associated with |key1| should be purgeable.
  std::vector<int64_t> purgeable_ids_out;
  EXPECT_EQ(ServiceWorkerDatabase::Status::kOk,
            database->GetPurgeableResourceIds(&purgeable_ids_out));
  EXPECT_EQ(4u, purgeable_ids_out.size());
  EXPECT_TRUE(base::Contains(purgeable_ids_out, 1));
  EXPECT_TRUE(base::Contains(purgeable_ids_out, 2));
  EXPECT_TRUE(base::Contains(purgeable_ids_out, 3));
  EXPECT_TRUE(base::Contains(purgeable_ids_out, 4));

  // The user data associated with |key1| should be removed.
  std::vector<std::string> user_data_out;
  EXPECT_EQ(
      ServiceWorkerDatabase::Status::kErrorNotFound,
      database->ReadUserData(data1.registration_id, {"key1"}, &user_data_out));
  EXPECT_EQ(
      ServiceWorkerDatabase::Status::kErrorNotFound,
      database->ReadUserData(data1.registration_id, {"key2"}, &user_data_out));
  EXPECT_EQ(
      ServiceWorkerDatabase::Status::kErrorNotFound,
      database->ReadUserData(data2.registration_id, {"key3"}, &user_data_out));
  EXPECT_EQ(
      ServiceWorkerDatabase::Status::kErrorNotFound,
      database->ReadUserData(data2.registration_id, {"key4"}, &user_data_out));

  // The user data associated with |key2| should not be removed.
  EXPECT_EQ(
      ServiceWorkerDatabase::Status::kOk,
      database->ReadUserData(data3.registration_id, {"key5"}, &user_data_out));
  ASSERT_EQ(1u, user_data_out.size());
  EXPECT_EQ("value5", user_data_out[0]);
  EXPECT_EQ(
      ServiceWorkerDatabase::Status::kOk,
      database->ReadUserData(data3.registration_id, {"key6"}, &user_data_out));
  ASSERT_EQ(1u, user_data_out.size());
  EXPECT_EQ("value6", user_data_out[0]);
}

TEST(ServiceWorkerDatabaseTest, DestroyDatabase) {
  base::ScopedTempDir database_dir;
  ASSERT_TRUE(database_dir.CreateUniqueTempDir());
  std::unique_ptr<ServiceWorkerDatabase> database(
      CreateDatabase(database_dir.GetPath()));

  EXPECT_EQ(ServiceWorkerDatabase::Status::kOk, database->LazyOpen(true));
  ASSERT_TRUE(base::DirectoryExists(database_dir.GetPath()));

  EXPECT_EQ(ServiceWorkerDatabase::Status::kOk, database->DestroyDatabase());
  ASSERT_FALSE(base::DirectoryExists(database_dir.GetPath()));
}

TEST(ServiceWorkerDatabaseTest, Corruption_NoMainResource) {
  std::unique_ptr<ServiceWorkerDatabase> database(CreateDatabaseInMemory());
  ServiceWorkerDatabase::DeletedVersion deleted_version;

  GURL origin("https://example.com");

  RegistrationData data;
  data.registration_id = 10;
  data.scope = URL(origin, "/foo");
  data.key = blink::StorageKey(url::Origin::Create(data.scope));
  data.script = URL(origin, "/resource1");
  data.version_id = 100;
  data.resources_total_size_bytes = 2016;

  // Simulate that "/resource1" wasn't correctly written in the database by not
  // adding it.
  std::vector<ResourceRecordPtr> resources;
  resources.push_back(CreateResource(2, URL(origin, "/resource2"), 2016));

  ASSERT_EQ(ServiceWorkerDatabase::Status::kOk,
            database->WriteRegistration(data, resources, &deleted_version));

  // The database should detect lack of the main resource (i.e. "/resource1").
  RegistrationDataPtr data_out;
  std::vector<ResourceRecordPtr> resources_out;
  EXPECT_EQ(
      ServiceWorkerDatabase::Status::kErrorCorrupted,
      database->ReadRegistration(data.registration_id,
                                 blink::StorageKey(url::Origin::Create(origin)),
                                 &data_out, &resources_out));
  EXPECT_TRUE(resources_out.empty());
}

// Tests that GetRegistrationsForStorageKey() detects corruption without
// crashing. It must delete the database after freeing the iterator it uses to
// read all registrations. Regression test for https://crbug.com/909024.
TEST(ServiceWorkerDatabaseTest, Corruption_GetRegistrationsForStorageKey) {
  std::unique_ptr<ServiceWorkerDatabase> database(CreateDatabaseInMemory());
  ServiceWorkerDatabase::DeletedVersion deleted_version;
  std::vector<ResourceRecordPtr> resources;
  GURL origin("https://example.com");
  blink::StorageKey key(url::Origin::Create(origin));

  // Write a normal registration.
  RegistrationData data1;
  data1.registration_id = 1;
  data1.scope = URL(origin, "/foo");
  data1.key = key;
  data1.script = URL(origin, "/resource1");
  data1.version_id = 1;
  data1.resources_total_size_bytes = 2016;
  resources.push_back(CreateResource(1, URL(origin, "/resource1"), 2016));
  ASSERT_EQ(ServiceWorkerDatabase::Status::kOk,
            database->WriteRegistration(data1, resources, &deleted_version));

  // Write a corrupt registration.
  RegistrationData data2;
  data2.registration_id = 2;
  data2.scope = URL(origin, "/foo");
  data2.key = key;
  data2.script = URL(origin, "/resource2");
  data2.version_id = 2;
  data2.resources_total_size_bytes = 2016;
  // Simulate that "/resource2" wasn't correctly written in the database by
  // not adding it.
  resources.clear();
  resources.push_back(CreateResource(3, URL(origin, "/resource3"), 2016));
  ASSERT_EQ(ServiceWorkerDatabase::Status::kOk,
            database->WriteRegistration(data2, resources, &deleted_version));

  // Call GetRegistrationsForStorageKey(). It should detect corruption, and not
  // crash.
  base::HistogramTester histogram_tester;
  std::vector<mojom::ServiceWorkerRegistrationDataPtr> registrations;
  std::vector<std::vector<ResourceRecordPtr>> resources_list;
  EXPECT_EQ(ServiceWorkerDatabase::Status::kErrorCorrupted,
            database->GetRegistrationsForStorageKey(key, &registrations,
                                                    &resources_list));
  EXPECT_TRUE(registrations.empty());
  EXPECT_TRUE(resources_list.empty());

  // There should be three "read" operations logged:
  // 1. Reading all registration data.
  // 2. Reading the resources of the first registration.
  // 3. Reading the resources of the second registration. This one fails.
  histogram_tester.ExpectTotalCount("ServiceWorker.Database.ReadResult", 3);
  histogram_tester.ExpectBucketCount("ServiceWorker.Database.ReadResult",
                                     ServiceWorkerDatabase::Status::kOk, 2);
  histogram_tester.ExpectBucketCount(
      "ServiceWorker.Database.ReadResult",
      ServiceWorkerDatabase::Status::kErrorCorrupted, 1);
}

// Test that invalid WebFeatures on disk are ignored when reading a
// registration. See https://crbug.com/965944.
TEST(ServiceWorkerDatabaseTest, InvalidWebFeature) {
  std::unique_ptr<ServiceWorkerDatabase> database(CreateDatabaseInMemory());

  // Prepare a registration proto that has invalid features.
  ServiceWorkerRegistrationData data;
  data.set_registration_id(1);
  data.set_scope_url("https://example.com");
  data.set_script_url("https://example.com/sw");
  data.set_version_id(1);
  data.set_is_active(true);
  data.set_has_fetch_handler(true);
  data.set_fetch_handler_skippable_type(
      ServiceWorkerRegistrationData::NOT_SKIPPABLE);
  data.set_last_update_check_time(
      base::Time::Now().ToDeltaSinceWindowsEpoch().InMicroseconds());

  data.add_used_features(
      static_cast<uint32_t>(blink::mojom::WebFeature::kFetch));
  // Add a removed feature.
  data.add_used_features(2067);
  data.add_used_features(
      static_cast<uint32_t>(blink::mojom::WebFeature::kBackgroundSync));
  // Add an out of range feature.
  data.add_used_features(
      static_cast<uint32_t>(blink::mojom::WebFeature::kNumberOfFeatures) + 11);
  data.add_used_features(
      static_cast<uint32_t>(blink::mojom::WebFeature::kNetInfoType));

  database->next_avail_registration_id_ = 2;
  database->next_avail_version_id_ = 2;

  blink::StorageKey key =
      blink::StorageKey::CreateFromStringForTesting(data.scope_url());

  // Write the serialization.
  std::string value;
  ASSERT_TRUE(data.SerializeToString(&value));

  // Parse the serialized data. The invalid features should be ignored.
  RegistrationDataPtr registration;
  ASSERT_EQ(ServiceWorkerDatabase::Status::kOk,
            database->ParseRegistrationData(value, key, &registration));
  std::vector<blink::mojom::WebFeature> expect = {
      blink::mojom::WebFeature::kFetch,
      blink::mojom::WebFeature::kBackgroundSync,
      blink::mojom::WebFeature::kNetInfoType};
  EXPECT_EQ(expect, registration->used_features);
}

// Check that every field of CrossOriginEmbedderPolicy can be properly
// serialized and deserialized.
TEST(ServiceWorkerDatabaseTest, CrossOriginEmbedderPolicyStoreRestore) {
  auto store_and_restore = [](network::CrossOriginEmbedderPolicy policy) {
    // Build the minimal RegistrationData with the given |policy|.
    GURL origin("https://example.com");
    RegistrationData data;
    data.registration_id = 123;
    data.scope = URL(origin, "/foo");
    data.key = blink::StorageKey(url::Origin::Create(data.scope));
    data.script = URL(origin, "/script.js");
    data.version_id = 456;
    data.resources_total_size_bytes = 100;
    data.cross_origin_embedder_policy = policy;
    data.policy_container_policies =
        blink::mojom::PolicyContainerPolicies::New();
    data.policy_container_policies->cross_origin_embedder_policy = policy.value;
    std::vector<ResourceRecordPtr> resources;
    resources.push_back(CreateResource(1, data.script, 100));

    // Store.
    std::unique_ptr<ServiceWorkerDatabase> database(CreateDatabaseInMemory());
    ServiceWorkerDatabase::DeletedVersion deleted_version;
    ASSERT_EQ(ServiceWorkerDatabase::Status::kOk,
              database->WriteRegistration(data, resources, &deleted_version));

    // Restore.
    std::vector<mojom::ServiceWorkerRegistrationDataPtr> registrations;
    std::vector<std::vector<ResourceRecordPtr>> resources_list;
    EXPECT_EQ(ServiceWorkerDatabase::Status::kOk,
              database->GetRegistrationsForStorageKey(
                  blink::StorageKey(url::Origin::Create(origin)),
                  &registrations, &resources_list));

    // The data must not have been altered.
    VerifyRegistrationData(data, *registrations[0]);
  };

  {
    network::CrossOriginEmbedderPolicy policy;
    policy.value = network::mojom::CrossOriginEmbedderPolicyValue::kRequireCorp;
    store_and_restore(policy);
    policy.value = network::mojom::CrossOriginEmbedderPolicyValue::kNone;
    store_and_restore(policy);
    policy.value =
        network::mojom::CrossOriginEmbedderPolicyValue::kCredentialless;
    store_and_restore(policy);
  }

  {
    network::CrossOriginEmbedderPolicy policy;
    policy.reporting_endpoint = "foo";
    store_and_restore(policy);
  }

  {
    network::CrossOriginEmbedderPolicy policy;
    policy.report_only_value =
        network::mojom::CrossOriginEmbedderPolicyValue::kRequireCorp;
    store_and_restore(policy);
    policy.report_only_value =
        network::mojom::CrossOriginEmbedderPolicyValue::kNone;
    store_and_restore(policy);
    policy.report_only_value =
        network::mojom::CrossOriginEmbedderPolicyValue::kCredentialless;
    store_and_restore(policy);
  }

  {
    network::CrossOriginEmbedderPolicy policy;
    policy.report_only_reporting_endpoint = "bar";
    store_and_restore(policy);
  }
}

TEST(ServiceWorkerDatabaseTest, NoCrossOriginEmbedderPolicyValue) {
  std::unique_ptr<ServiceWorkerDatabase> database(CreateDatabaseInMemory());

  // Prepare a registration proto that doesn't have Cross Origin Embedder
  // Policy.
  ServiceWorkerRegistrationData data;
  data.set_registration_id(1);
  data.set_scope_url("https://example.com");
  data.set_script_url("https://example.com/sw");
  data.set_version_id(1);
  data.set_is_active(true);
  data.set_has_fetch_handler(true);
  data.set_fetch_handler_skippable_type(
      ServiceWorkerRegistrationData::NOT_SKIPPABLE);
  data.set_last_update_check_time(
      base::Time::Now().ToDeltaSinceWindowsEpoch().InMicroseconds());

  database->next_avail_registration_id_ = 2;
  database->next_avail_version_id_ = 2;

  blink::StorageKey key =
      blink::StorageKey::CreateFromStringForTesting(data.scope_url());

  // Write the serialization.
  std::string value;
  ASSERT_TRUE(data.SerializeToString(&value));

  // Parse the serialized data. The policy is kNone if it's not set.
  RegistrationDataPtr registration;
  ASSERT_EQ(ServiceWorkerDatabase::Status::kOk,
            database->ParseRegistrationData(value, key, &registration));
  EXPECT_EQ(network::mojom::CrossOriginEmbedderPolicyValue::kNone,
            registration->cross_origin_embedder_policy.value);
}

const network::mojom::WebSandboxFlags kWebSandboxFlags[] = {
    network::mojom::WebSandboxFlags::kNone,
    network::mojom::WebSandboxFlags::kNavigation,
    network::mojom::WebSandboxFlags::kPlugins,
    network::mojom::WebSandboxFlags::kOrigin,
    network::mojom::WebSandboxFlags::kForms,
    network::mojom::WebSandboxFlags::kScripts,
    network::mojom::WebSandboxFlags::kTopNavigation,
    network::mojom::WebSandboxFlags::kPopups,
    network::mojom::WebSandboxFlags::kAutomaticFeatures,
    network::mojom::WebSandboxFlags::kPointerLock,
    network::mojom::WebSandboxFlags::kDocumentDomain,
    network::mojom::WebSandboxFlags::kOrientationLock,
    network::mojom::WebSandboxFlags::kPropagatesToAuxiliaryBrowsingContexts,
    network::mojom::WebSandboxFlags::kModals,
    network::mojom::WebSandboxFlags::kPresentationController,
    network::mojom::WebSandboxFlags::kTopNavigationByUserActivation,
    network::mojom::WebSandboxFlags::kDownloads,
    network::mojom::WebSandboxFlags::kStorageAccessByUserActivation,
    network::mojom::WebSandboxFlags::kTopNavigationToCustomProtocols,
    network::mojom::WebSandboxFlags::kAll,
};

static_assert(
    network::mojom::WebSandboxFlags::kTopNavigationToCustomProtocols ==
        network::mojom::WebSandboxFlags::kMaxValue,
    "The array should contain all the flags");

// Check that every field of PolicyContainerPolicies can be properly
// serialized and deserialized.
TEST(ServiceWorkerDatabaseTest, PolicyContainerPoliciesStoreRestore) {
  auto store_and_restore =
      [](blink::mojom::PolicyContainerPoliciesPtr policies) {
        // Build the minimal RegistrationData with the given |policy|.
        GURL origin("https://example.com");
        RegistrationData data;
        data.registration_id = 123;
        data.scope = URL(origin, "/foo");
        data.key = blink::StorageKey(url::Origin::Create(data.scope));
        data.script = URL(origin, "/script.js");
        data.version_id = 456;
        data.resources_total_size_bytes = 100;
        data.policy_container_policies = std::move(policies);
        data.cross_origin_embedder_policy.value =
            data.policy_container_policies->cross_origin_embedder_policy;
        std::vector<ResourceRecordPtr> resources;
        resources.push_back(CreateResource(1, data.script, 100));

        // Store.
        std::unique_ptr<ServiceWorkerDatabase> database(
            CreateDatabaseInMemory());
        ServiceWorkerDatabase::DeletedVersion deleted_version;
        ASSERT_EQ(
            ServiceWorkerDatabase::Status::kOk,
            database->WriteRegistration(data, resources, &deleted_version));

        // Restore.
        std::vector<mojom::ServiceWorkerRegistrationDataPtr> registrations;
        std::vector<std::vector<ResourceRecordPtr>> resources_list;
        EXPECT_EQ(ServiceWorkerDatabase::Status::kOk,
                  database->GetRegistrationsForStorageKey(
                      blink::StorageKey(url::Origin::Create(origin)),
                      &registrations, &resources_list));

        // The data must not have been altered.
        VerifyRegistrationData(data, *registrations[0]);
      };

  {
    auto policies = blink::mojom::PolicyContainerPolicies::New();
    store_and_restore(policies->Clone());

    for (auto value : {
             network::mojom::CrossOriginEmbedderPolicyValue::kNone,
             network::mojom::CrossOriginEmbedderPolicyValue::kRequireCorp,
             network::mojom::CrossOriginEmbedderPolicyValue::kCredentialless,
         }) {
      policies->cross_origin_embedder_policy = value;
      store_and_restore(policies->Clone());
    }
  }

  {
    auto policies = blink::mojom::PolicyContainerPolicies::New();

    for (auto referrer_policy : {
             network::mojom::ReferrerPolicy::kAlways,
             network::mojom::ReferrerPolicy::kDefault,
             network::mojom::ReferrerPolicy::kNoReferrerWhenDowngrade,
             network::mojom::ReferrerPolicy::kNever,
             network::mojom::ReferrerPolicy::kOrigin,
             network::mojom::ReferrerPolicy::kOriginWhenCrossOrigin,
             network::mojom::ReferrerPolicy::kStrictOriginWhenCrossOrigin,
             network::mojom::ReferrerPolicy::kSameOrigin,
             network::mojom::ReferrerPolicy::kStrictOrigin,
         }) {
      policies->referrer_policy = referrer_policy;
      store_and_restore(policies->Clone());
    }
  }

  {
    auto policies = blink::mojom::PolicyContainerPolicies::New();

    for (auto sandbox_flags : kWebSandboxFlags) {
      policies->sandbox_flags = sandbox_flags;
      store_and_restore(policies->Clone());
      for (auto sandbox_flags_2 : kWebSandboxFlags) {
        if (sandbox_flags_2 >= sandbox_flags)
          break;
        policies->sandbox_flags = sandbox_flags | sandbox_flags_2;
        store_and_restore(policies->Clone());
      }
    }
  }

  {
    auto policies = blink::mojom::PolicyContainerPolicies::New();

    for (auto ip_address_space : {
             network::mojom::IPAddressSpace::kLocal,
             network::mojom::IPAddressSpace::kPrivate,
             network::mojom::IPAddressSpace::kPublic,
             network::mojom::IPAddressSpace::kUnknown,
         }) {
      policies->ip_address_space = ip_address_space;
      store_and_restore(policies->Clone());
    }
  }
}

// As part of crbug.com/1199077 ServiceWorkerDataBase was refactored to use
// blink::StorageKey instead of url::Origin/GURL. The refactor is/should be a
// no-op but this test exists to confirms that a DB created with the Origin/GURL
// impl can still be correctly read by the blink::StorageKey impl.
TEST(ServiceWorkerDatabaseTest, StorageKeyImplCanReadPreviousOriginImplDB) {
  base::FilePath root_path;
  base::PathService::Get(base::DIR_SOURCE_ROOT, &root_path);
  base::FilePath database_dir = root_path.AppendASCII(
      "components/test/data/service_worker/created_by_origin_impl/Database/");

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  ASSERT_TRUE(base::CopyDirectory(database_dir, temp_dir.GetPath(),
                                  /*recursive=*/true));

  base::FilePath temp_database_dir = temp_dir.GetPath();
  temp_database_dir = temp_database_dir.AppendASCII("Database");

  std::unique_ptr<ServiceWorkerDatabase> database(
      CreateDatabase(temp_database_dir));

  int64_t next_registration_id;
  int64_t next_version_id;
  int64_t next_resource_id;
  // Make sure to read in the IDs first, otherwise the
  // GetRegistrationsForStorageKey will return as corrupted.
  EXPECT_EQ(ServiceWorkerDatabase::Status::kOk,
            database->GetNextAvailableIds(&next_registration_id,
                                          &next_version_id, &next_resource_id));
  EXPECT_EQ(next_registration_id, 1);
  EXPECT_EQ(next_version_id, 1);
  EXPECT_EQ(next_resource_id, 1);

  // https://googlechrome.github.io/samples/service-worker/basic/ provided the
  // service worker for this test.
  blink::StorageKey key = blink::StorageKey::CreateFromStringForTesting(
      "https://googlechrome.github.io/");

  std::vector<mojom::ServiceWorkerRegistrationDataPtr> registrations;
  std::vector<std::vector<ResourceRecordPtr>> resources_list;
  EXPECT_EQ(ServiceWorkerDatabase::Status::kOk,
            database->GetRegistrationsForStorageKey(key, &registrations,
                                                    &resources_list));

  EXPECT_FALSE(registrations.empty());
  EXPECT_FALSE(resources_list.empty());
}

TEST(ServiceWorkerDatabaseTest, NoFetchHandlerType) {
  std::unique_ptr<ServiceWorkerDatabase> database(CreateDatabaseInMemory());

  ServiceWorkerRegistrationData data;
  data.set_registration_id(1);
  data.set_scope_url("https://example.com");
  data.set_script_url("https://example.com/sw");
  data.set_version_id(1);
  data.set_is_active(true);
  data.set_last_update_check_time(
      base::Time::Now().ToDeltaSinceWindowsEpoch().InMicroseconds());

  database->next_avail_registration_id_ = 2;
  database->next_avail_version_id_ = 2;

  blink::StorageKey key =
      blink::StorageKey::CreateFromStringForTesting(data.scope_url());

  {
    // has_fetch_handler = true.
    data.set_has_fetch_handler(true);

    // Write the serialization.
    std::string value;
    ASSERT_TRUE(data.SerializeToString(&value));

    // Parse the serialized data. The kNotSkippable if has_fetch_handler is true
    // and no fetch_handler_type.
    RegistrationDataPtr registration;
    ASSERT_EQ(ServiceWorkerDatabase::Status::kOk,
              database->ParseRegistrationData(value, key, &registration));
    EXPECT_EQ(blink::mojom::ServiceWorkerFetchHandlerType::kNotSkippable,
              registration->fetch_handler_type);
  }

  {
    // has_fetch_handler = false.
    data.set_has_fetch_handler(false);

    // Write the serialization.
    std::string value;
    ASSERT_TRUE(data.SerializeToString(&value));

    // Parse the serialized data. The kNoHandler if has_fetch_handler is
    // false and no fetch_handler_type.
    RegistrationDataPtr registration;
    ASSERT_EQ(ServiceWorkerDatabase::Status::kOk,
              database->ParseRegistrationData(value, key, &registration));
    EXPECT_EQ(blink::mojom::ServiceWorkerFetchHandlerType::kNoHandler,
              registration->fetch_handler_type);
  }
}

TEST(ServiceWorkerDatabaseTest, FetchHandlerType) {
  std::unique_ptr<ServiceWorkerDatabase> database(CreateDatabaseInMemory());

  ServiceWorkerRegistrationData data;
  data.set_registration_id(1);
  data.set_scope_url("https://example.com");
  data.set_script_url("https://example.com/sw");
  data.set_version_id(1);
  data.set_is_active(true);
  data.set_has_fetch_handler(true);
  data.set_last_update_check_time(
      base::Time::Now().ToDeltaSinceWindowsEpoch().InMicroseconds());

  database->next_avail_registration_id_ = 2;
  database->next_avail_version_id_ = 2;

  blink::StorageKey key =
      blink::StorageKey::CreateFromStringForTesting(data.scope_url());

  {
    data.set_fetch_handler_skippable_type(
        ServiceWorkerRegistrationData::NOT_SKIPPABLE);
    // Write the serialization.
    std::string value;
    ASSERT_TRUE(data.SerializeToString(&value));

    RegistrationDataPtr registration;
    ASSERT_EQ(ServiceWorkerDatabase::Status::kOk,
              database->ParseRegistrationData(value, key, &registration));
    EXPECT_EQ(blink::mojom::ServiceWorkerFetchHandlerType::kNotSkippable,
              registration->fetch_handler_type);
  }

  {
    data.set_fetch_handler_skippable_type(
        ServiceWorkerRegistrationData::SKIPPABLE_EMPTY_FETCH_HANDLER);
    // Write the serialization.
    std::string value;
    ASSERT_TRUE(data.SerializeToString(&value));

    // Parse the serialized data. The policy is kNone if it's not set.
    RegistrationDataPtr registration;
    ASSERT_EQ(ServiceWorkerDatabase::Status::kOk,
              database->ParseRegistrationData(value, key, &registration));
    EXPECT_EQ(blink::mojom::ServiceWorkerFetchHandlerType::kEmptyFetchHandler,
              registration->fetch_handler_type);
  }
}

TEST(ServiceWorkerDatabaseTest, FetchHandlerTypeStoreRestore) {
  auto store_and_restore =
      [](blink::mojom::ServiceWorkerFetchHandlerType type) {
        GURL origin("https://example.com");
        RegistrationData data;
        data.registration_id = 123;
        data.scope = URL(origin, "/foo");
        data.key = blink::StorageKey(url::Origin::Create(data.scope));
        data.script = URL(origin, "/script.js");
        data.version_id = 456;
        data.fetch_handler_type = type;
        data.resources_total_size_bytes = 100;
        data.cross_origin_embedder_policy = CrossOriginEmbedderPolicyNone();
        std::vector<ResourceRecordPtr> resources;
        resources.push_back(CreateResource(1, data.script, 100));

        // Store.
        std::unique_ptr<ServiceWorkerDatabase> database(
            CreateDatabaseInMemory());
        ServiceWorkerDatabase::DeletedVersion deleted_version;
        ASSERT_EQ(
            ServiceWorkerDatabase::Status::kOk,
            database->WriteRegistration(data, resources, &deleted_version));

        // Restore.
        std::vector<mojom::ServiceWorkerRegistrationDataPtr> registrations;
        std::vector<std::vector<ResourceRecordPtr>> resources_list;
        EXPECT_EQ(ServiceWorkerDatabase::Status::kOk,
                  database->GetRegistrationsForStorageKey(
                      blink::StorageKey(url::Origin::Create(origin)),
                      &registrations, &resources_list));

        // The data must not have been altered.
        VerifyRegistrationData(data, *registrations[0]);
      };
  store_and_restore(blink::mojom::ServiceWorkerFetchHandlerType::kNoHandler);
  store_and_restore(blink::mojom::ServiceWorkerFetchHandlerType::kNotSkippable);
  store_and_restore(
      blink::mojom::ServiceWorkerFetchHandlerType::kEmptyFetchHandler);
}

}  // namespace storage
