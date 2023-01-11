// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UPDATE_CLIENT_UNZIP_UNZIP_IMPL_H_
#define COMPONENTS_UPDATE_CLIENT_UNZIP_UNZIP_IMPL_H_

#include <memory>

#include "base/functional/callback.h"
#include "components/services/unzip/public/mojom/unzipper.mojom.h"
#include "components/update_client/unzipper.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace update_client {

class UnzipChromiumFactory : public UnzipperFactory {
 public:
  using Callback =
      base::RepeatingCallback<mojo::PendingRemote<unzip::mojom::Unzipper>()>;

  explicit UnzipChromiumFactory(Callback callback);

  UnzipChromiumFactory(const UnzipChromiumFactory&) = delete;
  UnzipChromiumFactory& operator=(const UnzipChromiumFactory&) = delete;

  std::unique_ptr<Unzipper> Create() const override;

 protected:
  ~UnzipChromiumFactory() override;

 private:
  const Callback callback_;
};

}  // namespace update_client

#endif  // COMPONENTS_UPDATE_CLIENT_UNZIP_UNZIP_IMPL_H_
