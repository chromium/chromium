// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/filesystem/directory_test_helper.h"

#include <memory>
#include <utility>

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "components/services/filesystem/directory_impl.h"
#include "components/services/filesystem/public/mojom/directory.mojom.h"
#include "components/services/filesystem/shared_temp_dir.h"
#include "mojo/public/cpp/bindings/unique_receiver_set.h"

namespace filesystem {

class DirectoryTestHelper::BlockingState {
 public:
  BlockingState() = default;
  ~BlockingState() = default;

  BlockingState(const BlockingState&) = delete;
  BlockingState& operator=(const BlockingState&) = delete;

  void BindNewTempDirectory(mojo::PendingReceiver<mojom::Directory> receiver) {
    auto temp_dir = std::make_unique<base::ScopedTempDir>();
    CHECK(temp_dir->CreateUniqueTempDir());
    base::FilePath path = temp_dir->GetPath();
    directories_.Add(
        std::make_unique<DirectoryImpl>(
            path, base::MakeRefCounted<SharedTempDir>(std::move(temp_dir))),
        std::move(receiver));
  }

 private:
  mojo::UniqueReceiverSet<mojom::Directory> directories_;
};

DirectoryTestHelper::DirectoryTestHelper()
    : blocking_state_(
          base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()})) {}

DirectoryTestHelper::~DirectoryTestHelper() = default;

mojo::Remote<mojom::Directory> DirectoryTestHelper::CreateTempDir() {
  mojo::Remote<mojom::Directory> remote;
  blocking_state_.AsyncCall(&BlockingState::BindNewTempDirectory)
      .WithArgs(remote.BindNewPipeAndPassReceiver());
  return remote;
}

}  // namespace filesystem
