// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/storage_service_impl.h"

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "components/services/storage/partition_impl.h"
#include "components/services/storage/public/mojom/partition.mojom.h"
#include "components/services/storage/public/mojom/storage_service.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace storage {

class StorageServiceImplTest : public testing::Test {
 public:
  StorageServiceImplTest() = default;

  StorageServiceImplTest(const StorageServiceImplTest&) = delete;
  StorageServiceImplTest& operator=(const StorageServiceImplTest&) = delete;

  ~StorageServiceImplTest() override = default;

 protected:
  mojom::StorageService* remote_service() { return remote_service_.get(); }
  StorageServiceImpl& service_impl() { return service_; }

 private:
  base::test::TaskEnvironment task_environment_;
  mojo::Remote<mojom::StorageService> remote_service_;
  StorageServiceImpl service_{remote_service_.BindNewPipeAndPassReceiver(),
                              /*io_task_runner=*/nullptr};
};

TEST_F(StorageServiceImplTest, UniqueInMemoryPartitions) {
  // Verifies that every partition client bound without a path is bound to a
  // unique partition instance.

  mojo::Remote<mojom::Partition> in_memory_partition1;
  remote_service()->BindPartition(
      /*path=*/std::nullopt, in_memory_partition1.BindNewPipeAndPassReceiver());
  in_memory_partition1.FlushForTesting();

  EXPECT_EQ(1u, service_impl().partitions().size());

  mojo::Remote<mojom::Partition> in_memory_partition2;
  remote_service()->BindPartition(
      std::nullopt /* path */,
      in_memory_partition2.BindNewPipeAndPassReceiver());
  in_memory_partition2.FlushForTesting();

  EXPECT_EQ(2u, service_impl().partitions().size());

  // Also verify that a new client with a provided path is unique from the above
  // partitions.
  base::ScopedTempDir temp_dir;
  CHECK(temp_dir.CreateUniqueTempDir());

  mojo::Remote<mojom::Partition> persistent_partition;
  remote_service()->BindPartition(
      temp_dir.GetPath(), persistent_partition.BindNewPipeAndPassReceiver());
  persistent_partition.FlushForTesting();

  EXPECT_EQ(3u, service_impl().partitions().size());
}

TEST_F(StorageServiceImplTest, SharedPersistentPartition) {
  // Verifies that multiple clients can share the same persistent partition
  // instance.

  base::ScopedTempDir temp_dir;
  CHECK(temp_dir.CreateUniqueTempDir());

  mojo::Remote<mojom::Partition> client1;
  remote_service()->BindPartition(temp_dir.GetPath(),
                                  client1.BindNewPipeAndPassReceiver());
  client1.FlushForTesting();

  EXPECT_EQ(1u, service_impl().partitions().size());

  mojo::Remote<mojom::Partition> client2;
  remote_service()->BindPartition(temp_dir.GetPath(),
                                  client2.BindNewPipeAndPassReceiver());
  client2.FlushForTesting();

  EXPECT_EQ(1u, service_impl().partitions().size());
  EXPECT_TRUE(client1.is_connected());
  EXPECT_TRUE(client2.is_connected());
}

TEST_F(StorageServiceImplTest, PartitionDestroyedOnLastClientDisconnect) {
  base::ScopedTempDir temp_dir;
  CHECK(temp_dir.CreateUniqueTempDir());

  mojo::Remote<mojom::Partition> client1;
  remote_service()->BindPartition(temp_dir.GetPath(),
                                  client1.BindNewPipeAndPassReceiver());
  client1.FlushForTesting();

  mojo::Remote<mojom::Partition> client2;
  remote_service()->BindPartition(temp_dir.GetPath(),
                                  client2.BindNewPipeAndPassReceiver());
  client2.FlushForTesting();

  EXPECT_EQ(1u, service_impl().partitions().size());

  client1.reset();
  client2.reset();

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(0u, service_impl().partitions().size());
}

TEST_F(StorageServiceImplTest, PersistentPartitionRequiresAbsolutePath) {
  mojo::Remote<mojom::Partition> client;
  const base::FilePath kTestRelativePath{FILE_PATH_LITERAL("invalid")};
  remote_service()->BindPartition(kTestRelativePath,
                                  client.BindNewPipeAndPassReceiver());

  // We should be imminently disconnected because the BindPartition request
  // should be ignored by the service.
  base::RunLoop loop;
  client.set_disconnect_handler(loop.QuitClosure());
  loop.Run();

  EXPECT_FALSE(client.is_connected());
}

}  // namespace storage
