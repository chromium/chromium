// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_UNZIP_UNZIPPER_IMPL_H_
#define COMPONENTS_SERVICES_UNZIP_UNZIPPER_IMPL_H_

#include "base/files/file.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "components/services/storage/public/cpp/filesystem/filesystem_impl.h"
#include "components/services/unzip/public/mojom/unzipper.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace unzip {

class UnzipperImpl : public mojom::Unzipper {
 public:
  // Constructs an UnzipperImpl which will be bound to some externally owned
  // Receiver, such as through |mojo::MakeSelfOwnedReceiver()|.
  UnzipperImpl();

  // Constructs an UnzipperImpl bound to |receiver|.
  explicit UnzipperImpl(mojo::PendingReceiver<mojom::Unzipper> receiver);

  UnzipperImpl(const UnzipperImpl&) = delete;
  UnzipperImpl& operator=(const UnzipperImpl&) = delete;

  static void Listener(const mojo::Remote<mojom::UnzipListener>& listener,
                       uint64_t bytes);

  ~UnzipperImpl() override;

 private:
  // unzip::mojom::Unzipper:
  void Unzip(base::File zip_file,
             mojo::PendingRemote<storage::mojom::Directory> output_dir_remote,
             mojom::UnzipOptionsPtr options,
             mojo::PendingRemote<mojom::UnzipFilter> filter_remote,
             mojo::PendingRemote<mojom::UnzipListener> listener_remote,
             UnzipCallback callback) override;

  void DetectEncoding(base::File zip_file,
                      DetectEncodingCallback callback) override;

  void GetExtractedInfo(base::File zip_file,
                        GetExtractedInfoCallback callback) override;

  // Disconnect handler for the receiver.
  void OnReceiverDisconnect();

  // Task runner for ZIP extraction.
  using RunnerPtr = scoped_refptr<base::SequencedTaskRunner>;
  const RunnerPtr runner_ = base::ThreadPool::CreateSequencedTaskRunner(
      {base::MayBlock(), base::WithBaseSyncPrimitives(),
       base::TaskPriority::USER_BLOCKING,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});

  mojo::Receiver<mojom::Unzipper> receiver_{this};

  base::WeakPtrFactory<UnzipperImpl> weak_ptr_factory_{this};
};

}  // namespace unzip

#endif  // COMPONENTS_SERVICES_UNZIP_UNZIPPER_IMPL_H_
