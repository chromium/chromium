// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/patch/in_process_file_patcher.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/task/thread_pool.h"
#include "components/services/patch/file_patcher_impl.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace patch {

namespace {

void BindInProcessFilePatcher(
    mojo::PendingReceiver<mojom::FilePatcher> receiver) {
  mojo::MakeSelfOwnedReceiver(std::make_unique<FilePatcherImpl>(),
                              std::move(receiver));
}

}  // namespace

mojo::PendingRemote<mojom::FilePatcher> LaunchInProcessFilePatcher() {
  mojo::PendingRemote<mojom::FilePatcher> remote;
  base::ThreadPool::CreateSequencedTaskRunner(
      {base::MayBlock(), base::WithBaseSyncPrimitives()})
      ->PostTask(FROM_HERE,
                 base::BindOnce(&BindInProcessFilePatcher,
                                remote.InitWithNewPipeAndPassReceiver()));
  return remote;
}

}  // namespace patch
