// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/client_certificates/core/unexportable_private_key_factory.h"

#include "base/memory/scoped_refptr.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/enterprise/client_certificates/core/private_key.h"
#include "crypto/scoped_mock_unexportable_key_provider.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace client_certificates {

TEST(UnexportablePrivateKeyFactoryTest, SupportedCreateKey) {
  base::test::TaskEnvironment task_environment;
  crypto::ScopedMockUnexportableKeyProvider scoped_provider;

  auto factory = UnexportablePrivateKeyFactory::TryCreate();

  ASSERT_TRUE(factory);

  base::test::TestFuture<scoped_refptr<PrivateKey>> test_future;
  factory->CreatePrivateKey(test_future.GetCallback());

  EXPECT_TRUE(test_future.Get());
}

TEST(UnexportablePrivateKeyFactoryTest, UnsupportedCreateKey) {
  crypto::ScopedNullUnexportableKeyProvider scoped_provider;

  auto factory = UnexportablePrivateKeyFactory::TryCreate();

  EXPECT_FALSE(factory);
}

}  // namespace client_certificates
