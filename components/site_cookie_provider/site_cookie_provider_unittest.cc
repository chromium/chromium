// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/site_cookie_provider/site_cookie_provider.h"

#include "base/test/task_environment.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace site_cookie_provider {

TEST(SiteCookieProviderTest, InstantiationStub) {
  base::test::TaskEnvironment task_environment;
  network::TestURLLoaderFactory test_url_loader_factory;
  auto shared_url_loader_factory =
      base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
          &test_url_loader_factory);

  auto provider = SiteCookieProvider::Create(
      mojo::PendingRemote<network::mojom::CookieManager>(),
      shared_url_loader_factory);

  EXPECT_TRUE(provider);
}

}  // namespace site_cookie_provider
