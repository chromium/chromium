// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/proto_database_provider_test_base.h"

namespace optimization_guide {

ProtoDatabaseProviderTestBase::ProtoDatabaseProviderTestBase() = default;
ProtoDatabaseProviderTestBase::~ProtoDatabaseProviderTestBase() = default;

void ProtoDatabaseProviderTestBase::SetUp() {
  ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  db_provider_ = std::make_unique<leveldb_proto::ProtoDatabaseProvider>(
      temp_dir_.GetPath());
}

void ProtoDatabaseProviderTestBase::TearDown() {
  db_provider_.reset();
}

}  // namespace optimization_guide
