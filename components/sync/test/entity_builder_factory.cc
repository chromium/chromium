// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/test/entity_builder_factory.h"

#include "base/base64.h"
#include "base/rand_util.h"
#include "base/uuid.h"

namespace fake_server {
namespace {

std::string GenerateCacheGUID() {
  // Generate a GUID with 128 bits of randomness.
  const int kGuidBytes = 128 / 8;
  return base::Base64Encode(base::RandBytesAsVector(kGuidBytes));
}

}  // namespace

EntityBuilderFactory::EntityBuilderFactory()
    : cache_guid_(GenerateCacheGUID()) {}

EntityBuilderFactory::EntityBuilderFactory(const std::string& cache_guid)
    : cache_guid_(cache_guid) {}

EntityBuilderFactory::~EntityBuilderFactory() = default;

void EntityBuilderFactory::EnableClientTagHash() {
  use_client_tag_hash_ = true;
}

BookmarkEntityBuilder EntityBuilderFactory::NewBookmarkEntityBuilder(
    const std::string& title,
    const base::Uuid& uuid) {
  auto builder = BookmarkEntityBuilder(title, uuid, cache_guid_);
  if (use_client_tag_hash_) {
    builder.EnableClientTagHash();
  }
  return builder;
}

}  // namespace fake_server
