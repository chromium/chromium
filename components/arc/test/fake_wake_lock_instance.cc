// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "components/arc/test/fake_wake_lock_instance.h"

namespace arc {

FakeWakeLockInstance::FakeWakeLockInstance() = default;

FakeWakeLockInstance::~FakeWakeLockInstance() = default;

void FakeWakeLockInstance::Init(
    mojo::PendingRemote<mojom::WakeLockHost> host_remote,
    InitCallback callback) {
  host_remote_.reset();
  host_remote_.Bind(std::move(host_remote));
  std::move(callback).Run();
}

}  // namespace arc
