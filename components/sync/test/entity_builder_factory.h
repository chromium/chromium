// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_TEST_ENTITY_BUILDER_FACTORY_H_
#define COMPONENTS_SYNC_TEST_ENTITY_BUILDER_FACTORY_H_

#include <stdint.h>

#include <optional>
#include <string>

#include "base/strings/string_util.h"
#include "components/sync/test/bookmark_entity_builder.h"
#include "url/gurl.h"

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

  BookmarkEntityBuilder NewBookmarkEntityBuilder(
      const std::string& title,
      std::optional<std::string> originator_client_item_id = std::nullopt);

 private:
  // An identifier used when creating entities. This value is used similarly to
  // the value in the legacy Sync Directory code.
  const std::string cache_guid_;
};

}  // namespace fake_server

#endif  // COMPONENTS_SYNC_TEST_ENTITY_BUILDER_FACTORY_H_
