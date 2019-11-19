// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/test/fake_server/entity_builder_factory.h"

#include "base/guid.h"
#include "base/strings/string_number_conversions.h"

using std::string;

namespace fake_server {

EntityBuilderFactory::EntityBuilderFactory()
    : cache_guid_(base::GenerateGUID()) {}

EntityBuilderFactory::EntityBuilderFactory(const string& cache_guid)
    : cache_guid_(cache_guid) {}

EntityBuilderFactory::~EntityBuilderFactory() {}

const BookmarkEntityBuilder EntityBuilderFactory::NewBookmarkEntityBuilder(
    const string& title,
    base::Optional<std::string> originator_client_item_id) {
  if (!originator_client_item_id)
    originator_client_item_id = base::GenerateGUID();

  BookmarkEntityBuilder builder(title, cache_guid_, *originator_client_item_id);
  return builder;
}

}  // namespace fake_server
