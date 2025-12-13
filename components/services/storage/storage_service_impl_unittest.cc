// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/storage_service_impl.h"

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "components/services/storage/public/mojom/local_storage_control.mojom.h"
#include "components/services/storage/public/mojom/session_storage_control.mojom.h"
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

TEST_F(StorageServiceImplTest, BindingLocalStorageControlRequiresAbsolutePath) {
  mojo::Remote<storage::mojom::LocalStorageControl> storage_control;
  const base::FilePath kTestRelativePath{FILE_PATH_LITERAL("invalid")};
  remote_service()->BindLocalStorageControl(
      kTestRelativePath, storage_control.BindNewPipeAndPassReceiver());

  // The BindLocalStorageControl request should be ignored by the service,
  // resulting in disconnection.
  base::RunLoop loop;
  storage_control.set_disconnect_handler(loop.QuitClosure());
  loop.Run();

  EXPECT_FALSE(storage_control.is_connected());
}

TEST_F(StorageServiceImplTest,
       BindingSessionStorageControlRequiresAbsolutePath) {
  mojo::Remote<storage::mojom::SessionStorageControl> storage_control;
  const base::FilePath kTestRelativePath{FILE_PATH_LITERAL("invalid")};
  remote_service()->BindSessionStorageControl(
      kTestRelativePath, storage_control.BindNewPipeAndPassReceiver());

  // The BindSessionStorageControl request should be ignored by the service,
  // resulting in disconnection.
  base::RunLoop loop;
  storage_control.set_disconnect_handler(loop.QuitClosure());
  loop.Run();

  EXPECT_FALSE(storage_control.is_connected());
}

}  // namespace storage
