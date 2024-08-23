// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CORE_BROWSER_DB_V4_TEST_UTIL_H_
#define COMPONENTS_SAFE_BROWSING_CORE_BROWSER_DB_V4_TEST_UTIL_H_

// Contains classes and methods useful for tests.

#include <map>
#include <memory>
#include <ostream>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "components/safe_browsing/core/browser/db/v4_database.h"
#include "components/safe_browsing/core/browser/db/v4_get_hash_protocol_manager.h"

namespace safe_browsing {

struct ThreatMetadata;
struct V4ProtocolConfig;

V4ProtocolConfig GetTestV4ProtocolConfig(bool disable_auto_update = false);

std::ostream& operator<<(std::ostream& os, const ThreatMetadata& meta);

class TestV4Store : public V4Store {
 public:
  TestV4Store(const scoped_refptr<base::SequencedTaskRunner>& task_runner,
              const base::FilePath& store_path);
  ~TestV4Store() override;

  bool HasValidData() override;

  void MarkPrefixAsBad(HashPrefixStr prefix);

  // |prefixes| does not need to be sorted.
  void SetPrefixes(std::vector<HashPrefixStr> prefixes, PrefixSize size);

  HashPrefixStr GetMatchingHashPrefix(const FullHashStr& full_hash) override;

 private:
  // Holds mock prefixes from calls to MarkPrefixAsBad / SetPrefixes. Stored as
  // a vector for simplicity.
  std::map<PrefixSize, std::vector<HashPrefixStr>> mock_prefixes_;
};

class TestV4StoreFactory : public V4StoreFactory {
 public:
  TestV4StoreFactory();
  ~TestV4StoreFactory() override;

  V4StorePtr CreateV4Store(
      const scoped_refptr<base::SequencedTaskRunner>& task_runner,
      const base::FilePath& store_path) override;
};

class TestV4Database : public V4Database {
 public:
  TestV4Database(const scoped_refptr<base::SequencedTaskRunner>& db_task_runner,
                 std::unique_ptr<StoreMap> store_map);

  void MarkPrefixAsBad(ListIdentifier list_id, HashPrefixStr prefix);

  // V4Database implementation
  int64_t GetStoreSizeInBytes(const ListIdentifier& store) const override;
};

class TestV4DatabaseFactory : public V4DatabaseFactory {
 public:
  TestV4DatabaseFactory();
  ~TestV4DatabaseFactory() override;

  std::unique_ptr<V4Database, base::OnTaskRunnerDeleter> Create(
      const scoped_refptr<base::SequencedTaskRunner>& db_task_runner,
      std::unique_ptr<StoreMap> store_map) override;

  void MarkPrefixAsBad(ListIdentifier list_id, HashPrefixStr prefix);

  bool IsReady();

 private:
  // Owned by V4LocalDatabaseManager. The following usage is expected: each
  // test in the test fixture instantiates a new SafebrowsingService instance,
  // which instantiates a new V4LocalDatabaseManager, which instantiates a new
  // V4Database using this method so use-after-free isn't possible.
  raw_ptr<TestV4Database, AcrossTasksDanglingUntriaged> v4_db_ = nullptr;
};

class TestV4GetHashProtocolManager : public V4GetHashProtocolManager {
 public:
  TestV4GetHashProtocolManager(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const StoresToCheck& stores_to_check,
      const V4ProtocolConfig& config);

  void AddToFullHashCache(FullHashInfo fhi);
};

class TestV4GetHashProtocolManagerFactory
    : public V4GetHashProtocolManagerFactory {
 public:
  TestV4GetHashProtocolManagerFactory();
  ~TestV4GetHashProtocolManagerFactory() override;

  std::unique_ptr<V4GetHashProtocolManager> CreateProtocolManager(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const StoresToCheck& stores_to_check,
      const V4ProtocolConfig& config) override;

  void AddToFullHashCache(FullHashInfo fhi) { pm_->AddToFullHashCache(fhi); }

 private:
  // Owned by the SafeBrowsingService.
  raw_ptr<TestV4GetHashProtocolManager, AcrossTasksDanglingUntriaged> pm_ =
      nullptr;
};

struct TestV4HashResponseInfo {
  explicit TestV4HashResponseInfo(FullHashStr full_hash,
                                  ListIdentifier list_id);
  TestV4HashResponseInfo(const TestV4HashResponseInfo& other);
  TestV4HashResponseInfo& operator=(const TestV4HashResponseInfo& other) =
      default;
  ~TestV4HashResponseInfo();

  struct KeyValue {
   public:
    explicit KeyValue(const std::string key, const std::string value);
    KeyValue(const KeyValue& other);
    KeyValue& operator=(const KeyValue& other) = default;
    ~KeyValue();

    std::string key;
    std::string value;

   private:
    KeyValue();
  };

  FullHashStr full_hash;
  ListIdentifier list_id;
  std::vector<KeyValue> key_values;

 private:
  TestV4HashResponseInfo();
};
// Converts the |response_infos| into a serialized version of a
// |FindFullHashesResponse|. It also adds values for the cache durations.
std::string GetV4HashResponse(
    std::vector<TestV4HashResponseInfo> response_infos);

// Returns FullHashInfo object for the basic host+path pattern for a given URL
// after canonicalization.
FullHashInfo GetFullHashInfo(const GURL& url, const ListIdentifier& list_id);

// Returns a FullHashInfo info for the basic host+path pattern for a given URL
// after canonicalization. Also adds metadata information to the FullHashInfo
// object.
FullHashInfo GetFullHashInfoWithMetadata(const GURL& url,
                                         const ListIdentifier& list_id,
                                         const ThreatMetadata& threat_metadata);

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CORE_BROWSER_DB_V4_TEST_UTIL_H_
