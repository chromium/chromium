// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_CROS_SAFETY_CLOUD_SAFETY_SESSION_H_
#define CHROMEOS_ASH_SERVICES_CROS_SAFETY_CLOUD_SAFETY_SESSION_H_

#include <memory>

#include "chromeos/ash/services/cros_safety/public/mojom/cros_safety.mojom.h"
#include "components/manta/manta_service.h"
#include "components/manta/walrus_provider.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace ash {

// The CloudSafetySession uses the Manta WalrusProvider to connect to the cloud
// classifier server.
class CloudSafetySession : public cros_safety::mojom::CloudSafetySession {
 public:
  using ClassifySafetyCallback =
      base::OnceCallback<void(cros_safety::mojom::SafetyClassifierVerdict)>;

  explicit CloudSafetySession(
      std::unique_ptr<manta::WalrusProvider> walrus_provider);
  CloudSafetySession(const CloudSafetySession&) = delete;
  CloudSafetySession& operator=(const CloudSafetySession&) = delete;
  ~CloudSafetySession() override;

  void AddReceiver(
      mojo::PendingReceiver<cros_safety::mojom::CloudSafetySession> receiver);

  // cros_safety::mojom::CloudSafetySession overrides
  void ClassifyTextSafety(cros_safety::mojom::SafetyRuleset ruleset,
                          const std::string& text,
                          ClassifySafetyCallback callback) override;
  void ClassifyImageSafety(cros_safety::mojom::SafetyRuleset ruleset,
                           const std::optional<std::string>& text,
                           mojo_base::BigBuffer image,
                           ClassifySafetyCallback callback) override;

 private:
  std::unique_ptr<manta::WalrusProvider> walrus_provider_;
  mojo::ReceiverSet<cros_safety::mojom::CloudSafetySession> receiver_set_;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_SERVICES_CROS_SAFETY_CLOUD_SAFETY_SESSION_H_
