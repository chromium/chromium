// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_TEST_FAKE_CAST_RECEIVER_INSTANCE_H_
#define COMPONENTS_ARC_TEST_FAKE_CAST_RECEIVER_INSTANCE_H_

#include <string>

#include "base/optional.h"
#include "components/arc/mojom/cast_receiver.mojom.h"

namespace arc {

class FakeCastReceiverInstance : public mojom::CastReceiverInstance {
 public:
  FakeCastReceiverInstance();
  FakeCastReceiverInstance(const FakeCastReceiverInstance&) = delete;
  FakeCastReceiverInstance& operator=(const FakeCastReceiverInstance&) = delete;
  ~FakeCastReceiverInstance() override;

  // mojom::CastReceiverInstance overrides:
  using GetNameCallback =
      base::OnceCallback<void(mojom::CastReceiverInstance::Result,
                              const std::string&)>;
  void GetName(GetNameCallback callback) override;

  using SetEnabledCallback =
      base::OnceCallback<void(mojom::CastReceiverInstance::Result)>;
  void SetEnabled(bool enabled, SetEnabledCallback callback) override;

  using SetNameCallback =
      base::OnceCallback<void(mojom::CastReceiverInstance::Result)>;
  void SetName(const std::string& name, SetNameCallback callback) override;

  const base::Optional<bool>& last_enabled() const { return last_enabled_; }
  const base::Optional<std::string>& last_name() const { return last_name_; }

 private:
  base::Optional<bool> last_enabled_;
  base::Optional<std::string> last_name_;
};

}  // namespace arc

#endif  // COMPONENTS_ARC_TEST_FAKE_CAST_RECEIVER_INSTANCE_H_
