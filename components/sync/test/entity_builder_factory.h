// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_TEST_ENTITY_BUILDER_FACTORY_H_
#define COMPONENTS_SYNC_TEST_ENTITY_BUILDER_FACTORY_H_

#include <stdint.h>

#include <optional>
#include <string>

#include "base/uuid.h"
#include "components/sync/test/bookmark_entity_builder.h"

namespace fake_server {

// Creates various types of EntityBuilders.
//
// This class exists because we maintain state related to the fake client that
// supposedly created these entities.
//
// TODO(pvalenzuela): Revisit the naming of this class once the FakeServer
// injection API is widely used. If this class stays bookmark-specific, it
// should be named appropriately.
class EntityBuilderFactory {
 public:
  EntityBuilderFactory();
  explicit EntityBuilderFactory(const std::string& cache_guid);
  virtual ~EntityBuilderFactory();

  // Uses a client tag hash instead of the pair
  // originator_cache_guid/originator_client_item_id in future builders.
  void EnableClientTagHash();

  const std::string& cache_guid() const { return cache_guid_; }

  BookmarkEntityBuilder NewBookmarkEntityBuilder(
      const std::string& title,
      const base::Uuid& uuid = base::Uuid::GenerateRandomV4());
  BookmarkEntityBuilder NewBookmarkEntityBuilder(
      const std::u16string& title,
      const base::Uuid& uuid = base::Uuid::GenerateRandomV4());

 private:
  const std::string cache_guid_;
  bool use_client_tag_hash_ = false;
};

}  // namespace fake_server

#endif  // COMPONENTS_SYNC_TEST_ENTITY_BUILDER_FACTORY_H_
