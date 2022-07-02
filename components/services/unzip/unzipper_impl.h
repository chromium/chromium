// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_UNZIP_UNZIPPER_IMPL_H_
#define COMPONENTS_SERVICES_UNZIP_UNZIPPER_IMPL_H_

#include "base/files/file.h"
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

  ~UnzipperImpl() override;

 private:
  // unzip::mojom::Unzipper:
  void Unzip(
      base::File zip_file,
      mojo::PendingRemote<filesystem::mojom::Directory> output_dir_remote,
      mojom::UnzipOptionsPtr options,
      mojo::PendingRemote<mojom::UnzipFilter> filter_remote,
      mojo::PendingRemote<mojom::UnzipListener> listener_remote,
      UnzipCallback callback) override;

  void DetectEncoding(base::File zip_file,
                      DetectEncodingCallback callback) override;

  void GetExtractedInfo(base::File zip_file,
                        GetExtractedInfoCallback callback) override;

  static void Listener(const mojo::Remote<mojom::UnzipListener>& listener,
                       uint64_t bytes);

  mojo::Receiver<mojom::Unzipper> receiver_{this};
};

}  // namespace unzip

#endif  // COMPONENTS_SERVICES_UNZIP_UNZIPPER_IMPL_H_
