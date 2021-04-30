// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_TEST_FAKE_TIMER_INSTANCE_H_
#define COMPONENTS_ARC_TEST_FAKE_TIMER_INSTANCE_H_

#include "components/arc/mojom/timer.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace arc {

class FakeTimerInstance : public mojom::TimerInstance {
 public:
  FakeTimerInstance();
  ~FakeTimerInstance() override;

  // mojom::TimerInstance overrides:
  void Init(mojo::PendingRemote<mojom::TimerHost> host_remote,
            InitCallback callback) override;

  mojom::TimerHost* GetTimerHost() const;

 private:
  mojo::Remote<mojom::TimerHost> host_remote_;

  DISALLOW_COPY_AND_ASSIGN(FakeTimerInstance);
};

}  // namespace arc

#endif  // COMPONENTS_ARC_TEST_FAKE_TIMER_INSTANCE_H_
