// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr.h"
#include "components/services/storage/storage_service_impl.h"

#include "base/containers/contains.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "components/services/storage/partition_impl.h"
#include "components/services/storage/public/mojom/partition.mojom.h"
#include "components/services/storage/public/mojom/storage_service.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace storage {

class StorageServicePartitionImplTest : public testing::Test {
 public:
  StorageServicePartitionImplTest() = default;

  StorageServicePartitionImplTest(const StorageServicePartitionImplTest&) =
      delete;
  StorageServicePartitionImplTest& operator=(
      const StorageServicePartitionImplTest&) = delete;

  ~StorageServicePartitionImplTest() override = default;

  void SetUp() override {
    remote_service_->BindPartition(
        std::nullopt, remote_test_partition_.BindNewPipeAndPassReceiver());
    remote_test_partition_.FlushForTesting();

    ASSERT_EQ(1u, service_.partitions().size());
    test_partition_impl_ = service_.partitions().begin()->get();
  }

 protected:
  mojom::Partition* remote_test_partition() {
    return remote_test_partition_.get();
  }
  PartitionImpl* test_partition_impl() { return test_partition_impl_; }

 private:
  base::test::TaskEnvironment task_environment_;
  mojo::Remote<mojom::StorageService> remote_service_;
  StorageServiceImpl service_{remote_service_.BindNewPipeAndPassReceiver(),
                              /*io_task_runner=*/nullptr};
  mojo::Remote<mojom::Partition> remote_test_partition_;
  raw_ptr<PartitionImpl> test_partition_impl_ = nullptr;
};

TEST_F(StorageServicePartitionImplTest, IndependentOriginContexts) {
  // Verifies that clients for unique origins get bound to unique OriginContext
  // backends.

  const url::Origin kTestOrigin1 =
      url::Origin::Create(GURL("http://example.com"));
  mojo::Remote<mojom::OriginContext> context1;
  remote_test_partition()->BindOriginContext(
      kTestOrigin1, context1.BindNewPipeAndPassReceiver());
  context1.FlushForTesting();
  EXPECT_EQ(1u, test_partition_impl()->origin_contexts().size());

  const url::Origin kTestOrigin2 =
      url::Origin::Create(GURL("https://google.com"));
  mojo::Remote<mojom::OriginContext> context2;
  remote_test_partition()->BindOriginContext(
      kTestOrigin2, context2.BindNewPipeAndPassReceiver());
  context2.FlushForTesting();
  EXPECT_EQ(2u, test_partition_impl()->origin_contexts().size());

  EXPECT_TRUE(context1.is_connected());
  EXPECT_TRUE(context2.is_connected());

  // Verify that |context1| was connected to the backend for |kTestOrigin1| by
  // disconnecting |context1| and waiting for the backend to be destroyed.
  context1.reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(
      base::Contains(test_partition_impl()->origin_contexts(), kTestOrigin2));
  EXPECT_FALSE(
      base::Contains(test_partition_impl()->origin_contexts(), kTestOrigin1));

  // Same for |context2|.
  context2.reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(
      base::Contains(test_partition_impl()->origin_contexts(), kTestOrigin2));
}

TEST_F(StorageServicePartitionImplTest, SingleOriginMultipleClients) {
  // Verifies that multiple clients can bind a connection to the same
  // OriginContext within a Partition.

  const url::Origin kTestOrigin =
      url::Origin::Create(GURL("http://example.com"));
  mojo::Remote<mojom::OriginContext> context1;
  remote_test_partition()->BindOriginContext(
      kTestOrigin, context1.BindNewPipeAndPassReceiver());
  context1.FlushForTesting();
  EXPECT_EQ(1u, test_partition_impl()->origin_contexts().size());

  mojo::Remote<mojom::OriginContext> context2;
  remote_test_partition()->BindOriginContext(
      kTestOrigin, context2.BindNewPipeAndPassReceiver());
  context2.FlushForTesting();
  EXPECT_EQ(1u, test_partition_impl()->origin_contexts().size());

  EXPECT_TRUE(context1.is_connected());
  EXPECT_TRUE(context2.is_connected());
}

TEST_F(StorageServicePartitionImplTest,
       OriginContextDestroyedOnLastClientDisconnect) {
  const url::Origin kTestOrigin =
      url::Origin::Create(GURL("http://example.com"));
  mojo::Remote<mojom::OriginContext> context1;
  remote_test_partition()->BindOriginContext(
      kTestOrigin, context1.BindNewPipeAndPassReceiver());
  context1.FlushForTesting();

  mojo::Remote<mojom::OriginContext> context2;
  remote_test_partition()->BindOriginContext(
      kTestOrigin, context2.BindNewPipeAndPassReceiver());
  context2.FlushForTesting();

  EXPECT_EQ(1u, test_partition_impl()->origin_contexts().size());

  context1.reset();
  context2.reset();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(0u, test_partition_impl()->origin_contexts().size());
}

}  // namespace storage
