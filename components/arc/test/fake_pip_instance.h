// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_TEST_FAKE_PIP_INSTANCE_H_
#define COMPONENTS_ARC_TEST_FAKE_PIP_INSTANCE_H_

#include "base/macros.h"
#include "components/arc/mojom/pip.mojom.h"

namespace arc {

class FakePipInstance : public mojom::PipInstance {
 public:
  FakePipInstance();
  ~FakePipInstance() override;

  int num_closed() { return num_closed_; }
  base::Optional<bool> suppressed() const { return suppressed_; }

  // mojom::PipInstance overrides:
  void Init(mojom::PipHostPtr host_ptr, InitCallback callback) override;
  void ClosePip() override;
  void SetPipSuppressionStatus(bool suppressed) override;

 private:
  mojom::PipHostPtr host_ptr_;
  int num_closed_ = 0;
  base::Optional<bool> suppressed_;

  DISALLOW_COPY_AND_ASSIGN(FakePipInstance);
};

}  // namespace arc

#endif  // COMPONENTS_ARC_TEST_FAKE_PIP_INSTANCE_H_
