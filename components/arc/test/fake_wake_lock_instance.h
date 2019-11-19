// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_TEST_FAKE_WAKE_LOCK_INSTANCE_H_
#define COMPONENTS_ARC_TEST_FAKE_WAKE_LOCK_INSTANCE_H_

#include "base/macros.h"
#include "components/arc/mojom/wake_lock.mojom.h"

namespace arc {

class FakeWakeLockInstance : public mojom::WakeLockInstance {
 public:
  FakeWakeLockInstance();
  ~FakeWakeLockInstance() override;

  // mojom::WakeLockInstance overrides:
  void Init(mojom::WakeLockHostPtr host_ptr, InitCallback callback) override;

 private:
  mojom::WakeLockHostPtr host_ptr_;

  DISALLOW_COPY_AND_ASSIGN(FakeWakeLockInstance);
};

}  // namespace arc

#endif  // COMPONENTS_ARC_TEST_FAKE_WAKE_LOCK_INSTANCE_H_
