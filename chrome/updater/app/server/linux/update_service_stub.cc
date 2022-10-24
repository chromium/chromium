// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/app/server/linux/update_service_stub.h"

#include <utility>

#include "base/callback.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/version.h"

namespace updater {

UpdateServiceStub::UpdateServiceStub(
    mojo::PendingReceiver<mojom::UpdateService> receiver,
    scoped_refptr<updater::UpdateService> impl)
    : receiver_(this, std::move(receiver)), impl_(impl) {}

UpdateServiceStub::~UpdateServiceStub() = default;

void UpdateServiceStub::GetVersion(GetVersionCallback callback) {
  impl_->GetVersion(base::BindOnce(
      [](GetVersionCallback callback, const base::Version& version) {
        std::move(callback).Run(version.GetString());
      },
      std::move(callback)));
}

}  // namespace updater
