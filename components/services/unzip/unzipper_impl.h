// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_UNZIP_UNZIPPER_IMPL_H_
#define COMPONENTS_SERVICES_UNZIP_UNZIPPER_IMPL_H_

#include <memory>

#include "base/files/file.h"
#include "base/macros.h"
#include "components/services/unzip/public/mojom/unzipper.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace unzip {

class UnzipperImpl : public mojom::Unzipper {
 public:
  // Constructs an UnzipperImpl which will be bound to some externally owned
  // Receiver, such as through |mojo::MakeSelfOwnedReceiver()|.
  UnzipperImpl();

  // Constructs an UnzipperImpl bound to |receiver|.
  explicit UnzipperImpl(mojo::PendingReceiver<mojom::Unzipper> receiver);

  ~UnzipperImpl() override;

 private:
  // unzip::mojom::Unzipper:
  void Unzip(
      base::File zip_file,
      mojo::PendingRemote<filesystem::mojom::Directory> output_dir_remote,
      UnzipCallback callback) override;

  void UnzipWithFilter(
      base::File zip_file,
      mojo::PendingRemote<filesystem::mojom::Directory> output_dir_remote,
      mojo::PendingRemote<mojom::UnzipFilter> filter_remote,
      UnzipWithFilterCallback callback) override;

  mojo::Receiver<mojom::Unzipper> receiver_{this};

  DISALLOW_COPY_AND_ASSIGN(UnzipperImpl);
};

}  // namespace unzip

#endif  // COMPONENTS_SERVICES_UNZIP_UNZIPPER_IMPL_H_
