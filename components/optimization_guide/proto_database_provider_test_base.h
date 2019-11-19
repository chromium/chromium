// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_PROTO_DATABASE_PROVIDER_TEST_BASE_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_PROTO_DATABASE_PROVIDER_TEST_BASE_H_

#include "base/files/scoped_temp_dir.h"
#include "components/leveldb_proto/public/proto_database_provider.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace optimization_guide {

// Test class that creates a ScopedTempDir, a SimpleFactoryKey and a
// ProtoDatabaseProvider on each run, it is used because many tests need to
// create a HintCacheStore, which needs a ProtoDatabaseProvider to initialize
// its database.
class ProtoDatabaseProviderTestBase : public testing::Test {
 public:
  ProtoDatabaseProviderTestBase();
  ~ProtoDatabaseProviderTestBase() override;

  void SetUp() override;
  void TearDown() override;

 protected:
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<leveldb_proto::ProtoDatabaseProvider> db_provider_;
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_PROTO_DATABASE_PROVIDER_TEST_BASE_H_
