// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/local_search_service/local_search_service_proxy.h"

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "chromeos/components/local_search_service/index_proxy.h"
#include "chromeos/components/local_search_service/local_search_service_sync.h"
#include "chromeos/components/local_search_service/mojom/types.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace local_search_service {

class LocalSearchServiceProxyTest : public testing::Test {
 public:
  LocalSearchServiceProxyTest() {
    service_proxy_ = std::make_unique<LocalSearchServiceProxy>(&service_);
    service_proxy_->BindReceiver(service_remote_.BindNewPipeAndPassReceiver());
  }

 protected:
  mojo::Remote<mojom::LocalSearchServiceProxy> service_remote_;

 private:
  base::test::TaskEnvironment task_environment_;

  LocalSearchServiceSync service_;
  std::unique_ptr<LocalSearchServiceProxy> service_proxy_;
};

TEST_F(LocalSearchServiceProxyTest, GetIndex) {
  mojo::Remote<mojom::IndexProxy> index_remote;
  service_remote_->GetIndex(IndexId::kCrosSettings, Backend::kLinearMap,
                            index_remote.BindNewPipeAndPassReceiver());
  base::RunLoop().RunUntilIdle();

  // Check that IndexRemote is bound.
  bool callback_done = false;
  uint64_t num_items;
  index_remote->GetSize(base::BindOnce(
      [](bool* callback_done, uint64_t* num_items, uint64_t size) {
        *callback_done = true;
        *num_items = size;
      },
      &callback_done, &num_items));
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(callback_done);
  EXPECT_EQ(num_items, 0U);
}

}  // namespace local_search_service
}  // namespace chromeos
