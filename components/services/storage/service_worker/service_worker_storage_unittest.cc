// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/service_worker/service_worker_storage.h"

#include <stdint.h>

#include <memory>
#include <string>
#include <utility>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/net_errors.h"
#include "net/disk_cache/disk_cache.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace storage {
namespace service_worker_storage_unittest {

struct ReadResponseHeadResult {
  int result;
  network::mojom::URLResponseHeadPtr response_head;
  std::optional<mojo_base::BigBuffer> metadata;
};

using ResourceRecord = mojom::ServiceWorkerResourceRecordPtr;
using ResourceList = std::vector<mojom::ServiceWorkerResourceRecordPtr>;

ResourceRecord CreateResourceRecord(int64_t resource_id,
                                    const GURL& url,
                                    int64_t size_bytes) {
  EXPECT_TRUE(url.is_valid());
  return mojom::ServiceWorkerResourceRecord::New(resource_id, url, size_bytes,
                                                 /*sha256_checksum=*/"");
}

mojom::ServiceWorkerRegistrationDataPtr CreateRegistrationData(
    int64_t registration_id,
    int64_t version_id,
    const GURL& scope,
    const blink::StorageKey& key,
    const GURL& script_url,
    const std::vector<ResourceRecord>& resources) {
  auto data = mojom::ServiceWorkerRegistrationData::New();
  data->registration_id = registration_id;
  data->version_id = version_id;
  data->scope = scope;
  data->key = key;
  data->script = script_url;
  data->navigation_preload_state = blink::mojom::NavigationPreloadState::New();
  data->is_active = true;

  int64_t resources_total_size_bytes = 0;
  for (auto& resource : resources) {
    resources_total_size_bytes += resource->size_bytes;
  }
  data->resources_total_size_bytes = resources_total_size_bytes;

  return data;
}

void DatabaseStatusCallback(
    base::OnceClosure quit_closure,
    std::optional<ServiceWorkerDatabase::Status>* result,
    ServiceWorkerDatabase::Status status) {
  *result = status;
  std::move(quit_closure).Run();
}

class ServiceWorkerStorageTest : public testing::Test {
 public:
  ServiceWorkerStorageTest() = default;
  ~ServiceWorkerStorageTest() override = default;

  void SetUp() override {
    storage_ = ServiceWorkerStorage::Create(
        user_data_directory_path_,
        /*database_task_runner=*/base::SingleThreadTaskRunner::
            GetCurrentDefault());
  }

  void TearDown() override {
    storage_.reset();
    disk_cache::FlushCacheThreadForTesting();
    base::RunLoop().RunUntilIdle();
  }

  bool InitUserDataDirectory() {
    if (!user_data_directory_.CreateUniqueTempDir())
      return false;
    user_data_directory_path_ = user_data_directory_.GetPath();
    return true;
  }

  ServiceWorkerStorage* storage() { return storage_.get(); }

 protected:
  void LazyInitialize() { storage()->LazyInitializeForTest(); }

  ServiceWorkerDatabase::Status DeleteRegistration(
      int64_t registration_id,
      const blink::StorageKey& key) {
    ServiceWorkerDatabase::Status result;
    base::RunLoop loop;
    storage()->DeleteRegistration(
        registration_id, key,
        base::BindLambdaForTesting(
            [&](ServiceWorkerDatabase::Status status,
                ServiceWorkerStorage::StorageKeyState,
                int64_t /*deleted_version*/,
                uint64_t /*deleted_resources_size*/,
                const std::vector<int64_t>& /*newly_purgeable_resources*/) {
              result = status;
              loop.Quit();
            }));
    loop.Run();
    return result;
  }

  // Currently some Get/Find helper functions just return success/failure
  // status. This is because existing tests don't need actual values.

  ServiceWorkerDatabase::Status GetAllRegistrations() {
    ServiceWorkerDatabase::Status result;
    base::RunLoop loop;
    storage()->GetAllRegistrations(base::BindLambdaForTesting(
        [&](ServiceWorkerDatabase::Status status,
            std::unique_ptr<ServiceWorkerStorage::RegistrationList>) {
          result = status;
          loop.Quit();
        }));
    loop.Run();
    return result;
  }

  ServiceWorkerDatabase::Status GetUsageForStorageKey(
      const blink::StorageKey& key,
      int64_t& out_usage) {
    ServiceWorkerDatabase::Status result;
    base::RunLoop loop;
    storage()->GetUsageForStorageKey(
        key, base::BindLambdaForTesting(
                 [&](ServiceWorkerDatabase::Status status, int64_t usage) {
                   result = status;
                   out_usage = usage;
                   loop.Quit();
                 }));
    loop.Run();
    return result;
  }

  ServiceWorkerDatabase::Status GetRegistrationsForStorageKey(
      const blink::StorageKey& key) {
    ServiceWorkerDatabase::Status result;
    base::RunLoop loop;
    storage()->GetRegistrationsForStorageKey(
        key, base::BindLambdaForTesting(
                 [&](ServiceWorkerDatabase::Status status,
                     std::unique_ptr<ServiceWorkerStorage::RegistrationList>,
                     std::unique_ptr<std::vector<ResourceList>>) {
                   result = status;
                   loop.Quit();
                 }));
    loop.Run();
    return result;
  }

  ServiceWorkerDatabase::Status GetUserData(
      int64_t registration_id,
      const std::vector<std::string>& keys,
      std::vector<std::string>& out_data) {
    ServiceWorkerDatabase::Status result;
    base::RunLoop loop;
    storage()->GetUserData(
        registration_id, keys,
        base::BindLambdaForTesting([&](ServiceWorkerDatabase::Status status,
                                       const std::vector<std::string>& data) {
          result = status;
          out_data = data;
          loop.Quit();
        }));
    loop.Run();
    return result;
  }

  ServiceWorkerDatabase::Status GetUserDataByKeyPrefix(
      int64_t registration_id,
      const std::string& key_prefix,
      std::vector<std::string>& out_data) {
    ServiceWorkerDatabase::Status result;
    base::RunLoop loop;
    storage()->GetUserDataByKeyPrefix(
        registration_id, key_prefix,
        base::BindLambdaForTesting([&](ServiceWorkerDatabase::Status status,
                                       const std::vector<std::string>& data) {
          result = status;
          out_data = data;
          loop.Quit();
        }));
    loop.Run();
    return result;
  }

  ServiceWorkerDatabase::Status StoreUserData(
      int64_t registration_id,
      const blink::StorageKey& key,
      const std::vector<std::pair<std::string, std::string>>& key_value_pairs) {
    std::vector<mojom::ServiceWorkerUserDataPtr> user_data;
    for (const auto& kv : key_value_pairs) {
      user_data.push_back(mojom::ServiceWorkerUserData::New(
          registration_id, kv.first, kv.second));
    }

    ServiceWorkerDatabase::Status result;
    base::RunLoop loop;
    storage()->StoreUserData(
        registration_id, key, std::move(user_data),
        base::BindLambdaForTesting([&](ServiceWorkerDatabase::Status status) {
          result = status;
          loop.Quit();
        }));
    loop.Run();
    return result;
  }

  ServiceWorkerDatabase::Status ClearUserData(
      int64_t registration_id,
      const std::vector<std::string>& keys) {
    ServiceWorkerDatabase::Status result;
    base::RunLoop loop;
    storage()->ClearUserData(
        registration_id, keys,
        base::BindLambdaForTesting([&](ServiceWorkerDatabase::Status status) {
          result = status;
          loop.Quit();
        }));
    loop.Run();
    return result;
  }

  ServiceWorkerDatabase::Status ClearUserDataByKeyPrefixes(
      int64_t registration_id,
      const std::vector<std::string>& key_prefixes) {
    base::RunLoop loop;
    ServiceWorkerDatabase::Status result;
    storage()->ClearUserDataByKeyPrefixes(
        registration_id, key_prefixes,
        base::BindLambdaForTesting([&](ServiceWorkerDatabase::Status status) {
          result = status;
          loop.Quit();
        }));
    loop.Run();
    return result;
  }

  ServiceWorkerDatabase::Status GetUserDataForAllRegistrations(
      const std::string& key,
      std::vector<std::pair<int64_t, std::string>>& data) {
    ServiceWorkerDatabase::Status result;
    base::RunLoop loop;
    storage()->GetUserDataForAllRegistrations(
        key, base::BindLambdaForTesting(
                 [&](ServiceWorkerDatabase::Status status,
                     std::vector<mojom::ServiceWorkerUserDataPtr> entries) {
                   result = status;
                   for (auto& entry : entries) {
                     data.emplace_back(entry->registration_id, entry->value);
                   }
                   loop.Quit();
                 }));
    loop.Run();
    return result;
  }

  ServiceWorkerDatabase::Status ClearUserDataForAllRegistrationsByKeyPrefix(
      const std::string& key_prefix) {
    ServiceWorkerDatabase::Status result;
    base::RunLoop loop;
    storage()->ClearUserDataForAllRegistrationsByKeyPrefix(
        key_prefix,
        base::BindLambdaForTesting([&](ServiceWorkerDatabase::Status status) {
          result = status;
          loop.Quit();
        }));
    loop.Run();
    return result;
  }

  ServiceWorkerDatabase::Status UpdateToActiveState(
      int64_t registration_id,
      const blink::StorageKey& key) {
    ServiceWorkerDatabase::Status result;
    base::RunLoop loop;
    storage()->UpdateToActiveState(
        registration_id, key,
        base::BindLambdaForTesting([&](ServiceWorkerDatabase::Status status) {
          result = status;
          loop.Quit();
        }));
    loop.Run();
    return result;
  }

  ServiceWorkerDatabase::Status UpdateFetchHandlerType(
      int64_t registration_id,
      const blink::StorageKey& key,
      blink::mojom::ServiceWorkerFetchHandlerType fetch_handler_type) {
    ServiceWorkerDatabase::Status result;
    base::RunLoop loop;
    storage()->UpdateFetchHandlerType(
        registration_id, key, fetch_handler_type,
        base::BindLambdaForTesting([&](ServiceWorkerDatabase::Status status) {
          result = status;
          loop.Quit();
        }));
    loop.Run();
    return result;
  }

  ServiceWorkerDatabase::Status UpdateResourceSha256Checksums(
      int64_t registration_id,
      const blink::StorageKey& key,
      const base::flat_map<int64_t, std::string>& updated_sha256_checksums) {
    ServiceWorkerDatabase::Status result;
    base::RunLoop loop;
    storage()->UpdateResourceSha256Checksums(
        registration_id, key, updated_sha256_checksums,
        base::BindLambdaForTesting([&](ServiceWorkerDatabase::Status status) {
          result = status;
          loop.Quit();
        }));
    loop.Run();
    return result;
  }

  ServiceWorkerDatabase::Status FindRegistrationForClientUrl(
      const GURL& document_url,
      const blink::StorageKey& key) {
    ServiceWorkerDatabase::Status result;
    base::RunLoop loop;
    storage()->FindRegistrationForClientUrl(
        document_url, key,
        base::BindLambdaForTesting(
            [&](mojom::ServiceWorkerRegistrationDataPtr,
                std::unique_ptr<ResourceList>,
                const std::optional<std::vector<GURL>>& scopes,
                ServiceWorkerDatabase::Status status) {
              result = status;
              loop.Quit();
            }));
    loop.Run();
    return result;
  }

  ServiceWorkerDatabase::Status FindRegistrationForScope(
      const GURL& scope,
      const blink::StorageKey& key) {
    ServiceWorkerDatabase::Status result;
    base::RunLoop loop;
    storage()->FindRegistrationForScope(
        scope, key,
        base::BindLambdaForTesting([&](mojom::ServiceWorkerRegistrationDataPtr,
                                       std::unique_ptr<ResourceList>,
                                       ServiceWorkerDatabase::Status status) {
          result = status;
          loop.Quit();
        }));
    loop.Run();
    return result;
  }

  ServiceWorkerDatabase::Status FindRegistrationForId(
      int64_t registration_id,
      const blink::StorageKey& key) {
    ServiceWorkerDatabase::Status result;
    base::RunLoop loop;
    storage()->FindRegistrationForId(
        registration_id, key,
        base::BindLambdaForTesting([&](mojom::ServiceWorkerRegistrationDataPtr,
                                       std::unique_ptr<ResourceList>,
                                       ServiceWorkerDatabase::Status status) {
          result = status;
          loop.Quit();
        }));
    loop.Run();
    return result;
  }

  ServiceWorkerDatabase::Status FindRegistrationForIdOnly(
      int64_t registration_id) {
    ServiceWorkerDatabase::Status result;
    base::RunLoop loop;
    storage()->FindRegistrationForIdOnly(
        registration_id,
        base::BindLambdaForTesting([&](mojom::ServiceWorkerRegistrationDataPtr,
                                       std::unique_ptr<ResourceList>,
                                       ServiceWorkerDatabase::Status status) {
          result = status;
          loop.Quit();
        }));
    loop.Run();
    return result;
  }

  int64_t GetNewRegistrationId() {
    int64_t result;
    base::RunLoop loop;
    storage()->GetNewRegistrationId(base::BindLambdaForTesting([&](int64_t id) {
      result = id;
      loop.Quit();
    }));
    loop.Run();
    return result;
  }

  int64_t GetNewVersionId() {
    int64_t result;
    base::RunLoop loop;
    storage()->GetNewVersionId(base::BindLambdaForTesting([&](int64_t id) {
      result = id;
      loop.Quit();
    }));
    loop.Run();
    return result;
  }

  int64_t GetNewResourceId() {
    int64_t result;
    base::RunLoop loop;
    storage()->GetNewResourceId(base::BindLambdaForTesting([&](int64_t id) {
      result = id;
      loop.Quit();
    }));
    loop.Run();
    return result;
  }

  ReadResponseHeadResult ReadResponseHead(int64_t id) {
    ReadResponseHeadResult out;
    base::RunLoop loop;

    mojo::Remote<mojom::ServiceWorkerResourceReader> reader;
    storage()->CreateResourceReader(id, reader.BindNewPipeAndPassReceiver());
    reader.set_disconnect_handler(base::BindLambdaForTesting([&]() {
      out.result = net::ERR_CACHE_MISS;
      loop.Quit();
    }));

    reader->ReadResponseHead(base::BindLambdaForTesting(
        [&](int result, network::mojom::URLResponseHeadPtr response_head,
            std::optional<mojo_base::BigBuffer> metadata) {
          out.result = result;
          out.response_head = std::move(response_head);
          out.metadata = std::move(metadata);
          loop.Quit();
        }));
    loop.Run();
    return out;
  }

  int WriteBasicResponse(int64_t id) {
    const std::string kHttpHeaders =
        "HTTP/1.0 200 HONKYDORY\0Content-Length: 5\0\0";
    const std::string kHttpBody = "Hello";

    std::string headers(kHttpHeaders, std::size(kHttpHeaders));
    mojo_base::BigBuffer body(base::as_byte_span(kHttpBody));

    mojo::Remote<mojom::ServiceWorkerResourceWriter> writer;
    storage()->CreateResourceWriter(id, writer.BindNewPipeAndPassReceiver());

    int rv = 0;
    {
      auto response_head = network::mojom::URLResponseHead::New();
      response_head->request_time = base::Time::Now();
      response_head->response_time = base::Time::Now();
      response_head->headers = new net::HttpResponseHeaders(headers);
      response_head->content_length = body.size();

      base::RunLoop loop;
      writer.set_disconnect_handler(base::BindLambdaForTesting([&]() {
        rv = net::ERR_FAILED;
        loop.Quit();
      }));
      writer->WriteResponseHead(std::move(response_head),
                                base::BindLambdaForTesting([&](int result) {
                                  rv = result;
                                  loop.Quit();
                                }));
      loop.Run();
      if (rv < 0)
        return rv;
    }

    {
      base::RunLoop loop;
      writer->WriteData(std::move(body),
                        base::BindLambdaForTesting([&](int result) {
                          rv = result;
                          loop.Quit();
                        }));
      loop.Run();
    }

    return rv;
  }

  int WriteResponseMetadata(int64_t id, const std::string& metadata) {
    mojo_base::BigBuffer buffer(base::as_byte_span(metadata));

    mojo::Remote<mojom::ServiceWorkerResourceMetadataWriter> metadata_writer;
    storage()->CreateResourceMetadataWriter(
        id, metadata_writer.BindNewPipeAndPassReceiver());

    int rv = 0;
    base::RunLoop loop;
    metadata_writer.set_disconnect_handler(base::BindLambdaForTesting([&]() {
      rv = net::ERR_FAILED;
      loop.Quit();
    }));
    metadata_writer->WriteMetadata(std::move(buffer),
                                   base::BindLambdaForTesting([&](int result) {
                                     rv = result;
                                     loop.Quit();
                                   }));
    loop.Run();
    return rv;
  }

  ServiceWorkerDatabase::Status StoreRegistrationData(
      mojom::ServiceWorkerRegistrationDataPtr registration_data,
      std::vector<ResourceRecord> resources) {
    ServiceWorkerDatabase::Status result;
    base::RunLoop loop;
    storage()->StoreRegistrationData(
        std::move(registration_data), std::move(resources),
        base::BindLambdaForTesting(
            [&](mojom::ServiceWorkerDatabaseStatus status,
                int64_t /*deleted_version_id*/,
                uint64_t /*deleted_resources_size*/,
                const std::vector<int64_t>& /*newly_purgeable_resources*/) {
              result = status;
              loop.Quit();
            }));
    loop.Run();
    return result;
  }

  // user_data_directory_ must be declared first to preserve destructor order.
  base::ScopedTempDir user_data_directory_;
  base::FilePath user_data_directory_path_;
  std::unique_ptr<ServiceWorkerStorage> storage_;
  base::test::TaskEnvironment task_environment_;
};

TEST_F(ServiceWorkerStorageTest, DisabledStorage) {
  const GURL kScope("http://www.example.com/scope/");
  const url::Origin kOrigin = url::Origin::Create(kScope);
  const blink::StorageKey kKey = blink::StorageKey::CreateFirstParty(kOrigin);
  const GURL kScript("http://www.example.com/script.js");
  const GURL kDocumentUrl("http://www.example.com/scope/document.html");
  const int64_t kRegistrationId = 0;
  const int64_t kRegistrationId2 = 1;
  const int64_t kVersionId = 0;
  const int64_t kResourceId = 0;

  LazyInitialize();
  storage()->Disable();

  EXPECT_EQ(FindRegistrationForClientUrl(kDocumentUrl, kKey),
            ServiceWorkerDatabase::Status::kErrorDisabled);
  EXPECT_EQ(FindRegistrationForScope(kScope, kKey),
            ServiceWorkerDatabase::Status::kErrorDisabled);
  EXPECT_EQ(FindRegistrationForId(kRegistrationId, kKey),
            ServiceWorkerDatabase::Status::kErrorDisabled);
  EXPECT_EQ(FindRegistrationForIdOnly(kRegistrationId),
            ServiceWorkerDatabase::Status::kErrorDisabled);

  EXPECT_EQ(GetRegistrationsForStorageKey(kKey),
            ServiceWorkerDatabase::Status::kErrorDisabled);

  EXPECT_EQ(GetAllRegistrations(),
            ServiceWorkerDatabase::Status::kErrorDisabled);

  std::vector<ResourceRecord> resources;
  resources.push_back(CreateResourceRecord(kResourceId, kScript, 100));
  mojom::ServiceWorkerRegistrationDataPtr registration_data =
      CreateRegistrationData(kRegistrationId, kVersionId, kScope, kKey, kScript,
                             resources);
  EXPECT_EQ(
      StoreRegistrationData(std::move(registration_data), std::move(resources)),
      ServiceWorkerDatabase::Status::kErrorDisabled);

  EXPECT_EQ(UpdateToActiveState(kRegistrationId, kKey),
            ServiceWorkerDatabase::Status::kErrorDisabled);

  EXPECT_EQ(UpdateFetchHandlerType(
                kRegistrationId, kKey,
                blink::mojom::ServiceWorkerFetchHandlerType::kNotSkippable),
            ServiceWorkerDatabase::Status::kErrorDisabled);

  std::vector<ResourceRecord> resources2;
  resources2.push_back(CreateResourceRecord(kResourceId, kScript, 100));
  CreateRegistrationData(kRegistrationId2, kVersionId, kScope, kKey, kScript,
                         resources2);
  EXPECT_EQ(
      UpdateResourceSha256Checksums(kRegistrationId2, kKey,
                                    base::flat_map<int64_t, std::string>(
                                        {{resources2[0]->resource_id, ""}})),
      ServiceWorkerDatabase::Status::kErrorDisabled);

  EXPECT_EQ(DeleteRegistration(kRegistrationId, kKey),
            ServiceWorkerDatabase::Status::kErrorDisabled);

  // Response reader and writer created by the disabled storage should fail to
  // access the disk cache.
  ReadResponseHeadResult out = ReadResponseHead(kResourceId);
  EXPECT_EQ(out.result, net::ERR_CACHE_MISS);
  EXPECT_EQ(WriteBasicResponse(kResourceId), net::ERR_FAILED);
  EXPECT_EQ(WriteResponseMetadata(kResourceId, "foo"), net::ERR_FAILED);

  const std::string kUserDataKey = "key";
  std::vector<std::string> user_data_out;
  EXPECT_EQ(GetUserData(kRegistrationId, {kUserDataKey}, user_data_out),
            ServiceWorkerDatabase::Status::kErrorDisabled);
  EXPECT_EQ(GetUserDataByKeyPrefix(kRegistrationId, "prefix", user_data_out),
            ServiceWorkerDatabase::Status::kErrorDisabled);
  EXPECT_EQ(StoreUserData(kRegistrationId, kKey, {{kUserDataKey, "foo"}}),
            ServiceWorkerDatabase::Status::kErrorDisabled);
  EXPECT_EQ(ClearUserData(kRegistrationId, {kUserDataKey}),
            ServiceWorkerDatabase::Status::kErrorDisabled);
  EXPECT_EQ(ClearUserDataByKeyPrefixes(kRegistrationId, {"prefix"}),
            ServiceWorkerDatabase::Status::kErrorDisabled);
  std::vector<std::pair<int64_t, std::string>> data_list_out;
  EXPECT_EQ(GetUserDataForAllRegistrations(kUserDataKey, data_list_out),
            ServiceWorkerDatabase::Status::kErrorDisabled);
  EXPECT_EQ(ClearUserDataForAllRegistrationsByKeyPrefix("prefix"),
            ServiceWorkerDatabase::Status::kErrorDisabled);

  // Next available ids should be invalid.
  EXPECT_EQ(blink::mojom::kInvalidServiceWorkerRegistrationId,
            GetNewRegistrationId());
  EXPECT_EQ(blink::mojom::kInvalidServiceWorkerVersionId, GetNewVersionId());
  EXPECT_EQ(blink::mojom::kInvalidServiceWorkerResourceId, GetNewResourceId());
}

TEST_F(ServiceWorkerStorageTest, StoreUserData) {
  const int64_t kRegistrationId = 1;
  const GURL kScope("http://www.test.not/scope/");
  const url::Origin kOrigin = url::Origin::Create(kScope);
  const blink::StorageKey kKey = blink::StorageKey::CreateFirstParty(kOrigin);
  const GURL kScript("http://www.test.not/script.js");
  LazyInitialize();

  // Store a registration.
  std::vector<ResourceRecord> resources;
  resources.push_back(CreateResourceRecord(1, kScript, 100));
  mojom::ServiceWorkerRegistrationDataPtr registration_data =
      CreateRegistrationData(kRegistrationId,
                             /*version_id=*/1, kScope, kKey, kScript,
                             resources);
  ASSERT_EQ(
      StoreRegistrationData(std::move(registration_data), std::move(resources)),
      ServiceWorkerDatabase::Status::kOk);

  // Store user data associated with the registration.
  std::vector<std::string> data_out;
  EXPECT_EQ(StoreUserData(kRegistrationId, kKey, {{"key", "data"}}),
            ServiceWorkerDatabase::Status::kOk);
  EXPECT_EQ(GetUserData(kRegistrationId, {"key"}, data_out),
            ServiceWorkerDatabase::Status::kOk);
  ASSERT_EQ(1u, data_out.size());
  EXPECT_EQ("data", data_out[0]);
  EXPECT_EQ(GetUserData(kRegistrationId, {"unknown_key"}, data_out),
            ServiceWorkerDatabase::Status::kErrorNotFound);
  std::vector<std::pair<int64_t, std::string>> data_list_out;
  EXPECT_EQ(GetUserDataForAllRegistrations("key", data_list_out),
            ServiceWorkerDatabase::Status::kOk);
  ASSERT_EQ(1u, data_list_out.size());
  EXPECT_EQ(kRegistrationId, data_list_out[0].first);
  EXPECT_EQ("data", data_list_out[0].second);
  data_list_out.clear();
  EXPECT_EQ(GetUserDataForAllRegistrations("unknown_key", data_list_out),
            ServiceWorkerDatabase::Status::kOk);
  EXPECT_EQ(0u, data_list_out.size());
  EXPECT_EQ(ClearUserData(kRegistrationId, {"key"}),
            ServiceWorkerDatabase::Status::kOk);
  EXPECT_EQ(GetUserData(kRegistrationId, {"key"}, data_out),
            ServiceWorkerDatabase::Status::kErrorNotFound);

  // Write/overwrite multiple user data keys.
  EXPECT_EQ(StoreUserData(
                kRegistrationId, kKey,
                {{"key", "overwrite"}, {"key3", "data3"}, {"key4", "data4"}}),
            ServiceWorkerDatabase::Status::kOk);
  EXPECT_EQ(GetUserData(kRegistrationId, {"key2"}, data_out),
            ServiceWorkerDatabase::Status::kErrorNotFound);
  EXPECT_TRUE(data_out.empty());
  EXPECT_EQ(GetUserData(kRegistrationId, {"key", "key3", "key4"}, data_out),
            ServiceWorkerDatabase::Status::kOk);
  ASSERT_EQ(3u, data_out.size());
  EXPECT_EQ("overwrite", data_out[0]);
  EXPECT_EQ("data3", data_out[1]);
  EXPECT_EQ("data4", data_out[2]);
  // Multiple gets fail if one is not found.
  EXPECT_EQ(GetUserData(kRegistrationId, {"key", "key2"}, data_out),
            ServiceWorkerDatabase::Status::kErrorNotFound);
  EXPECT_TRUE(data_out.empty());

  // Delete multiple user data keys, even if some are not found.
  EXPECT_EQ(ClearUserData(kRegistrationId, {"key", "key2", "key3"}),
            ServiceWorkerDatabase::Status::kOk);
  EXPECT_EQ(GetUserData(kRegistrationId, {"key"}, data_out),
            ServiceWorkerDatabase::Status::kErrorNotFound);
  EXPECT_EQ(GetUserData(kRegistrationId, {"key2"}, data_out),
            ServiceWorkerDatabase::Status::kErrorNotFound);
  EXPECT_EQ(GetUserData(kRegistrationId, {"key3"}, data_out),
            ServiceWorkerDatabase::Status::kErrorNotFound);
  EXPECT_EQ(GetUserData(kRegistrationId, {"key4"}, data_out),
            ServiceWorkerDatabase::Status::kOk);
  ASSERT_EQ(1u, data_out.size());
  EXPECT_EQ("data4", data_out[0]);

  // Get/delete multiple user data keys by prefixes.
  EXPECT_EQ(StoreUserData(kRegistrationId, kKey,
                          {{"prefixA", "data1"},
                           {"prefixA2", "data2"},
                           {"prefixB", "data3"},
                           {"prefixC", "data4"}}),
            ServiceWorkerDatabase::Status::kOk);
  EXPECT_EQ(GetUserDataByKeyPrefix(kRegistrationId, "prefix", data_out),
            ServiceWorkerDatabase::Status::kOk);
  ASSERT_EQ(4u, data_out.size());
  EXPECT_EQ("data1", data_out[0]);
  EXPECT_EQ("data2", data_out[1]);
  EXPECT_EQ("data3", data_out[2]);
  EXPECT_EQ("data4", data_out[3]);
  EXPECT_EQ(ClearUserDataByKeyPrefixes(kRegistrationId, {"prefixA", "prefixC"}),
            ServiceWorkerDatabase::Status::kOk);
  EXPECT_EQ(GetUserDataByKeyPrefix(kRegistrationId, "prefix", data_out),
            ServiceWorkerDatabase::Status::kOk);
  ASSERT_EQ(1u, data_out.size());
  EXPECT_EQ("data3", data_out[0]);

  EXPECT_EQ(ClearUserDataForAllRegistrationsByKeyPrefix("prefixB"),
            ServiceWorkerDatabase::Status::kOk);
  EXPECT_EQ(GetUserDataByKeyPrefix(kRegistrationId, "prefix", data_out),
            ServiceWorkerDatabase::Status::kOk);
  EXPECT_TRUE(data_out.empty());

  // User data should be deleted when the associated registration is deleted.
  ASSERT_EQ(StoreUserData(kRegistrationId, kKey, {{"key", "data"}}),
            ServiceWorkerDatabase::Status::kOk);
  ASSERT_EQ(GetUserData(kRegistrationId, {"key"}, data_out),
            ServiceWorkerDatabase::Status::kOk);
  ASSERT_EQ(1u, data_out.size());
  ASSERT_EQ("data", data_out[0]);

  EXPECT_EQ(DeleteRegistration(kRegistrationId, kKey),
            ServiceWorkerDatabase::Status::kOk);
  EXPECT_EQ(GetUserData(kRegistrationId, {"key"}, data_out),
            ServiceWorkerDatabase::Status::kErrorNotFound);
  data_list_out.clear();
  EXPECT_EQ(GetUserDataForAllRegistrations("key", data_list_out),
            ServiceWorkerDatabase::Status::kOk);
  EXPECT_EQ(0u, data_list_out.size());

  // Data access with an invalid registration id should be failed.
  EXPECT_EQ(StoreUserData(blink::mojom::kInvalidServiceWorkerRegistrationId,
                          kKey, {{"key", "data"}}),
            ServiceWorkerDatabase::Status::kErrorFailed);
  EXPECT_EQ(GetUserData(blink::mojom::kInvalidServiceWorkerRegistrationId,
                        {"key"}, data_out),
            ServiceWorkerDatabase::Status::kErrorFailed);
  EXPECT_EQ(
      GetUserDataByKeyPrefix(blink::mojom::kInvalidServiceWorkerRegistrationId,
                             "prefix", data_out),
      ServiceWorkerDatabase::Status::kErrorFailed);
  EXPECT_EQ(
      ClearUserData(blink::mojom::kInvalidServiceWorkerRegistrationId, {"key"}),
      ServiceWorkerDatabase::Status::kErrorFailed);
  EXPECT_EQ(ClearUserDataByKeyPrefixes(
                blink::mojom::kInvalidServiceWorkerRegistrationId, {"prefix"}),
            ServiceWorkerDatabase::Status::kErrorFailed);

  // Data access with an empty key should be failed.
  EXPECT_EQ(StoreUserData(kRegistrationId, kKey,
                          std::vector<std::pair<std::string, std::string>>()),
            ServiceWorkerDatabase::Status::kErrorFailed);
  EXPECT_EQ(StoreUserData(kRegistrationId, kKey, {{std::string(), "data"}}),
            ServiceWorkerDatabase::Status::kErrorFailed);
  EXPECT_EQ(StoreUserData(kRegistrationId, kKey,
                          {{std::string(), "data"}, {"key", "data"}}),
            ServiceWorkerDatabase::Status::kErrorFailed);
  EXPECT_EQ(GetUserData(kRegistrationId, std::vector<std::string>(), data_out),
            ServiceWorkerDatabase::Status::kErrorFailed);
  EXPECT_EQ(GetUserDataByKeyPrefix(kRegistrationId, std::string(), data_out),
            ServiceWorkerDatabase::Status::kErrorFailed);
  EXPECT_EQ(GetUserData(kRegistrationId, {std::string()}, data_out),
            ServiceWorkerDatabase::Status::kErrorFailed);
  EXPECT_EQ(GetUserData(kRegistrationId, {std::string(), "key"}, data_out),
            ServiceWorkerDatabase::Status::kErrorFailed);
  EXPECT_EQ(ClearUserData(kRegistrationId, std::vector<std::string>()),
            ServiceWorkerDatabase::Status::kErrorFailed);
  EXPECT_EQ(ClearUserData(kRegistrationId, {std::string()}),
            ServiceWorkerDatabase::Status::kErrorFailed);
  EXPECT_EQ(ClearUserData(kRegistrationId, {std::string(), "key"}),
            ServiceWorkerDatabase::Status::kErrorFailed);
  EXPECT_EQ(ClearUserDataByKeyPrefixes(kRegistrationId, {}),
            ServiceWorkerDatabase::Status::kErrorFailed);
  EXPECT_EQ(ClearUserDataByKeyPrefixes(kRegistrationId, {std::string()}),
            ServiceWorkerDatabase::Status::kErrorFailed);
  EXPECT_EQ(ClearUserDataForAllRegistrationsByKeyPrefix(std::string()),
            ServiceWorkerDatabase::Status::kErrorFailed);
  data_list_out.clear();
  EXPECT_EQ(GetUserDataForAllRegistrations(std::string(), data_list_out),
            ServiceWorkerDatabase::Status::kErrorFailed);
}

// The *_BeforeInitialize tests exercise the API before LazyInitialize() is
// called.
TEST_F(ServiceWorkerStorageTest, StoreUserData_BeforeInitialize) {
  const int kRegistrationId = 0;
  EXPECT_EQ(StoreUserData(kRegistrationId,
                          blink::StorageKey::CreateFirstParty(
                              url::Origin::Create(GURL("https://example.com"))),
                          {{"key", "data"}}),
            ServiceWorkerDatabase::Status::kErrorNotFound);
}

TEST_F(ServiceWorkerStorageTest, GetUserData_BeforeInitialize) {
  const int kRegistrationId = 0;
  std::vector<std::string> data_out;
  EXPECT_EQ(GetUserData(kRegistrationId, {"key"}, data_out),
            ServiceWorkerDatabase::Status::kErrorNotFound);
  EXPECT_EQ(GetUserDataByKeyPrefix(kRegistrationId, "prefix", data_out),
            ServiceWorkerDatabase::Status::kErrorNotFound);
}

TEST_F(ServiceWorkerStorageTest, ClearUserData_BeforeInitialize) {
  const int kRegistrationId = 0;
  EXPECT_EQ(ClearUserData(kRegistrationId, {"key"}),
            ServiceWorkerDatabase::Status::kOk);
  EXPECT_EQ(ClearUserDataByKeyPrefixes(kRegistrationId, {"prefix"}),
            ServiceWorkerDatabase::Status::kOk);
  EXPECT_EQ(ClearUserDataForAllRegistrationsByKeyPrefix("key"),
            ServiceWorkerDatabase::Status::kOk);
}

TEST_F(ServiceWorkerStorageTest,
       GetUserDataForAllRegistrations_BeforeInitialize) {
  std::vector<std::pair<int64_t, std::string>> data_list_out;
  EXPECT_EQ(GetUserDataForAllRegistrations("key", data_list_out),
            ServiceWorkerDatabase::Status::kOk);
  EXPECT_TRUE(data_list_out.empty());
}

// Test fixture that uses disk storage, rather than memory. Useful for tests
// that test persistence by simulating browser shutdown and restart.
class ServiceWorkerStorageDiskTest : public ServiceWorkerStorageTest {
 public:
  ServiceWorkerStorageDiskTest() = default;
  ~ServiceWorkerStorageDiskTest() override = default;

  void SetUp() override {
    ASSERT_TRUE(InitUserDataDirectory());
    ServiceWorkerStorageTest::SetUp();
    LazyInitialize();

    // Store a registration with a resource to make sure disk cache and
    // database directories are created.
    const GURL kScope("http://www.example.com/scope/");
    const blink::StorageKey kKey =
        blink::StorageKey::CreateFirstParty(url::Origin::Create(kScope));
    const GURL kScript("http://www.example.com/script.js");
    const int64_t kScriptSize = 5;
    auto data = mojom::ServiceWorkerRegistrationData::New();
    data->registration_id = 1;
    data->version_id = 1;
    data->scope = kScope;
    data->key = kKey;
    data->script = kScript;
    data->navigation_preload_state =
        blink::mojom::NavigationPreloadState::New();
    data->resources_total_size_bytes = kScriptSize;

    std::vector<ResourceRecord> resources;
    resources.push_back(CreateResourceRecord(1, kScript, kScriptSize));

    ASSERT_EQ(StoreRegistrationData(std::move(data), std::move(resources)),
              ServiceWorkerDatabase::Status::kOk);
    WriteBasicResponse(1);
  }
};

TEST_F(ServiceWorkerStorageDiskTest, DeleteAndStartOver) {
  EXPECT_FALSE(storage()->IsDisabled());
  ASSERT_TRUE(base::DirectoryExists(storage()->GetDiskCachePath()));
  ASSERT_TRUE(base::DirectoryExists(storage()->GetDatabasePath()));

  base::RunLoop run_loop;
  std::optional<ServiceWorkerDatabase::Status> status;
  storage()->DeleteAndStartOver(
      base::BindOnce(&DatabaseStatusCallback, run_loop.QuitClosure(), &status));
  run_loop.Run();

  EXPECT_EQ(ServiceWorkerDatabase::Status::kOk, *status);
  EXPECT_TRUE(storage()->IsDisabled());
  EXPECT_FALSE(base::DirectoryExists(storage()->GetDiskCachePath()));
  EXPECT_FALSE(base::DirectoryExists(storage()->GetDatabasePath()));
}

TEST_F(ServiceWorkerStorageDiskTest, DeleteAndStartOver_UnrelatedFileExists) {
  EXPECT_FALSE(storage()->IsDisabled());
  ASSERT_TRUE(base::DirectoryExists(storage()->GetDiskCachePath()));
  ASSERT_TRUE(base::DirectoryExists(storage()->GetDatabasePath()));

  // Create an unrelated file in the database directory to make sure such a file
  // does not prevent DeleteAndStartOver.
  base::FilePath file_path;
  ASSERT_TRUE(
      base::CreateTemporaryFileInDir(storage()->GetDatabasePath(), &file_path));
  ASSERT_TRUE(base::PathExists(file_path));

  base::RunLoop run_loop;
  std::optional<ServiceWorkerDatabase::Status> status;
  storage()->DeleteAndStartOver(
      base::BindOnce(&DatabaseStatusCallback, run_loop.QuitClosure(), &status));
  run_loop.Run();

  EXPECT_EQ(ServiceWorkerDatabase::Status::kOk, *status);
  EXPECT_TRUE(storage()->IsDisabled());
  EXPECT_FALSE(base::DirectoryExists(storage()->GetDiskCachePath()));
  EXPECT_FALSE(base::DirectoryExists(storage()->GetDatabasePath()));
}

TEST_F(ServiceWorkerStorageDiskTest, DeleteAndStartOver_OpenedFileExists) {
  EXPECT_FALSE(storage()->IsDisabled());
  ASSERT_TRUE(base::DirectoryExists(storage()->GetDiskCachePath()));
  ASSERT_TRUE(base::DirectoryExists(storage()->GetDatabasePath()));

  // Create an unrelated opened file in the database directory to make sure such
  // a file does not prevent DeleteAndStartOver on non-Windows platforms.
  base::FilePath file_path;
  base::ScopedFILE file = base::CreateAndOpenTemporaryStreamInDir(
      storage()->GetDatabasePath(), &file_path);
  ASSERT_TRUE(file);
  ASSERT_TRUE(base::PathExists(file_path));

  base::RunLoop run_loop;
  std::optional<ServiceWorkerDatabase::Status> status;
  storage()->DeleteAndStartOver(
      base::BindOnce(&DatabaseStatusCallback, run_loop.QuitClosure(), &status));
  run_loop.Run();

#if BUILDFLAG(IS_WIN)
  // On Windows, deleting the directory containing an opened file should fail.
  EXPECT_EQ(ServiceWorkerDatabase::Status::kErrorIOError, *status);
  EXPECT_TRUE(storage()->IsDisabled());
  EXPECT_TRUE(base::DirectoryExists(storage()->GetDiskCachePath()));
  EXPECT_TRUE(base::DirectoryExists(storage()->GetDatabasePath()));
#else
  EXPECT_EQ(ServiceWorkerDatabase::Status::kOk, *status);
  EXPECT_TRUE(storage()->IsDisabled());
  EXPECT_FALSE(base::DirectoryExists(storage()->GetDiskCachePath()));
  EXPECT_FALSE(base::DirectoryExists(storage()->GetDatabasePath()));
#endif
}

// Tests reading storage usage from database.
TEST_F(ServiceWorkerStorageTest, GetStorageUsageForOrigin) {
  const int64_t kRegistrationId1 = 1;
  const GURL kScope1("https://www.example.com/foo/");
  const blink::StorageKey kKey1 =
      blink::StorageKey::CreateFirstParty(url::Origin::Create(kScope1));
  const GURL kScript1("https://www.example.com/foo/sw.js");
  const int64_t kRegistrationId2 = 2;
  const GURL kScope2("https://www.example.com/bar/");
  const GURL kScript2("https://www.example.com/bar/sw.js");
  const GURL kScript3("https://www.example.com/bar/sub.js");

  // Preparation: Store two registrations.
  std::vector<ResourceRecord> resources1;
  resources1.push_back(CreateResourceRecord(1, kScript1, 123));
  mojom::ServiceWorkerRegistrationDataPtr data1 = CreateRegistrationData(
      /*registration_id=*/kRegistrationId1,
      /*version_id=*/1,
      /*scope=*/kScope1,
      /*key=*/kKey1,
      /*script_url=*/kScript1, resources1);
  int64_t resources_total_size_bytes1 = data1->resources_total_size_bytes;
  ASSERT_EQ(StoreRegistrationData(std::move(data1), std::move(resources1)),
            ServiceWorkerDatabase::Status::kOk);

  std::vector<ResourceRecord> resources2;
  resources2.push_back(CreateResourceRecord(2, kScript2, 456));
  resources2.push_back(CreateResourceRecord(3, kScript3, 789));
  mojom::ServiceWorkerRegistrationDataPtr data2 = CreateRegistrationData(
      /*registration_id=*/kRegistrationId2,
      /*version_id=*/1,
      /*scope=*/kScope1,
      /*key=*/kKey1,
      /*script_url=*/kScript2, resources2);
  int64_t resources_total_size_bytes2 = data2->resources_total_size_bytes;
  ASSERT_EQ(StoreRegistrationData(std::move(data2), std::move(resources2)),
            ServiceWorkerDatabase::Status::kOk);

  // Storage usage should report total resource size from two registrations.
  int64_t usage;
  EXPECT_EQ(GetUsageForStorageKey(kKey1, usage),
            ServiceWorkerDatabase::Status::kOk);
  EXPECT_EQ(usage, resources_total_size_bytes1 + resources_total_size_bytes2);

  // Delete the first registration. Storage usage should report only the second
  // registration.
  EXPECT_EQ(DeleteRegistration(kRegistrationId1, kKey1),
            ServiceWorkerDatabase::Status::kOk);
  EXPECT_EQ(GetUsageForStorageKey(kKey1, usage),
            ServiceWorkerDatabase::Status::kOk);
  EXPECT_EQ(usage, resources_total_size_bytes2);

  // Delete the second registration. No storage usage should be reported.
  EXPECT_EQ(DeleteRegistration(kRegistrationId2, kKey1),
            ServiceWorkerDatabase::Status::kOk);
  EXPECT_EQ(GetUsageForStorageKey(kKey1, usage),
            ServiceWorkerDatabase::Status::kOk);
  EXPECT_EQ(usage, 0);
}

}  // namespace service_worker_storage_unittest
}  // namespace storage
