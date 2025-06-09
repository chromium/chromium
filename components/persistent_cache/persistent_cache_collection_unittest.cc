// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/persistent_cache/persistent_cache_collection.h"

#include "base/containers/span.h"
#include "base/files/scoped_temp_dir.h"
#include "components/persistent_cache/backend_params_manager.h"
#include "components/persistent_cache/entry.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace persistent_cache {
namespace {

TEST(PersistentCacheCollection, Retrieval) {
  base::ScopedTempDir temp_dir;
  CHECK(temp_dir.CreateUniqueTempDir());
  PersistentCacheCollection collection(
      std::make_unique<BackendParamsManager>(temp_dir.GetPath()));

  constexpr char first_cache_id[] = "first_cache_id";
  constexpr char second_cache_id[] = "second_cache_id";

  constexpr char first_key[] = "first_key";
  constexpr char second_key[] = "second_key";

  constexpr const char first_content[] = "first_content";

  // At first there is nothing in the collection.
  EXPECT_EQ(collection.Find(first_cache_id, first_key), nullptr);
  EXPECT_EQ(collection.Find(first_cache_id, second_key), nullptr);
  EXPECT_EQ(collection.Find(second_cache_id, first_key), nullptr);
  EXPECT_EQ(collection.Find(second_cache_id, second_key), nullptr);

  // Inserting for a certain cache id allows retrieval for this id and this id
  // only.
  collection.Insert(first_cache_id, first_key,
                    base::byte_span_from_cstring(first_content));
  auto entry = collection.Find(first_cache_id, first_key);
  ASSERT_NE(entry, nullptr);
  EXPECT_EQ(entry->GetContentSpan(),
            base::byte_span_from_cstring(first_content));
  EXPECT_EQ(collection.Find(second_cache_id, first_key), nullptr);
}

TEST(PersistentCacheCollection, RetrievalAfterClear) {
  base::ScopedTempDir temp_dir;
  CHECK(temp_dir.CreateUniqueTempDir());
  PersistentCacheCollection collection(
      std::make_unique<BackendParamsManager>(temp_dir.GetPath()));

  std::string first_cache_id = "first_cache_id";
  std::string first_key = "first_key";
  constexpr const char first_content[] = "first_content";

  // Test basic retrieval.
  EXPECT_EQ(collection.Find(first_cache_id, first_key), nullptr);
  collection.Insert(first_cache_id, first_key,
                    base::byte_span_from_cstring(first_content));
  EXPECT_NE(collection.Find(first_cache_id, first_key), nullptr);

  // Retrieval still works after clear because data persistence is unnafected by
  // lifetime of PersistentCache instances.
  collection.ClearForTesting();
  EXPECT_NE(collection.Find(first_cache_id, first_key), nullptr);
}

TEST(PersistentCacheCollection, DeleteAllFiles) {
  base::ScopedTempDir temp_dir;
  CHECK(temp_dir.CreateUniqueTempDir());
  PersistentCacheCollection collection(
      std::make_unique<BackendParamsManager>(temp_dir.GetPath()));

  std::string first_cache_id = "first_cache_id";
  std::string first_key = "first_key";
  constexpr const char first_content[] = "first_content";

  // Inserting an entry makes it available.
  collection.Insert(first_cache_id, first_key,
                    base::byte_span_from_cstring(first_content));
  EXPECT_NE(collection.Find(first_cache_id, first_key), nullptr);

  collection.DeleteAllFiles();

  // After deletion the content is not available anymore.
  EXPECT_EQ(collection.Find(first_cache_id, first_key), nullptr);
}

}  // namespace
}  // namespace persistent_cache
