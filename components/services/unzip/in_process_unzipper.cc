// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/unzip/in_process_unzipper.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/task/thread_pool.h"
#include "components/services/unzip/unzipper_impl.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace unzip {

namespace {

void BindInProcessUnzipper(mojo::PendingReceiver<mojom::Unzipper> receiver) {
  mojo::MakeSelfOwnedReceiver(std::make_unique<UnzipperImpl>(),
                              std::move(receiver));
}

}  // namespace

mojo::PendingRemote<mojom::Unzipper> LaunchInProcessUnzipper() {
  mojo::PendingRemote<mojom::Unzipper> remote;
  base::ThreadPool::CreateSequencedTaskRunner(
      {base::MayBlock(), base::WithBaseSyncPrimitives()})
      ->PostTask(FROM_HERE,
                 base::BindOnce(&BindInProcessUnzipper,
                                remote.InitWithNewPipeAndPassReceiver()));
  return remote;
}

}  // namespace unzip
