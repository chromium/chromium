// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UPDATE_CLIENT_PATCH_PATCH_IMPL_H_
#define COMPONENTS_UPDATE_CLIENT_PATCH_PATCH_IMPL_H_

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "components/services/patch/public/mojom/file_patcher.mojom.h"
#include "components/update_client/patcher.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace update_client {

class PatchChromiumFactory : public PatcherFactory {
 public:
  using Callback =
      base::RepeatingCallback<mojo::PendingRemote<patch::mojom::FilePatcher>()>;
  explicit PatchChromiumFactory(Callback callback);

  scoped_refptr<Patcher> Create() const override;

 protected:
  ~PatchChromiumFactory() override;

 private:
  const Callback callback_;

  DISALLOW_COPY_AND_ASSIGN(PatchChromiumFactory);
};

}  // namespace update_client

#endif  // COMPONENTS_UPDATE_CLIENT_PATCH_PATCH_IMPL_H_
