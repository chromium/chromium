// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_DB_V4_TEST_UTIL_H_
#define COMPONENTS_SAFE_BROWSING_DB_V4_TEST_UTIL_H_

// Contains classes and methods useful for tests.

#include <map>
#include <memory>
#include <ostream>
#include <vector>

#include "components/safe_browsing/db/v4_database.h"
#include "components/safe_browsing/db/v4_get_hash_protocol_manager.h"

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

  bool HasValidData() const override;

  void MarkPrefixAsBad(HashPrefix prefix);

  // |prefixes| does not need to be sorted.
  void SetPrefixes(std::vector<HashPrefix> prefixes, PrefixSize size);

 private:
  // Holds mock prefixes from calls to MarkPrefixAsBad / SetPrefixes. Stored as
  // a vector for simplicity.
  std::map<PrefixSize, std::vector<HashPrefix>> mock_prefixes_;
};

class TestV4StoreFactory : public V4StoreFactory {
 public:
  TestV4StoreFactory();
  ~TestV4StoreFactory() override;

  std::unique_ptr<V4Store> CreateV4Store(
      const scoped_refptr<base::SequencedTaskRunner>& task_runner,
      const base::FilePath& store_path) override;
};

class TestV4Database : public V4Database {
 public:
  TestV4Database(const scoped_refptr<base::SequencedTaskRunner>& db_task_runner,
                 std::unique_ptr<StoreMap> store_map);

  void MarkPrefixAsBad(ListIdentifier list_id, HashPrefix prefix);
};

class TestV4DatabaseFactory : public V4DatabaseFactory {
 public:
  TestV4DatabaseFactory();
  ~TestV4DatabaseFactory() override;

  std::unique_ptr<V4Database> Create(
      const scoped_refptr<base::SequencedTaskRunner>& db_task_runner,
      std::unique_ptr<StoreMap> store_map) override;

  void MarkPrefixAsBad(ListIdentifier list_id, HashPrefix prefix);

 private:
  // Owned by V4LocalDatabaseManager. The following usage is expected: each
  // test in the test fixture instantiates a new SafebrowsingService instance,
  // which instantiates a new V4LocalDatabaseManager, which instantiates a new
  // V4Database using this method so use-after-free isn't possible.
  TestV4Database* v4_db_ = nullptr;
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
  TestV4GetHashProtocolManager* pm_ = nullptr;
};

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

#endif  // COMPONENTS_SAFE_BROWSING_DB_V4_TEST_UTIL_H_
