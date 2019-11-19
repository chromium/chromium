// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/filesystem/directory_test_helper.h"

#include <memory>
#include <utility>

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/task/post_task.h"
#include "base/test/bind_test_util.h"
#include "components/services/filesystem/directory_impl.h"
#include "components/services/filesystem/lock_table.h"
#include "components/services/filesystem/public/mojom/directory.mojom.h"
#include "components/services/filesystem/shared_temp_dir.h"
#include "mojo/public/cpp/bindings/unique_receiver_set.h"

namespace filesystem {

class DirectoryTestHelper::BlockingState {
 public:
  BlockingState() : lock_table_(base::MakeRefCounted<LockTable>()) {}
  ~BlockingState() = default;

  void BindNewTempDirectory(mojo::PendingReceiver<mojom::Directory> receiver) {
    auto temp_dir = std::make_unique<base::ScopedTempDir>();
    CHECK(temp_dir->CreateUniqueTempDir());
    base::FilePath path = temp_dir->GetPath();
    directories_.Add(
        std::make_unique<DirectoryImpl>(
            path, base::MakeRefCounted<SharedTempDir>(std::move(temp_dir)),
            lock_table_),
        std::move(receiver));
  }

 private:
  const scoped_refptr<LockTable> lock_table_;
  mojo::UniqueReceiverSet<mojom::Directory> directories_;

  DISALLOW_COPY_AND_ASSIGN(BlockingState);
};

DirectoryTestHelper::DirectoryTestHelper()
    : blocking_state_(base::CreateSequencedTaskRunner(
          {base::ThreadPool(), base::MayBlock()})) {}

DirectoryTestHelper::~DirectoryTestHelper() = default;

mojo::Remote<mojom::Directory> DirectoryTestHelper::CreateTempDir() {
  mojo::Remote<mojom::Directory> remote;
  blocking_state_.Post(FROM_HERE, &BlockingState::BindNewTempDirectory,
                       remote.BindNewPipeAndPassReceiver());
  return remote;
}

}  // namespace filesystem
