// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/test/entity_builder_factory.h"

#include "base/strings/string_number_conversions.h"
#include "base/uuid.h"

using std::string;

namespace fake_server {

EntityBuilderFactory::EntityBuilderFactory()
    : cache_guid_(base::Uuid::GenerateRandomV4().AsLowercaseString()) {}

EntityBuilderFactory::EntityBuilderFactory(const string& cache_guid)
    : cache_guid_(cache_guid) {}

EntityBuilderFactory::~EntityBuilderFactory() = default;

BookmarkEntityBuilder EntityBuilderFactory::NewBookmarkEntityBuilder(
    const string& title,
    std::optional<std::string> originator_client_item_id) {
  if (!originator_client_item_id) {
    originator_client_item_id =
        base::Uuid::GenerateRandomV4().AsLowercaseString();
  }

  BookmarkEntityBuilder builder(title, cache_guid_, *originator_client_item_id);
  return builder;
}

}  // namespace fake_server
