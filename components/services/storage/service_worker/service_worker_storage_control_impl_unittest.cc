// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/service_worker/service_worker_storage_control_impl.h"

#include <cstdint>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/span.h"
#include "base/files/scoped_temp_dir.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/services/storage/service_worker/service_worker_storage_test_utils.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/data_pipe_utils.h"
#include "net/disk_cache/disk_cache.h"
#include "net/http/http_util.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/service_worker/navigation_preload_state.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace storage {

using DatabaseStatus = mojom::ServiceWorkerDatabaseStatus;
using RegistrationData = mojom::ServiceWorkerRegistrationDataPtr;
using ResourceRecord = mojom::ServiceWorkerResourceRecordPtr;

namespace {

struct FindRegistrationResult {
  DatabaseStatus status;
  mojom::ServiceWorkerFindRegistrationResultPtr entry;
};

struct ReadResponseHeadResult {
  int status;
  network::mojom::URLResponseHeadPtr response_head;
  std::optional<mojo_base::BigBuffer> metadata;
};

struct ReadDataResult {
  int status;
  std::string data;
};

struct GetRegistrationsForOriginResult {
  DatabaseStatus status;
  std::vector<mojom::ServiceWorkerFindRegistrationResultPtr> registrations;
};

struct DeleteRegistrationResult {
  DatabaseStatus status;
  mojom::ServiceWorkerStorageStorageKeyState storage_key_state;
};

struct GetNewVersionIdResult {
  int64_t version_id;
  mojo::PendingRemote<mojom::ServiceWorkerLiveVersionRef> reference;
};

struct GetUserDataResult {
  DatabaseStatus status;
  std::vector<std::string> values;
};

struct GetUserDataByKeyPrefixResult {
  DatabaseStatus status;
  std::vector<std::string> values;
};

struct GetUserKeysAndDataByKeyPrefixResult {
  DatabaseStatus status;
  base::flat_map<std::string, std::string> user_data;
};

struct GetUserDataForAllRegistrationsResult {
  DatabaseStatus status;
  std::vector<mojom::ServiceWorkerUserDataPtr> values;
};

struct GetUsageForStorageKeyResult {
  DatabaseStatus status;
  int64_t usage;
};

ReadResponseHeadResult ReadResponseHead(
    mojom::ServiceWorkerResourceReader* reader) {
  ReadResponseHeadResult result;
  base::RunLoop loop;
  reader->ReadResponseHead(base::BindLambdaForTesting(
      [&](int status, network::mojom::URLResponseHeadPtr response_head,
          std::optional<mojo_base::BigBuffer> metadata) {
        result.status = status;
        result.response_head = std::move(response_head);
        result.metadata = std::move(metadata);
        loop.Quit();
      }));
  loop.Run();
  return result;
}

ReadDataResult ReadResponseData(mojom::ServiceWorkerResourceReader* reader,
                                int data_size) {
  mojo::ScopedDataPipeConsumerHandle data_consumer;
  base::RunLoop loop;
  reader->PrepareReadData(
      data_size,
      base::BindLambdaForTesting([&](mojo::ScopedDataPipeConsumerHandle pipe) {
        data_consumer = std::move(pipe);
        loop.Quit();
      }));
  loop.Run();

  ReadDataResult result;

  base::RunLoop loop2;
  reader->ReadData(base::BindLambdaForTesting([&](int32_t status) {
    result.status = status;
    loop2.Quit();
  }));
  result.data = test::ReadDataPipeViaRunLoop(std::move(data_consumer));
  loop2.Run();

  return result;
}

int WriteResponseHead(mojom::ServiceWorkerResourceWriter* writer,
                      network::mojom::URLResponseHeadPtr response_head) {
  int return_value;
  base::RunLoop loop;
  writer->WriteResponseHead(std::move(response_head),
                            base::BindLambdaForTesting([&](int result) {
                              return_value = result;
                              loop.Quit();
                            }));
  loop.Run();
  return return_value;
}

int WriteResponseData(mojom::ServiceWorkerResourceWriter* writer,
                      mojo_base::BigBuffer data) {
  int return_value;
  base::RunLoop loop;
  writer->WriteData(std::move(data),
                    base::BindLambdaForTesting([&](int result) {
                      return_value = result;
                      loop.Quit();
                    }));
  loop.Run();
  return return_value;
}

int WriteResponseMetadata(mojom::ServiceWorkerResourceMetadataWriter* writer,
                          mojo_base::BigBuffer metadata) {
  int return_value;
  base::RunLoop loop;
  writer->WriteMetadata(std::move(metadata),
                        base::BindLambdaForTesting([&](int result) {
                          return_value = result;
                          loop.Quit();
                        }));
  loop.Run();
  return return_value;
}

}  // namespace

class ServiceWorkerStorageControlImplTest : public testing::Test {
 public:
  ServiceWorkerStorageControlImplTest() = default;

  void SetUp() override {
    ASSERT_TRUE(user_data_directory_.CreateUniqueTempDir());
    SetUpStorage();
  }

  void TearDown() override { DestroyStorage(); }

  void SetUpStorage() {
    storage_impl_ = std::make_unique<ServiceWorkerStorageControlImpl>(
        user_data_directory_.GetPath(),
        /*database_task_runner=*/
        base::SingleThreadTaskRunner::GetCurrentDefault(),
        remote_.BindNewPipeAndPassReceiver());
  }

  void DestroyStorage() {
    remote_.reset();
    storage_impl_.reset();
    disk_cache::FlushCacheThreadForTesting();
    task_environment().RunUntilIdle();
  }

  void RestartStorage() {
    DestroyStorage();
    SetUpStorage();
    LazyInitializeForTest();
  }

  mojom::ServiceWorkerStorageControl* storage() { return storage_impl_.get(); }

  base::test::TaskEnvironment& task_environment() { return task_environment_; }

  void LazyInitializeForTest() { storage_impl_->LazyInitializeForTest(); }

  FindRegistrationResult FindRegistrationForClientUrl(
      const GURL& client_url,
      const blink::StorageKey& key) {
    FindRegistrationResult return_value;
    base::RunLoop loop;
    storage()->FindRegistrationForClientUrl(
        client_url, key,
        base::BindLambdaForTesting(
            [&](DatabaseStatus status,
                mojom::ServiceWorkerFindRegistrationResultPtr entry,
                const std::optional<std::vector<GURL>>& scopes) {
              return_value.status = status;
              return_value.entry = std::move(entry);
              loop.Quit();
            }));
    loop.Run();
    return return_value;
  }

  FindRegistrationResult FindRegistrationForScope(
      const GURL& scope,
      const blink::StorageKey& key) {
    FindRegistrationResult return_value;
    base::RunLoop loop;
    storage()->FindRegistrationForScope(
        scope, key,
        base::BindLambdaForTesting(
            [&](DatabaseStatus status,
                mojom::ServiceWorkerFindRegistrationResultPtr entry) {
              return_value.status = status;
              return_value.entry = std::move(entry);
              loop.Quit();
            }));
    loop.Run();
    return return_value;
  }

  FindRegistrationResult FindRegistrationForId(
      int64_t registration_id,
      const std::optional<blink::StorageKey>& key) {
    FindRegistrationResult return_value;
    base::RunLoop loop;
    storage()->FindRegistrationForId(
        registration_id, key,
        base::BindLambdaForTesting(
            [&](DatabaseStatus status,
                mojom::ServiceWorkerFindRegistrationResultPtr entry) {
              return_value.status = status;
              return_value.entry = std::move(entry);
              loop.Quit();
            }));
    loop.Run();
    return return_value;
  }

  GetRegistrationsForOriginResult GetRegistrationsForStorageKey(
      const blink::StorageKey& key) {
    GetRegistrationsForOriginResult result;
    base::RunLoop loop;
    storage()->GetRegistrationsForStorageKey(
        key, base::BindLambdaForTesting(
                 [&](DatabaseStatus status,
                     std::vector<mojom::ServiceWorkerFindRegistrationResultPtr>
                         registrations) {
                   result.status = status;
                   result.registrations = std::move(registrations);
                   loop.Quit();
                 }));
    loop.Run();
    return result;
  }

  DatabaseStatus StoreRegistration(
      RegistrationData registration,
      std::vector<mojom::ServiceWorkerResourceRecordPtr> resources) {
    DatabaseStatus out_status;
    base::RunLoop loop;
    storage()->StoreRegistration(
        std::move(registration), std::move(resources),
        base::BindLambdaForTesting(
            [&](DatabaseStatus status, uint64_t /*deleted_resources_size*/) {
              out_status = status;
              loop.Quit();
            }));
    loop.Run();
    return out_status;
  }

  DeleteRegistrationResult DeleteRegistration(int64_t registration_id,
                                              const blink::StorageKey& key) {
    DeleteRegistrationResult result;
    base::RunLoop loop;
    storage()->DeleteRegistration(
        registration_id, key,
        base::BindLambdaForTesting(
            [&](DatabaseStatus status, uint64_t /*deleted_resources_size*/,
                mojom::ServiceWorkerStorageStorageKeyState storage_key_state) {
              result.status = status;
              result.storage_key_state = storage_key_state;
              loop.Quit();
            }));
    loop.Run();
    return result;
  }

  DatabaseStatus UpdateToActiveState(int64_t registration_id,
                                     const blink::StorageKey& key) {
    DatabaseStatus out_status;
    base::RunLoop loop;
    storage()->UpdateToActiveState(
        registration_id, key,
        base::BindLambdaForTesting([&](DatabaseStatus status) {
          out_status = status;
          loop.Quit();
        }));
    loop.Run();
    return out_status;
  }

  DatabaseStatus UpdateLastUpdateCheckTime(int64_t registration_id,
                                           const blink::StorageKey& key,
                                           base::Time last_update_check_time) {
    DatabaseStatus out_status;
    base::RunLoop loop;
    storage()->UpdateLastUpdateCheckTime(
        registration_id, key, last_update_check_time,
        base::BindLambdaForTesting([&](DatabaseStatus status) {
          out_status = status;
          loop.Quit();
        }));
    loop.Run();
    return out_status;
  }

  DatabaseStatus UpdateFetchHandlerType(
      int64_t registration_id,
      const blink::StorageKey& key,
      blink::mojom::ServiceWorkerFetchHandlerType fetch_handler_type) {
    DatabaseStatus out_status;
    base::RunLoop loop;
    storage()->UpdateFetchHandlerType(
        registration_id, key, fetch_handler_type,
        base::BindLambdaForTesting([&](DatabaseStatus status) {
          out_status = status;
          loop.Quit();
        }));
    loop.Run();
    return out_status;
  }

  DatabaseStatus UpdateResourceSha256Checksums(
      int64_t registration_id,
      const blink::StorageKey& key,
      const base::flat_map<int64_t, std::string>& updated_sha256_checksums) {
    DatabaseStatus out_status;
    base::RunLoop loop;
    storage()->UpdateResourceSha256Checksums(
        registration_id, key, updated_sha256_checksums,
        base::BindLambdaForTesting([&](DatabaseStatus status) {
          out_status = status;
          loop.Quit();
        }));
    loop.Run();
    return out_status;
  }

  DatabaseStatus UpdateNavigationPreloadEnabled(int64_t registration_id,
                                                const blink::StorageKey& key,
                                                bool enable) {
    DatabaseStatus out_status;
    base::RunLoop loop;
    storage()->UpdateNavigationPreloadEnabled(
        registration_id, key, enable,
        base::BindLambdaForTesting([&](DatabaseStatus status) {
          out_status = status;
          loop.Quit();
        }));
    loop.Run();
    return out_status;
  }

  DatabaseStatus UpdateNavigationPreloadHeader(int64_t registration_id,
                                               const blink::StorageKey& key,
                                               const std::string& value) {
    DatabaseStatus out_status;
    base::RunLoop loop;
    storage()->UpdateNavigationPreloadHeader(
        registration_id, key, value,
        base::BindLambdaForTesting([&](DatabaseStatus status) {
          out_status = status;
          loop.Quit();
        }));
    loop.Run();
    return out_status;
  }

  int64_t GetNewRegistrationId() {
    int64_t return_value;
    base::RunLoop loop;
    storage()->GetNewRegistrationId(
        base::BindLambdaForTesting([&](int64_t registration_id) {
          return_value = registration_id;
          loop.Quit();
        }));
    loop.Run();
    return return_value;
  }

  GetNewVersionIdResult GetNewVersionId() {
    GetNewVersionIdResult result;
    base::RunLoop loop;
    storage()->GetNewVersionId(base::BindLambdaForTesting(
        [&](int64_t version_id,
            mojo::PendingRemote<mojom::ServiceWorkerLiveVersionRef> reference) {
          result.version_id = version_id;
          result.reference = std::move(reference);
          loop.Quit();
        }));
    loop.Run();
    return result;
  }

  int64_t GetNewResourceId() {
    int64_t return_value;
    base::RunLoop loop;
    storage()->GetNewResourceId(
        base::BindLambdaForTesting([&](int64_t resource_id) {
          return_value = resource_id;
          loop.Quit();
        }));
    loop.Run();
    return return_value;
  }

  DatabaseStatus StoreUncommittedResourceId(int64_t resource_id) {
    DatabaseStatus return_value;
    base::RunLoop loop;
    storage()->StoreUncommittedResourceId(
        resource_id, base::BindLambdaForTesting([&](DatabaseStatus status) {
          return_value = status;
          loop.Quit();
        }));
    loop.Run();
    return return_value;
  }

  DatabaseStatus DoomUncommittedResources(
      const std::vector<int64_t> resource_ids) {
    DatabaseStatus return_value;
    base::RunLoop loop;
    storage()->DoomUncommittedResources(
        resource_ids, base::BindLambdaForTesting([&](DatabaseStatus status) {
          return_value = status;
          loop.Quit();
        }));
    loop.Run();
    return return_value;
  }

  GetUserDataResult GetUserData(int64_t registration_id,
                                const std::vector<std::string>& keys) {
    GetUserDataResult result;
    base::RunLoop loop;
    storage()->GetUserData(
        registration_id, keys,
        base::BindLambdaForTesting(
            [&](DatabaseStatus status, const std::vector<std::string>& values) {
              result.status = status;
              result.values = values;
              loop.Quit();
            }));
    loop.Run();
    return result;
  }

  DatabaseStatus StoreUserData(
      int64_t registration_id,
      const blink::StorageKey& key,
      std::vector<mojom::ServiceWorkerUserDataPtr> user_data) {
    DatabaseStatus return_value;
    base::RunLoop loop;
    storage()->StoreUserData(
        registration_id, key, std::move(user_data),
        base::BindLambdaForTesting([&](DatabaseStatus status) {
          return_value = status;
          loop.Quit();
        }));
    loop.Run();
    return return_value;
  }

  DatabaseStatus ClearUserData(int64_t registration_id,
                               const std::vector<std::string>& keys) {
    DatabaseStatus return_value;
    base::RunLoop loop;
    storage()->ClearUserData(
        registration_id, keys,
        base::BindLambdaForTesting([&](DatabaseStatus status) {
          return_value = status;
          loop.Quit();
        }));
    loop.Run();
    return return_value;
  }

  GetUserDataByKeyPrefixResult GetUserDataByKeyPrefix(
      int64_t registration_id,
      const std::string& key_prefix) {
    GetUserDataByKeyPrefixResult result;
    base::RunLoop loop;
    storage()->GetUserDataByKeyPrefix(
        registration_id, key_prefix,
        base::BindLambdaForTesting(
            [&](DatabaseStatus status, const std::vector<std::string>& values) {
              result.status = status;
              result.values = values;
              loop.Quit();
            }));
    loop.Run();
    return result;
  }

  GetUserKeysAndDataByKeyPrefixResult GetUserKeysAndDataByKeyPrefix(
      int64_t registration_id,
      const std::string& key_prefix) {
    GetUserKeysAndDataByKeyPrefixResult result;
    base::RunLoop loop;
    storage()->GetUserKeysAndDataByKeyPrefix(
        registration_id, key_prefix,
        base::BindLambdaForTesting(
            [&](DatabaseStatus status,
                const base::flat_map<std::string, std::string>& user_data) {
              result.status = status;
              result.user_data = user_data;
              loop.Quit();
            }));
    loop.Run();
    return result;
  }

  DatabaseStatus ClearUserDataByKeyPrefixes(
      int64_t registration_id,
      const std::vector<std::string>& key_prefixes) {
    DatabaseStatus return_value;
    base::RunLoop loop;
    storage()->ClearUserDataByKeyPrefixes(
        registration_id, key_prefixes,
        base::BindLambdaForTesting([&](DatabaseStatus status) {
          return_value = status;
          loop.Quit();
        }));
    loop.Run();
    return return_value;
  }

  GetUserDataForAllRegistrationsResult GetUserDataForAllRegistrations(
      const std::string& key) {
    GetUserDataForAllRegistrationsResult result;
    base::RunLoop loop;
    storage()->GetUserDataForAllRegistrations(
        key, base::BindLambdaForTesting(
                 [&](DatabaseStatus status,
                     std::vector<mojom::ServiceWorkerUserDataPtr> values) {
                   result.status = status;
                   result.values = std::move(values);
                   loop.Quit();
                 }));
    loop.Run();
    return result;
  }

  GetUserDataForAllRegistrationsResult
  GetUserDataForAllRegistrationsByKeyPrefix(const std::string& key_prefix) {
    GetUserDataForAllRegistrationsResult result;
    base::RunLoop loop;
    storage()->GetUserDataForAllRegistrationsByKeyPrefix(
        key_prefix,
        base::BindLambdaForTesting(
            [&](DatabaseStatus status,
                std::vector<mojom::ServiceWorkerUserDataPtr> values) {
              result.status = status;
              result.values = std::move(values);
              loop.Quit();
            }));
    loop.Run();
    return result;
  }

  DatabaseStatus ClearUserDataForAllRegistrationsByKeyPrefix(
      const std::string& key_prefix) {
    DatabaseStatus return_value;
    base::RunLoop loop;
    storage()->ClearUserDataForAllRegistrationsByKeyPrefix(
        key_prefix, base::BindLambdaForTesting([&](DatabaseStatus status) {
          return_value = status;
          loop.Quit();
        }));
    loop.Run();
    return return_value;
  }

  GetUsageForStorageKeyResult GetUsageForStorageKey(
      const blink::StorageKey& key) {
    GetUsageForStorageKeyResult result;
    base::RunLoop loop;
    storage()->GetUsageForStorageKey(
        key,
        base::BindLambdaForTesting([&](DatabaseStatus status, int64_t usage) {
          result.status = status;
          result.usage = usage;
          loop.Quit();
        }));
    loop.Run();
    return result;
  }

  // Creates a registration with the given resource records.
  RegistrationData CreateRegistrationData(
      int64_t registration_id,
      int64_t version_id,
      const GURL& scope,
      const blink::StorageKey& key,
      const GURL& script_url,
      const std::vector<mojom::ServiceWorkerResourceRecordPtr>& resources) {
    auto data = mojom::ServiceWorkerRegistrationData::New();
    data->registration_id = registration_id;
    data->version_id = version_id;
    data->scope = scope;
    data->key = key;
    data->script = script_url;
    data->navigation_preload_state =
        blink::mojom::NavigationPreloadState::New();

    int64_t resources_total_size_bytes = 0;
    for (auto& resource : resources) {
      resources_total_size_bytes += resource->size_bytes;
    }
    data->resources_total_size_bytes = resources_total_size_bytes;

    return data;
  }

  // Creates a registration with a single resource and stores the registration.
  DatabaseStatus CreateAndStoreRegistration(int64_t registration_id,
                                            int64_t version_id,
                                            int64_t resource_id,
                                            const GURL& scope,
                                            const blink::StorageKey& key,
                                            const GURL& script_url,
                                            int64_t script_size) {
    std::vector<mojom::ServiceWorkerResourceRecordPtr> resources;
    resources.push_back(mojom::ServiceWorkerResourceRecord::New(
        resource_id, script_url, script_size,
        /*sha256_checksum=*/std::nullopt));

    RegistrationData data = CreateRegistrationData(
        registration_id, version_id, scope, key, script_url, resources);

    DatabaseStatus status =
        StoreRegistration(std::move(data), std::move(resources));
    return status;
  }

  int WriteResource(int64_t resource_id, const std::string& data) {
    auto response_head = network::mojom::URLResponseHead::New();
    response_head->headers = base::MakeRefCounted<net::HttpResponseHeaders>(
        net::HttpUtil::AssembleRawHeaders(
            "HTTP/1.1 200 OK\n"
            "Content-Type: application/javascript\n"));
    response_head->headers->GetMimeType(&response_head->mime_type);

    mojo::Remote<mojom::ServiceWorkerResourceWriter> writer =
        CreateResourceWriter(resource_id);
    int result = WriteResponseHead(writer.get(), std::move(response_head));
    if (result < 0)
      return result;

    mojo_base::BigBuffer buffer(base::as_bytes(base::make_span(data)));
    result = WriteResponseData(writer.get(), std::move(buffer));
    return result;
  }

  ReadDataResult ReadResource(int64_t resource_id, int data_size) {
    mojo::Remote<mojom::ServiceWorkerResourceReader> reader =
        CreateResourceReader(resource_id);
    return ReadResponseData(reader.get(), data_size);
  }

  mojo::Remote<mojom::ServiceWorkerResourceReader> CreateResourceReader(
      int64_t resource_id) {
    mojo::Remote<mojom::ServiceWorkerResourceReader> reader;
    storage()->CreateResourceReader(resource_id,
                                    reader.BindNewPipeAndPassReceiver());
    return reader;
  }

  mojo::Remote<mojom::ServiceWorkerResourceWriter> CreateResourceWriter(
      int64_t resource_id) {
    mojo::Remote<mojom::ServiceWorkerResourceWriter> writer;
    storage()->CreateResourceWriter(resource_id,
                                    writer.BindNewPipeAndPassReceiver());
    return writer;
  }

  mojo::Remote<mojom::ServiceWorkerResourceMetadataWriter>
  CreateResourceMetadataWriter(int64_t resource_id) {
    mojo::Remote<mojom::ServiceWorkerResourceMetadataWriter> writer;
    storage()->CreateResourceMetadataWriter(
        resource_id, writer.BindNewPipeAndPassReceiver());
    return writer;
  }

  // Helper function that reads uncommitted resource ids from database.
  std::vector<int64_t> GetUncommittedResourceIds() {
    std::vector<int64_t> result;
    base::RunLoop loop;
    storage()->GetUncommittedResourceIdsForTest(base::BindLambdaForTesting(
        [&](DatabaseStatus status, const std::vector<int64_t>& ids) {
          EXPECT_EQ(status, ServiceWorkerDatabase::Status::kOk);
          result = ids;
          loop.Quit();
        }));
    loop.Run();
    return result;
  }

 private:
  base::ScopedTempDir user_data_directory_;
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<ServiceWorkerStorageControlImpl> storage_impl_;
  mojo::Remote<mojom::ServiceWorkerStorageControl> remote_;
};

// Tests that FindRegistration methods don't find anything without having stored
// anything.
TEST_F(ServiceWorkerStorageControlImplTest, FindRegistration_NoRegistration) {
  const GURL kScope("https://www.example.com/scope/");
  const blink::StorageKey kKey =
      blink::StorageKey::CreateFirstParty(url::Origin::Create(kScope));
  const GURL kClientUrl("https://www.example.com/scope/document.html");
  const int64_t kRegistrationId = 0;

  LazyInitializeForTest();

  {
    FindRegistrationResult result =
        FindRegistrationForClientUrl(kClientUrl, kKey);
    EXPECT_EQ(result.status, DatabaseStatus::kErrorNotFound);
  }

  {
    FindRegistrationResult result = FindRegistrationForScope(kScope, kKey);
    EXPECT_EQ(result.status, DatabaseStatus::kErrorNotFound);
  }

  {
    FindRegistrationResult result =
        FindRegistrationForId(kRegistrationId, kKey);
    EXPECT_EQ(result.status, DatabaseStatus::kErrorNotFound);
  }

  {
    FindRegistrationResult result =
        FindRegistrationForId(kRegistrationId, std::nullopt);
    EXPECT_EQ(result.status, DatabaseStatus::kErrorNotFound);
  }
}

// Tests that storing/finding/deleting a registration works.
TEST_F(ServiceWorkerStorageControlImplTest, StoreAndDeleteRegistration) {
  const GURL kScope("https://www.example.com/scope/");
  const blink::StorageKey kKey =
      blink::StorageKey::CreateFirstParty(url::Origin::Create(kScope));
  const GURL kScriptUrl("https://www.example.com/scope/sw.js");
  const GURL kClientUrl("https://www.example.com/scope/document.html");
  const int64_t kScriptSize = 10;

  LazyInitializeForTest();

  const int64_t kRegistrationId = GetNewResourceId();
  const int64_t kVersionId = GetNewVersionId().version_id;

  // Create a registration data with a single resource.
  std::vector<mojom::ServiceWorkerResourceRecordPtr> resources;
  resources.push_back(mojom::ServiceWorkerResourceRecord::New(
      kRegistrationId, kScriptUrl, kScriptSize,
      /*sha256_checksum=*/std::nullopt));

  auto data = mojom::ServiceWorkerRegistrationData::New();
  data->registration_id = kRegistrationId;
  data->scope = kScope;
  data->key = kKey;
  data->script = kScriptUrl;
  data->version_id = kVersionId;
  data->navigation_preload_state = blink::mojom::NavigationPreloadState::New();

  int64_t resources_total_size_bytes = 0;
  for (auto& resource : resources) {
    resources_total_size_bytes += resource->size_bytes;
  }
  data->resources_total_size_bytes = resources_total_size_bytes;

  // Store the registration data.
  {
    DatabaseStatus status =
        StoreRegistration(std::move(data), std::move(resources));
    ASSERT_EQ(status, DatabaseStatus::kOk);
  }

  // Find the registration. Find operations should succeed.
  {
    FindRegistrationResult result =
        FindRegistrationForClientUrl(kClientUrl, kKey);
    ASSERT_EQ(result.status, DatabaseStatus::kOk);
    EXPECT_EQ(result.entry->registration->registration_id, kRegistrationId);
    EXPECT_EQ(result.entry->registration->scope, kScope);
    EXPECT_EQ(result.entry->registration->key, kKey);
    EXPECT_EQ(result.entry->registration->script, kScriptUrl);
    EXPECT_EQ(result.entry->registration->version_id, kVersionId);
    EXPECT_EQ(result.entry->registration->resources_total_size_bytes,
              resources_total_size_bytes);
    EXPECT_EQ(result.entry->resources.size(), 1UL);

    result = FindRegistrationForScope(kScope, kKey);
    EXPECT_EQ(result.status, DatabaseStatus::kOk);
    result = FindRegistrationForId(kRegistrationId, kKey);
    EXPECT_EQ(result.status, DatabaseStatus::kOk);
    result = FindRegistrationForId(kRegistrationId, std::nullopt);
    EXPECT_EQ(result.status, DatabaseStatus::kOk);
  }

  // Delete the registration.
  {
    DeleteRegistrationResult result = DeleteRegistration(kRegistrationId, kKey);
    ASSERT_EQ(result.status, DatabaseStatus::kOk);
    EXPECT_EQ(result.storage_key_state,
              mojom::ServiceWorkerStorageStorageKeyState::kDelete);
  }

  // Try to find the deleted registration. These operation should result in
  // kErrorNotFound.
  {
    FindRegistrationResult result =
        FindRegistrationForClientUrl(kClientUrl, kKey);
    EXPECT_EQ(result.status, DatabaseStatus::kErrorNotFound);
    result = FindRegistrationForScope(kScope, kKey);
    EXPECT_EQ(result.status, DatabaseStatus::kErrorNotFound);
    result = FindRegistrationForId(kRegistrationId, kKey);
    EXPECT_EQ(result.status, DatabaseStatus::kErrorNotFound);
  }
}

TEST_F(ServiceWorkerStorageControlImplTest, UpdateToActiveState) {
  const GURL kScope("https://www.example.com/");
  const blink::StorageKey kKey =
      blink::StorageKey::CreateFirstParty(url::Origin::Create(kScope));
  const GURL kScriptUrl("https://www.example.com/sw.js");
  const int64_t kScriptSize = 10;

  LazyInitializeForTest();

  // Preparation: Store a registration.
  const int64_t registration_id = GetNewRegistrationId();
  const int64_t version_id = GetNewVersionId().version_id;
  const int64_t resource_id = GetNewResourceId();
  DatabaseStatus status =
      CreateAndStoreRegistration(registration_id, version_id, resource_id,
                                 kScope, kKey, kScriptUrl, kScriptSize);
  ASSERT_EQ(status, DatabaseStatus::kOk);

  // The stored registration shouldn't be activated yet.
  {
    FindRegistrationResult result =
        FindRegistrationForId(registration_id, kKey);
    ASSERT_EQ(result.status, DatabaseStatus::kOk);
    EXPECT_FALSE(result.entry->registration->is_active);
  }

  // Set the registration is active in storage.
  status = UpdateToActiveState(registration_id, kKey);
  ASSERT_EQ(status, DatabaseStatus::kOk);

  // Now the stored registration should be active.
  {
    FindRegistrationResult result =
        FindRegistrationForId(registration_id, kKey);
    ASSERT_EQ(result.status, DatabaseStatus::kOk);
    EXPECT_TRUE(result.entry->registration->is_active);
  }
}

TEST_F(ServiceWorkerStorageControlImplTest, UpdateLastUpdateCheckTime) {
  const GURL kScope("https://www.example.com/");
  const blink::StorageKey kKey =
      blink::StorageKey::CreateFirstParty(url::Origin::Create(kScope));
  const GURL kScriptUrl("https://www.example.com/sw.js");
  const int64_t kScriptSize = 10;

  LazyInitializeForTest();

  // Preparation: Store a registration.
  const int64_t registration_id = GetNewRegistrationId();
  const int64_t version_id = GetNewVersionId().version_id;
  const int64_t resource_id = GetNewResourceId();
  DatabaseStatus status =
      CreateAndStoreRegistration(registration_id, version_id, resource_id,
                                 kScope, kKey, kScriptUrl, kScriptSize);
  ASSERT_EQ(status, DatabaseStatus::kOk);

  // The stored registration shouldn't have the last update check time yet.
  {
    FindRegistrationResult result =
        FindRegistrationForId(registration_id, kKey);
    ASSERT_EQ(result.status, DatabaseStatus::kOk);
    EXPECT_EQ(result.entry->registration->last_update_check, base::Time());
  }

  // Set the last update check time.
  const base::Time now = base::Time::Now();
  status = UpdateLastUpdateCheckTime(registration_id, kKey, now);
  ASSERT_EQ(status, DatabaseStatus::kOk);

  // Now the stored registration should have the last update check time.
  {
    FindRegistrationResult result =
        FindRegistrationForId(registration_id, kKey);
    ASSERT_EQ(result.status, DatabaseStatus::kOk);
    EXPECT_EQ(result.entry->registration->last_update_check, now);
  }
}

TEST_F(ServiceWorkerStorageControlImplTest, UpdateFetchHandlerType) {
  const GURL kScope("https://www.example.com/");
  const blink::StorageKey kKey =
      blink::StorageKey::CreateFirstParty(url::Origin::Create(kScope));
  const GURL kScriptUrl("https://www.example.com/sw.js");
  const int64_t kScriptSize = 10;

  LazyInitializeForTest();

  // Preparation: Store a registration.
  const int64_t registration_id = GetNewRegistrationId();
  const int64_t version_id = GetNewVersionId().version_id;
  const int64_t resource_id = GetNewResourceId();
  DatabaseStatus status =
      CreateAndStoreRegistration(registration_id, version_id, resource_id,
                                 kScope, kKey, kScriptUrl, kScriptSize);
  ASSERT_EQ(status, DatabaseStatus::kOk);

  // The stored registration shouldn't have the fetch handler type yet.
  // i.e. we expect the default value.
  {
    FindRegistrationResult result =
        FindRegistrationForId(registration_id, kKey);
    ASSERT_EQ(result.status, DatabaseStatus::kOk);
    EXPECT_EQ(result.entry->registration->fetch_handler_type,
              blink::mojom::ServiceWorkerFetchHandlerType::kNoHandler);
  }

  // Set the fetch handler type.
  status = UpdateFetchHandlerType(
      registration_id, kKey,
      blink::mojom::ServiceWorkerFetchHandlerType::kNotSkippable);
  ASSERT_EQ(status, DatabaseStatus::kOk);

  // Now the stored registration should have the fetch handler type.
  {
    FindRegistrationResult result =
        FindRegistrationForId(registration_id, kKey);
    ASSERT_EQ(result.status, DatabaseStatus::kOk);
    EXPECT_EQ(result.entry->registration->fetch_handler_type,
              blink::mojom::ServiceWorkerFetchHandlerType::kNotSkippable);
  }
}

TEST_F(ServiceWorkerStorageControlImplTest, UpdateResourceSha256Checksums) {
  const GURL kScope("https://www.example.com/");
  const blink::StorageKey kKey =
      blink::StorageKey::CreateFirstParty(url::Origin::Create(kScope));
  const GURL kScriptUrl("https://www.example.com/sw.js");
  const GURL kImportedScriptUrl("https://www.example.com/imported.js");

  LazyInitializeForTest();

  // Preparation: Create a registration with two resources. These aren't written
  // to storage yet.
  std::vector<ResourceRecord> resources;
  const int64_t resource_id1 = GetNewResourceId();
  const std::string resource_data1 = "main script data";
  resources.push_back(mojom::ServiceWorkerResourceRecord::New(
      resource_id1, kScriptUrl, resource_data1.size(),
      /*sha256_checksum=*/std::nullopt));

  const int64_t resource_id2 = GetNewResourceId();
  const std::string resource_data2 = "imported script data";
  resources.push_back(mojom::ServiceWorkerResourceRecord::New(
      resource_id2, kImportedScriptUrl, resource_data2.size(),
      /*sha256_checksum=*/std::nullopt));

  // Preparation: Create a registration with two resources.
  const int64_t registration_id = GetNewRegistrationId();
  const int64_t version_id = GetNewVersionId().version_id;
  RegistrationData registration_data = CreateRegistrationData(
      registration_id, version_id, kScope, kKey, kScriptUrl, resources);
  DatabaseStatus status =
      StoreRegistration(std::move(registration_data), std::move(resources));
  ASSERT_EQ(status, DatabaseStatus::kOk);

  // Resources written in the storage don't have |sha256_checksum|
  FindRegistrationResult result = FindRegistrationForId(registration_id, kKey);
  ASSERT_EQ(result.status, DatabaseStatus::kOk);
  ASSERT_FALSE(result.entry->resources[0]->sha256_checksum.has_value());
  ASSERT_FALSE(result.entry->resources[1]->sha256_checksum.has_value());

  // Update resources with fake checksum strings.
  const std::string expected_checksum1 = "abcd";
  const std::string expected_checksum2 = "efgh";
  status = UpdateResourceSha256Checksums(
      registration_id, kKey,
      base::flat_map<int64_t, std::string>(
          {{result.entry->resources[0]->resource_id, expected_checksum1},
           {result.entry->resources[1]->resource_id, expected_checksum2}}));
  EXPECT_EQ(status, DatabaseStatus::kOk);

  // Now resources from the storage have fake checksums.
  result = FindRegistrationForId(registration_id, kKey);
  ASSERT_EQ(result.status, DatabaseStatus::kOk);
  EXPECT_EQ(result.entry->resources[0]->sha256_checksum, expected_checksum1);
  EXPECT_EQ(result.entry->resources[1]->sha256_checksum, expected_checksum2);
}

TEST_F(ServiceWorkerStorageControlImplTest, Update) {
  const GURL kScope("https://www.example.com/");
  const blink::StorageKey kKey =
      blink::StorageKey::CreateFirstParty(url::Origin::Create(kScope));
  const GURL kScriptUrl("https://www.example.com/sw.js");
  const int64_t kScriptSize = 10;

  LazyInitializeForTest();

  // Preparation: Store a registration.
  const int64_t registration_id = GetNewRegistrationId();
  const int64_t version_id = GetNewVersionId().version_id;
  const int64_t resource_id = GetNewResourceId();
  DatabaseStatus status =
      CreateAndStoreRegistration(registration_id, version_id, resource_id,
                                 kScope, kKey, kScriptUrl, kScriptSize);
  ASSERT_EQ(status, DatabaseStatus::kOk);

  // Check the stored registration has default navigation preload fields.
  {
    FindRegistrationResult result =
        FindRegistrationForId(registration_id, kKey);
    ASSERT_EQ(result.status, DatabaseStatus::kOk);
    EXPECT_FALSE(result.entry->registration->navigation_preload_state->enabled);
    EXPECT_EQ(result.entry->registration->navigation_preload_state->header,
              "true");
  }

  // Update navigation preload fields.
  const std::string header_value = "my-header";
  status = UpdateNavigationPreloadEnabled(registration_id, kKey, true);
  ASSERT_EQ(status, DatabaseStatus::kOk);
  status = UpdateNavigationPreloadHeader(registration_id, kKey, header_value);
  ASSERT_EQ(status, DatabaseStatus::kOk);

  // Check navigation preload fields are updated.
  {
    FindRegistrationResult result =
        FindRegistrationForId(registration_id, kKey);
    ASSERT_EQ(result.status, DatabaseStatus::kOk);
    EXPECT_TRUE(result.entry->registration->navigation_preload_state->enabled);
    EXPECT_EQ(result.entry->registration->navigation_preload_state->header,
              header_value);
  }
}

// Tests that getting registrations works.
TEST_F(ServiceWorkerStorageControlImplTest, GetRegistrationsForStorageKey) {
  const GURL kScope1("https://www.example.com/foo/");
  const blink::StorageKey kKey1 =
      blink::StorageKey::CreateFirstParty(url::Origin::Create(kScope1));
  const GURL kScriptUrl1("https://www.example.com/foo/sw.js");
  const GURL kScope2("https://www.example.com/bar/");
  const blink::StorageKey kKey2 =
      blink::StorageKey::CreateFirstParty(url::Origin::Create(kScope2));
  const GURL kScriptUrl2("https://www.example.com/bar/sw.js");
  const int64_t kScriptSize = 10;

  LazyInitializeForTest();

  // Store two registrations which have the same origin.
  DatabaseStatus status;
  const int64_t registration_id1 = GetNewRegistrationId();
  const int64_t version_id1 = GetNewVersionId().version_id;
  const int64_t resource_id1 = GetNewResourceId();
  status =
      CreateAndStoreRegistration(registration_id1, version_id1, resource_id1,
                                 kScope1, kKey1, kScriptUrl1, kScriptSize);
  ASSERT_EQ(status, DatabaseStatus::kOk);
  const int64_t registration_id2 = GetNewRegistrationId();
  const int64_t version_id2 = GetNewVersionId().version_id;
  const int64_t resource_id2 = GetNewResourceId();
  status =
      CreateAndStoreRegistration(registration_id2, version_id2, resource_id2,
                                 kScope2, kKey2, kScriptUrl2, kScriptSize);
  ASSERT_EQ(status, DatabaseStatus::kOk);

  // Get registrations for the origin.
  {
    const url::Origin origin = url::Origin::Create(kScope1);
    std::vector<mojom::ServiceWorkerFindRegistrationResultPtr> registrations;

    GetRegistrationsForOriginResult result =
        GetRegistrationsForStorageKey(kKey1);
    ASSERT_EQ(result.status, DatabaseStatus::kOk);
    EXPECT_EQ(result.registrations.size(), 2UL);

    for (auto& registration : result.registrations) {
      EXPECT_EQ(registration->registration->scope.DeprecatedGetOriginAsURL(),
                origin.GetURL());
      EXPECT_EQ(registration->registration->resources_total_size_bytes,
                kScriptSize);
      EXPECT_TRUE(registration->version_reference);
    }
  }

  // Getting registrations for another origin should succeed but shouldn't find
  // anything.
  {
    const url::Origin origin =
        url::Origin::Create(GURL("https://www.example.test/"));
    std::vector<mojom::ServiceWorkerFindRegistrationResultPtr> registrations;

    GetRegistrationsForOriginResult result = GetRegistrationsForStorageKey(
        blink::StorageKey::CreateFirstParty(origin));
    ASSERT_EQ(result.status, DatabaseStatus::kOk);
    EXPECT_EQ(result.registrations.size(), 0UL);
  }
}

// Tests that writing/reading a service worker script succeed.
TEST_F(ServiceWorkerStorageControlImplTest, WriteAndReadResource) {
  LazyInitializeForTest();

  // Create a SSLInfo to write/read.
  net::SSLInfo ssl_info = net::SSLInfo();
  ssl_info.cert =
      net::ImportCertFromFile(net::GetTestCertsDirectory(), "ok_cert.pem");
  ASSERT_TRUE(ssl_info.is_valid());

  int64_t resource_id = GetNewResourceId();

  mojo::Remote<mojom::ServiceWorkerResourceWriter> writer =
      CreateResourceWriter(resource_id);

  // Write a response head.
  {
    auto response_head = network::mojom::URLResponseHead::New();
    response_head->headers = base::MakeRefCounted<net::HttpResponseHeaders>(
        net::HttpUtil::AssembleRawHeaders(
            "HTTP/1.1 200 OK\n"
            "Content-Type: application/javascript\n"));
    response_head->headers->GetMimeType(&response_head->mime_type);
    response_head->ssl_info = ssl_info;

    int result = WriteResponseHead(writer.get(), std::move(response_head));
    ASSERT_GT(result, 0);
  }

  const std::string kData("/* script body */");
  int data_size = kData.size();

  // Write content.
  {
    mojo_base::BigBuffer data(base::as_bytes(base::make_span(kData)));
    int bytes_size = data.size();

    int result = WriteResponseData(writer.get(), std::move(data));
    ASSERT_EQ(bytes_size, result);
  }

  mojo::Remote<mojom::ServiceWorkerResourceReader> reader =
      CreateResourceReader(resource_id);

  // Read the response head, metadata and the content.
  {
    ReadResponseHeadResult result = ReadResponseHead(reader.get());
    ASSERT_GT(result.status, 0);

    EXPECT_EQ(result.response_head->mime_type, "application/javascript");
    EXPECT_EQ(result.response_head->content_length, data_size);
    EXPECT_TRUE(result.response_head->ssl_info->is_valid());
    EXPECT_EQ(result.response_head->ssl_info->cert->serial_number(),
              ssl_info.cert->serial_number());
    EXPECT_EQ(result.metadata, std::nullopt);

    ReadDataResult data_result = ReadResponseData(reader.get(), data_size);
    ASSERT_EQ(data_result.status, data_size);
    EXPECT_EQ(data_result.data, kData);
  }

  const auto kMetadata = base::as_bytes(base::make_span("metadata"));
  int metadata_size = kMetadata.size();

  // Write metadata.
  {
    mojo::Remote<mojom::ServiceWorkerResourceMetadataWriter> metadata_writer =
        CreateResourceMetadataWriter(resource_id);
    int result = WriteResponseMetadata(metadata_writer.get(),
                                       mojo_base::BigBuffer(kMetadata));
    ASSERT_EQ(result, metadata_size);
  }

  // Read the response head again. This time metadata should be read.
  {
    ReadResponseHeadResult result = ReadResponseHead(reader.get());
    ASSERT_GT(result.status, 0);
    ASSERT_TRUE(result.metadata.has_value());
    EXPECT_EQ(result.metadata->size(), kMetadata.size());
    EXPECT_EQ(
        memcmp(result.metadata->data(), kMetadata.data(), kMetadata.size()), 0);
  }
}

// Tests that uncommitted resources can be listed on storage and these resources
// will be committed when a registration is stored with these resources.
TEST_F(ServiceWorkerStorageControlImplTest, UncommittedResources) {
  const GURL kScope("https://www.example.com/");
  const blink::StorageKey kKey =
      blink::StorageKey::CreateFirstParty(url::Origin::Create(kScope));
  const GURL kScriptUrl("https://www.example.com/sw.js");
  const GURL kImportedScriptUrl("https://www.example.com/imported.js");

  LazyInitializeForTest();

  // Preparation: Create a registration with two resources. These aren't written
  // to storage yet.
  std::vector<ResourceRecord> resources;
  const int64_t resource_id1 = GetNewResourceId();
  const std::string resource_data1 = "main script data";
  resources.push_back(mojom::ServiceWorkerResourceRecord::New(
      resource_id1, kScriptUrl, resource_data1.size(),
      /*sha256_checksum=*/std::nullopt));

  const int64_t resource_id2 = GetNewResourceId();
  const std::string resource_data2 = "imported script data";
  resources.push_back(mojom::ServiceWorkerResourceRecord::New(
      resource_id2, kImportedScriptUrl, resource_data2.size(),
      /*sha256_checksum=*/std::nullopt));

  const int64_t registration_id = GetNewRegistrationId();
  const int64_t version_id = GetNewVersionId().version_id;
  RegistrationData registration_data = CreateRegistrationData(
      registration_id, version_id, kScope, kKey, kScriptUrl, resources);

  // Put these resources ids on the uncommitted list in storage.
  DatabaseStatus status;
  status = StoreUncommittedResourceId(resource_id1);
  ASSERT_EQ(status, DatabaseStatus::kOk);
  status = StoreUncommittedResourceId(resource_id2);
  ASSERT_EQ(status, DatabaseStatus::kOk);

  std::vector<int64_t> uncommitted_ids = GetUncommittedResourceIds();
  EXPECT_EQ(uncommitted_ids.size(), 2UL);

  // Write responses and the registration data.
  int result;
  result = WriteResource(resource_id1, resource_data1);
  ASSERT_GT(result, 0);
  result = WriteResource(resource_id2, resource_data2);
  ASSERT_GT(result, 0);
  status =
      StoreRegistration(std::move(registration_data), std::move(resources));
  ASSERT_EQ(status, DatabaseStatus::kOk);

  // Storing registration should take the resource ids out of the uncommitted
  // list.
  uncommitted_ids = GetUncommittedResourceIds();
  EXPECT_TRUE(uncommitted_ids.empty());
}

// Tests that uncommitted resource ids are purged by DoomUncommittedResources.
TEST_F(ServiceWorkerStorageControlImplTest, DoomUncommittedResources) {
  const GURL kScope("https://www.example.com/");

  LazyInitializeForTest();

  const int64_t resource_id1 = GetNewResourceId();
  const int64_t resource_id2 = GetNewResourceId();

  DatabaseStatus status;
  status = StoreUncommittedResourceId(resource_id1);
  ASSERT_EQ(status, DatabaseStatus::kOk);
  status = StoreUncommittedResourceId(resource_id2);
  ASSERT_EQ(status, DatabaseStatus::kOk);

  std::vector<int64_t> uncommitted_ids = GetUncommittedResourceIds();
  EXPECT_EQ(uncommitted_ids.size(), 2UL);

  status = DoomUncommittedResources({resource_id1, resource_id2});
  ASSERT_EQ(status, DatabaseStatus::kOk);
  uncommitted_ids = GetUncommittedResourceIds();
  EXPECT_TRUE(uncommitted_ids.empty());
}

// Tests that storing/getting user data for a registration works.
TEST_F(ServiceWorkerStorageControlImplTest, StoreAndGetUserData) {
  const GURL kScope("https://www.example.com/");
  const blink::StorageKey kKey =
      blink::StorageKey::CreateFirstParty(url::Origin::Create(kScope));
  const GURL kScriptUrl("https://www.example.com/sw.js");
  const int64_t kScriptSize = 10;

  LazyInitializeForTest();

  const int64_t registration_id = GetNewRegistrationId();
  const int64_t version_id = GetNewVersionId().version_id;
  const int64_t resource_id = GetNewResourceId();
  DatabaseStatus status;
  status = CreateAndStoreRegistration(registration_id, version_id, resource_id,
                                      kScope, kKey, kScriptUrl, kScriptSize);
  ASSERT_EQ(status, DatabaseStatus::kOk);

  // Store user data with two entries.
  {
    std::vector<mojom::ServiceWorkerUserDataPtr> user_data;
    user_data.push_back(
        mojom::ServiceWorkerUserData::New(registration_id, "key1", "value1"));
    user_data.push_back(
        mojom::ServiceWorkerUserData::New(registration_id, "key2", "value2"));

    status = StoreUserData(registration_id, kKey, std::move(user_data));
    ASSERT_EQ(status, DatabaseStatus::kOk);
  }

  // Get user data.
  {
    std::vector<std::string> keys = {"key1", "key2"};
    GetUserDataResult result = GetUserData(registration_id, keys);
    ASSERT_EQ(result.status, DatabaseStatus::kOk);
    EXPECT_EQ(result.values.size(), 2UL);
    EXPECT_EQ("value1", result.values[0]);
    EXPECT_EQ("value2", result.values[1]);
  }

  // Try to get user data with an unknown key should fail.
  {
    std::vector<std::string> keys = {"key1", "key2", "key3"};
    GetUserDataResult result = GetUserData(registration_id, keys);
    ASSERT_EQ(result.status, DatabaseStatus::kErrorNotFound);
    EXPECT_EQ(result.values.size(), 0UL);
  }

  // Clear the first entry.
  {
    std::vector<std::string> keys = {"key1"};
    status = ClearUserData(registration_id, keys);
    ASSERT_EQ(status, DatabaseStatus::kOk);
    GetUserDataResult result = GetUserData(registration_id, keys);
    ASSERT_EQ(result.status, DatabaseStatus::kErrorNotFound);
    EXPECT_EQ(result.values.size(), 0UL);
  }

  // Getting the second entry should succeed.
  {
    std::vector<std::string> keys = {"key2"};
    GetUserDataResult result = GetUserData(registration_id, keys);
    ASSERT_EQ(result.status, DatabaseStatus::kOk);
    EXPECT_EQ(result.values.size(), 1UL);
    EXPECT_EQ("value2", result.values[0]);
  }

  // Delete the registration and store a new registration for the same
  // scope.
  const int64_t new_registration_id = GetNewRegistrationId();
  const int64_t new_version_id = GetNewVersionId().version_id;
  const int64_t new_resource_id = GetNewResourceId();
  {
    DeleteRegistrationResult result = DeleteRegistration(registration_id, kKey);
    ASSERT_EQ(result.status, DatabaseStatus::kOk);

    status = CreateAndStoreRegistration(new_registration_id, new_version_id,
                                        new_resource_id, kScope, kKey,
                                        kScriptUrl, kScriptSize);
    ASSERT_EQ(status, DatabaseStatus::kOk);
  }

  // Try to get user data stored for the previous registration should fail.
  {
    std::vector<std::string> keys = {"key2"};
    GetUserDataResult result = GetUserData(new_registration_id, keys);
    ASSERT_EQ(result.status, DatabaseStatus::kErrorNotFound);
    EXPECT_EQ(result.values.size(), 0UL);
  }
}

// Tests that storing/getting user data by key prefix works.
TEST_F(ServiceWorkerStorageControlImplTest, StoreAndGetUserDataByKeyPrefix) {
  const GURL kScope("https://www.example.com/");
  const blink::StorageKey kKey =
      blink::StorageKey::CreateFirstParty(url::Origin::Create(kScope));
  const GURL kScriptUrl("https://www.example.com/sw.js");
  const int64_t kScriptSize = 10;

  LazyInitializeForTest();

  const int64_t registration_id = GetNewRegistrationId();
  const int64_t version_id = GetNewVersionId().version_id;
  const int64_t resource_id = GetNewResourceId();
  DatabaseStatus status;
  status = CreateAndStoreRegistration(registration_id, version_id, resource_id,
                                      kScope, kKey, kScriptUrl, kScriptSize);
  ASSERT_EQ(status, DatabaseStatus::kOk);

  // Store some user data with prefixes.
  std::vector<mojom::ServiceWorkerUserDataPtr> user_data;
  user_data.push_back(
      mojom::ServiceWorkerUserData::New(registration_id, "prefixA", "value1"));
  user_data.push_back(
      mojom::ServiceWorkerUserData::New(registration_id, "prefixA2", "value2"));
  user_data.push_back(
      mojom::ServiceWorkerUserData::New(registration_id, "prefixB", "value3"));
  user_data.push_back(
      mojom::ServiceWorkerUserData::New(registration_id, "prefixC", "value4"));
  status = StoreUserData(registration_id, kKey, std::move(user_data));
  ASSERT_EQ(status, DatabaseStatus::kOk);

  {
    GetUserDataByKeyPrefixResult result =
        GetUserDataByKeyPrefix(registration_id, "prefix");
    ASSERT_EQ(result.status, DatabaseStatus::kOk);
    EXPECT_EQ(result.values.size(), 4UL);
    EXPECT_EQ(result.values[0], "value1");
    EXPECT_EQ(result.values[1], "value2");
    EXPECT_EQ(result.values[2], "value3");
    EXPECT_EQ(result.values[3], "value4");
  }

  {
    GetUserKeysAndDataByKeyPrefixResult result =
        GetUserKeysAndDataByKeyPrefix(registration_id, "prefix");
    ASSERT_EQ(result.status, DatabaseStatus::kOk);
    ASSERT_EQ(result.user_data.size(), 4UL);
    EXPECT_EQ(result.user_data["A"], "value1");
    EXPECT_EQ(result.user_data["A2"], "value2");
    EXPECT_EQ(result.user_data["B"], "value3");
    EXPECT_EQ(result.user_data["C"], "value4");
  }

  {
    GetUserDataByKeyPrefixResult result =
        GetUserDataByKeyPrefix(registration_id, "prefixA");
    ASSERT_EQ(result.status, DatabaseStatus::kOk);
    ASSERT_EQ(result.values.size(), 2UL);
    EXPECT_EQ(result.values[0], "value1");
    EXPECT_EQ(result.values[1], "value2");
  }

  status = ClearUserDataByKeyPrefixes(registration_id, {"prefixA", "prefixC"});
  ASSERT_EQ(status, DatabaseStatus::kOk);

  {
    GetUserDataByKeyPrefixResult result =
        GetUserDataByKeyPrefix(registration_id, "prefix");
    ASSERT_EQ(result.status, DatabaseStatus::kOk);
    ASSERT_EQ(result.values.size(), 1UL);
    EXPECT_EQ(result.values[0], "value3");
  }
}

// Tests that storing/getting user data for multiple registrations work.
TEST_F(ServiceWorkerStorageControlImplTest,
       StoreAndGetUserDataForAllRegistrations) {
  const GURL kScope1("https://www.example.com/foo");
  const blink::StorageKey kKey1 =
      blink::StorageKey::CreateFirstParty(url::Origin::Create(kScope1));
  const GURL kScriptUrl1("https://www.example.com/foo/sw.js");
  const GURL kScope2("https://www.example.com/bar");
  const blink::StorageKey kKey2 =
      blink::StorageKey::CreateFirstParty(url::Origin::Create(kScope2));
  const GURL kScriptUrl2("https://www.example.com/bar/sw.js");
  const int64_t kScriptSize = 10;

  LazyInitializeForTest();

  // Preparation: Create and store two registrations.
  DatabaseStatus status;
  const int64_t registration_id1 = GetNewRegistrationId();
  const int64_t version_id1 = GetNewVersionId().version_id;
  const int64_t resource_id1 = GetNewResourceId();
  status =
      CreateAndStoreRegistration(registration_id1, version_id1, resource_id1,
                                 kScope1, kKey1, kScriptUrl1, kScriptSize);
  ASSERT_EQ(status, DatabaseStatus::kOk);
  const int64_t registration_id2 = GetNewRegistrationId();
  const int64_t version_id2 = GetNewVersionId().version_id;
  const int64_t resource_id2 = GetNewResourceId();
  status =
      CreateAndStoreRegistration(registration_id2, version_id2, resource_id2,
                                 kScope2, kKey2, kScriptUrl2, kScriptSize);
  ASSERT_EQ(status, DatabaseStatus::kOk);

  // Preparation: Store some user data to registrations. Both registrations have
  // "key1" and "prefixA" keys.
  {
    std::vector<mojom::ServiceWorkerUserDataPtr> user_data;
    user_data.push_back(mojom::ServiceWorkerUserData::New(
        registration_id1, "key1", "registration1_value1"));
    user_data.push_back(mojom::ServiceWorkerUserData::New(
        registration_id1, "key2", "registration1_value2"));
    user_data.push_back(mojom::ServiceWorkerUserData::New(
        registration_id1, "prefix1", "registration1_prefix_value1"));
    status = StoreUserData(registration_id1, kKey1, std::move(user_data));
    ASSERT_EQ(status, DatabaseStatus::kOk);
  }
  {
    std::vector<mojom::ServiceWorkerUserDataPtr> user_data;
    user_data.push_back(mojom::ServiceWorkerUserData::New(
        registration_id1, "key1", "registration2_value1"));
    user_data.push_back(mojom::ServiceWorkerUserData::New(
        registration_id1, "key3", "registration2_value3"));
    user_data.push_back(mojom::ServiceWorkerUserData::New(
        registration_id1, "prefix2", "registration2_prefix_value2"));
    status = StoreUserData(registration_id2, kKey2, std::move(user_data));
    ASSERT_EQ(status, DatabaseStatus::kOk);
  }

  // Get common user data.
  GetUserDataForAllRegistrationsResult result;
  result = GetUserDataForAllRegistrations("key1");
  ASSERT_EQ(result.status, DatabaseStatus::kOk);
  ASSERT_EQ(result.values.size(), 2UL);
  EXPECT_EQ(result.values[0]->registration_id, registration_id1);
  EXPECT_EQ(result.values[0]->value, "registration1_value1");
  EXPECT_EQ(result.values[1]->registration_id, registration_id2);
  EXPECT_EQ(result.values[1]->value, "registration2_value1");

  // Get uncommon user data.
  result = GetUserDataForAllRegistrations("key2");
  ASSERT_EQ(result.status, DatabaseStatus::kOk);
  ASSERT_EQ(result.values.size(), 1UL);
  EXPECT_EQ(result.values[0]->registration_id, registration_id1);
  EXPECT_EQ(result.values[0]->value, "registration1_value2");

  result = GetUserDataForAllRegistrations("key3");
  ASSERT_EQ(result.status, DatabaseStatus::kOk);
  ASSERT_EQ(result.values.size(), 1UL);
  EXPECT_EQ(result.values[0]->registration_id, registration_id2);
  EXPECT_EQ(result.values[0]->value, "registration2_value3");

  // Getting unknown key succeeds but returns an empty value.
  // TODO(bashi): Make sure this is an intentional behavior. The existing
  // unittest expects this behavior.
  result = GetUserDataForAllRegistrations("unknown_key");
  ASSERT_EQ(result.status, DatabaseStatus::kOk);
  EXPECT_EQ(result.values.size(), 0UL);

  // Clear common user data from one registration then get it again.
  // This time only one user data should be found.
  status = ClearUserData(registration_id1, {"key1"});
  ASSERT_EQ(status, DatabaseStatus::kOk);
  result = GetUserDataForAllRegistrations("key1");
  ASSERT_EQ(result.status, DatabaseStatus::kOk);
  ASSERT_EQ(result.values.size(), 1UL);
  EXPECT_EQ(result.values[0]->registration_id, registration_id2);
  EXPECT_EQ(result.values[0]->value, "registration2_value1");

  // Get prefixed user data.
  result = GetUserDataForAllRegistrationsByKeyPrefix("prefix");
  ASSERT_EQ(result.status, DatabaseStatus::kOk);
  ASSERT_EQ(result.values.size(), 2UL);
  EXPECT_EQ(result.values[0]->registration_id, registration_id1);
  EXPECT_EQ(result.values[0]->value, "registration1_prefix_value1");
  EXPECT_EQ(result.values[1]->registration_id, registration_id2);
  EXPECT_EQ(result.values[1]->value, "registration2_prefix_value2");

  // Clear prefixed user data.
  status = ClearUserDataForAllRegistrationsByKeyPrefix("prefix");
  ASSERT_EQ(status, DatabaseStatus::kOk);
  result = GetUserDataForAllRegistrationsByKeyPrefix("prefix");
  ASSERT_EQ(result.status, DatabaseStatus::kOk);
  EXPECT_EQ(result.values.size(), 0UL);
}

// Tests that getting usage for a storage key works at different stages of
// registration resource update.
TEST_F(ServiceWorkerStorageControlImplTest, GetUsageForStorageKey) {
  const GURL kScope("https://www.example.com/");
  const blink::StorageKey kKey =
      blink::StorageKey::CreateFirstParty(url::Origin::Create(kScope));
  const GURL kScriptUrl("https://www.example.com/sw.js");
  const GURL kImportedScriptUrl("https://www.example.com/imported.js");

  LazyInitializeForTest();

  // No data has written yet for a given storage key.
  {
    GetUsageForStorageKeyResult result = GetUsageForStorageKey(kKey);
    ASSERT_EQ(result.status, DatabaseStatus::kOk);
    ASSERT_EQ(result.usage, 0);
  }

  // Preparation: Create a registration with two resources. These aren't written
  // to storage yet.
  std::vector<ResourceRecord> resources;
  const int64_t resource_id1 = GetNewResourceId();
  const std::string resource_data1 = "main script data";
  resources.push_back(mojom::ServiceWorkerResourceRecord::New(
      resource_id1, kScriptUrl, resource_data1.size(),
      /*sha256_checksum=*/std::nullopt));

  const int64_t resource_id2 = GetNewResourceId();
  const std::string resource_data2 = "imported script data";
  resources.push_back(mojom::ServiceWorkerResourceRecord::New(
      resource_id2, kImportedScriptUrl, resource_data2.size(),
      /*sha256_checksum=*/std::nullopt));

  const int64_t registration_id = GetNewRegistrationId();
  const int64_t version_id = GetNewVersionId().version_id;
  RegistrationData registration_data = CreateRegistrationData(
      registration_id, version_id, kScope, kKey, kScriptUrl, resources);

  // Put these resources ids on the uncommitted list in storage.
  DatabaseStatus status;
  status = StoreUncommittedResourceId(resource_id1);
  ASSERT_EQ(status, DatabaseStatus::kOk);
  status = StoreUncommittedResourceId(resource_id2);
  ASSERT_EQ(status, DatabaseStatus::kOk);

  {
    GetUsageForStorageKeyResult result = GetUsageForStorageKey(kKey);
    ASSERT_EQ(result.status, DatabaseStatus::kOk);
    ASSERT_EQ(result.usage, 0)
        << "Resources that aren't stored with the registration "
        << "don't use quota.";
  }

  // Write responses and the registration data.
  {
    int result = WriteResource(resource_id1, resource_data1);
    ASSERT_GT(result, 0);
  }
  {
    int result = WriteResource(resource_id2, resource_data2);
    ASSERT_GT(result, 0);
  }
  status =
      StoreRegistration(std::move(registration_data), std::move(resources));
  ASSERT_EQ(status, DatabaseStatus::kOk);

  // Expect the storage usage for resources stored.
  {
    GetUsageForStorageKeyResult result = GetUsageForStorageKey(kKey);
    ASSERT_EQ(result.status, DatabaseStatus::kOk);
    const int64_t expected_usage =
        resource_data1.size() + resource_data2.size();
    ASSERT_EQ(result.usage, expected_usage);
  }

  // Delete the registration.
  {
    DeleteRegistrationResult result = DeleteRegistration(registration_id, kKey);
    ASSERT_EQ(result.status, DatabaseStatus::kOk);
  }

  // Expect no storage usage.
  {
    GetUsageForStorageKeyResult result = GetUsageForStorageKey(kKey);
    ASSERT_EQ(result.status, DatabaseStatus::kOk);
    ASSERT_EQ(result.usage, 0);
  }
}

// Tests that apply policy updates work.
TEST_F(ServiceWorkerStorageControlImplTest, ApplyPolicyUpdates) {
  const GURL kScope1("https://foo.example.com/");
  const blink::StorageKey kKey1 =
      blink::StorageKey::CreateFirstParty(url::Origin::Create(kScope1));
  const GURL kScriptUrl1("https://foo.example.com/sw.js");
  const GURL kScope2("https://bar.example.com/");
  const blink::StorageKey kKey2 =
      blink::StorageKey::CreateFirstParty(url::Origin::Create(kScope2));
  const GURL kScriptUrl2("https://bar.example.com/sw.js");
  const int64_t kScriptSize = 10;

  LazyInitializeForTest();

  // Preparation: Create and store two registrations.
  DatabaseStatus status;
  const int64_t registration_id1 = GetNewRegistrationId();
  const int64_t version_id1 = GetNewVersionId().version_id;
  const int64_t resource_id1 = GetNewResourceId();
  status =
      CreateAndStoreRegistration(registration_id1, version_id1, resource_id1,
                                 kScope1, kKey1, kScriptUrl1, kScriptSize);
  ASSERT_EQ(status, DatabaseStatus::kOk);
  const int64_t registration_id2 = GetNewRegistrationId();
  const int64_t version_id2 = GetNewVersionId().version_id;
  const int64_t resource_id2 = GetNewResourceId();
  status =
      CreateAndStoreRegistration(registration_id2, version_id2, resource_id2,
                                 kScope2, kKey2, kScriptUrl2, kScriptSize);
  ASSERT_EQ(status, DatabaseStatus::kOk);

  // Update policies to purge the registration for |kScope2| on shutdown.
  std::vector<mojom::StoragePolicyUpdatePtr> updates;
  updates.emplace_back(mojom::StoragePolicyUpdate::New(
      url::Origin::Create(kScope2.DeprecatedGetOriginAsURL()),
      /*purge_on_shutdown=*/true));
  base::RunLoop loop;
  storage()->ApplyPolicyUpdates(
      std::move(updates),
      base::BindLambdaForTesting([&](DatabaseStatus status) {
        EXPECT_EQ(status, DatabaseStatus::kOk);
        loop.Quit();
      }));
  loop.Run();

  // Restart the storage and check the registration for |kScope1| exists
  // but not for |kScope2|.
  RestartStorage();
  {
    FindRegistrationResult result = FindRegistrationForScope(kScope1, kKey1);
    ASSERT_EQ(result.status, DatabaseStatus::kOk);
  }
  {
    FindRegistrationResult result = FindRegistrationForScope(kScope2, kKey2);
    ASSERT_EQ(result.status, DatabaseStatus::kErrorNotFound);
  }
}

TEST_F(ServiceWorkerStorageControlImplTest, TrackRunningVersion) {
  const GURL kScope("https://www.example.com/");
  const blink::StorageKey kKey =
      blink::StorageKey::CreateFirstParty(url::Origin::Create(kScope));
  const GURL kScriptUrl("https://www.example.com/sw.js");
  const GURL kImportedScriptUrl("https://www.example.com/imported.js");

  LazyInitializeForTest();

  // Preparation: Create a registration with two resources (The main script and
  // an imported script).
  int result;
  std::vector<ResourceRecord> resources;
  const int64_t resource_id1 = GetNewResourceId();
  const std::string resource_data1 = "main script data";
  result = WriteResource(resource_id1, resource_data1);
  ASSERT_GT(result, 0);
  resources.push_back(mojom::ServiceWorkerResourceRecord::New(
      resource_id1, kScriptUrl, resource_data1.size(),
      /*sha256_checksum=*/std::nullopt));

  const int64_t resource_id2 = GetNewResourceId();
  const std::string resource_data2 = "imported script data";
  result = WriteResource(resource_id2, resource_data2);
  ASSERT_GT(result, 0);
  resources.push_back(mojom::ServiceWorkerResourceRecord::New(
      resource_id2, kImportedScriptUrl, resource_data2.size(),
      /*sha256_checksum=*/std::nullopt));

  const int64_t registration_id = GetNewRegistrationId();
  GetNewVersionIdResult new_version_id_result = GetNewVersionId();
  ASSERT_NE(new_version_id_result.version_id,
            blink::mojom::kInvalidServiceWorkerVersionId);
  const int64_t version_id = new_version_id_result.version_id;
  RegistrationData registration_data = CreateRegistrationData(
      registration_id, version_id, kScope, kKey, kScriptUrl, resources);
  DatabaseStatus status =
      StoreRegistration(std::move(registration_data), std::move(resources));
  ASSERT_EQ(status, DatabaseStatus::kOk);

  // Create three reference from 1. GetNewVersionId(), 2.
  // FindRegistrationForId(), and 3. GetRegistrationsForStorageKey().
  mojo::Remote<mojom::ServiceWorkerLiveVersionRef> reference1;
  ASSERT_TRUE(new_version_id_result.reference);
  reference1.Bind(std::move(new_version_id_result.reference));

  mojo::Remote<mojom::ServiceWorkerLiveVersionRef> reference2;
  {
    FindRegistrationResult find_result =
        FindRegistrationForId(registration_id, kKey);
    ASSERT_EQ(find_result.status, DatabaseStatus::kOk);
    ASSERT_TRUE(find_result.entry->version_reference);
    reference2.Bind(std::move(find_result.entry->version_reference));
  }

  mojo::Remote<mojom::ServiceWorkerLiveVersionRef> reference3;
  {
    GetRegistrationsForOriginResult get_registrations_result =
        GetRegistrationsForStorageKey(kKey);
    ASSERT_EQ(get_registrations_result.status, DatabaseStatus::kOk);
    ASSERT_EQ(get_registrations_result.registrations.size(), 1UL);
    ASSERT_TRUE(get_registrations_result.registrations[0]->version_reference);
    reference3.Bind(std::move(
        get_registrations_result.registrations[0]->version_reference));
  }

  // Drop the first reference and delete the registration.
  reference1.reset();
  {
    DeleteRegistrationResult delete_result =
        DeleteRegistration(registration_id, kKey);
    ASSERT_EQ(delete_result.status, DatabaseStatus::kOk);
  }

  // Make sure all tasks are ran.
  task_environment().RunUntilIdle();

  // Resources shouldn't be purged because there are two active references.
  {
    ReadDataResult read_resource_result1 =
        ReadResource(resource_id1, resource_data1.size());
    ASSERT_EQ(read_resource_result1.status,
              static_cast<int32_t>(resource_data1.size()));
    EXPECT_EQ(read_resource_result1.data, resource_data1);
    ReadDataResult read_resource_result2 =
        ReadResource(resource_id2, resource_data2.size());
    ASSERT_EQ(read_resource_result2.status,
              static_cast<int32_t>(resource_data2.size()));
    EXPECT_EQ(read_resource_result2.data, resource_data2);
  }

  // Drop the second reference.
  reference2.reset();
  task_environment().RunUntilIdle();

  // Resources shouldn't be purged because there is an active reference yet.
  {
    ReadDataResult read_resource_result1 =
        ReadResource(resource_id1, resource_data1.size());
    ASSERT_EQ(read_resource_result1.status,
              static_cast<int32_t>(resource_data1.size()));
    EXPECT_EQ(read_resource_result1.data, resource_data1);
    ReadDataResult read_resource_result2 =
        ReadResource(resource_id2, resource_data2.size());
    ASSERT_EQ(read_resource_result2.status,
              static_cast<int32_t>(resource_data2.size()));
    EXPECT_EQ(read_resource_result2.data, resource_data2);
  }

  // Drop the third reference.
  reference3.reset();
  task_environment().RunUntilIdle();

  // Resources should have been purged.
  {
    ReadDataResult read_resource_result1 =
        ReadResource(resource_id1, resource_data1.size());
    ASSERT_EQ(read_resource_result1.status, net::ERR_CACHE_MISS);
    EXPECT_EQ(read_resource_result1.data, "");
    ReadDataResult read_resource_result2 =
        ReadResource(resource_id2, resource_data2.size());
    ASSERT_EQ(read_resource_result2.status, net::ERR_CACHE_MISS);
    EXPECT_EQ(read_resource_result2.data, "");
  }
}

}  // namespace storage
