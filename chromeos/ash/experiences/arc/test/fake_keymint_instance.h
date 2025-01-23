// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_EXPERIENCES_ARC_TEST_FAKE_KEYMINT_INSTANCE_H_
#define CHROMEOS_ASH_EXPERIENCES_ARC_TEST_FAKE_KEYMINT_INSTANCE_H_

#include "chromeos/ash/experiences/arc/mojom/keymint.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace arc {

class FakeKeyMintInstance : public mojom::keymint::KeyMintInstance {
 public:
  FakeKeyMintInstance();
  FakeKeyMintInstance(const FakeKeyMintInstance&) = delete;
  FakeKeyMintInstance& operator=(const FakeKeyMintInstance&) = delete;
  ~FakeKeyMintInstance() override;

  // mojom::KeyMintInstance overrides:
  void Init(mojo::PendingRemote<mojom::keymint::KeyMintHost> host_remote,
            InitCallback callback) override;

  size_t num_init_called() const { return num_init_called_; }

 private:
  mojo::Remote<mojom::keymint::KeyMintHost> host_remote_;
  size_t num_init_called_ = 0;
};

}  // namespace arc

#endif  // CHROMEOS_ASH_EXPERIENCES_ARC_TEST_FAKE_KEYMINT_INSTANCE_H_
