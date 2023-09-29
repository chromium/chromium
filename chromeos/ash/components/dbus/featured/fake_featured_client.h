// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_FEATURED_FAKE_FEATURED_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_FEATURED_FAKE_FEATURED_CLIENT_H_

#include "base/component_export.h"
#include "base/containers/queue.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "chromeos/ash/components/dbus/featured/featured.pb.h"
#include "chromeos/ash/components/dbus/featured/featured_client.h"

namespace ash::featured {

// Fake implementation of FeaturedClient.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_DBUS_FEATURED) FakeFeaturedClient
    : public FeaturedClient {
 public:
  FakeFeaturedClient();

  FakeFeaturedClient(const FakeFeaturedClient&) = delete;
  FakeFeaturedClient& operator=(const FakeFeaturedClient&) = delete;

  ~FakeFeaturedClient() override;

  // Returns the global FakeFeaturedClient instance. Returns `nullptr` if
  // it is not initialized.
  static FakeFeaturedClient* Get();

  // |callback| is run with the first response in `responses_`. Call
  // `AddResponse` to add a response. |callback| is invoked with false if not
  // enough responses are provided.
  void HandleSeedFetched(
      const ::featured::SeedDetails& safe_seed,
      base::OnceCallback<void(bool success)> callback) override;

  // Adds a response to call `HandleSeedFetched` with.
  void AddResponse(bool success);

  // Returns the number of times `HandleSeedFetched` was called. Used for
  // testing.
  int handle_seed_fetched_attempts() const {
    return handle_seed_fetched_attempts_;
  }

  // Returns the safe seed received from HandleSeedFetched. Used for testing.
  const ::featured::SeedDetails& latest_safe_seed() const {
    return latest_safe_seed_;
  }

 private:
  base::queue<bool> responses_;
  size_t handle_seed_fetched_attempts_ = 0;
  ::featured::SeedDetails latest_safe_seed_;
};

}  // namespace ash::featured

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_FEATURED_FAKE_FEATURED_CLIENT_H_
