// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_FEATURED_FAKE_FEATURED_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_FEATURED_FAKE_FEATURED_CLIENT_H_

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "chromeos/ash/components/dbus/featured/featured_client.h"
#include "components/variations/proto/cros_safe_seed.pb.h"

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

  // Sets the callback parameter for `HandleSeedFetched`.
  void SetCallbackSuccess(bool success);

  // |callback| is run with true by default. Call |SetCallbackSuccess|
  // to change the callback parameter.
  void HandleSeedFetched(
      const variations::SeedDetails& safe_seed,
      base::OnceCallback<void(bool success)> callback) override;

 private:
  bool success_ = true;
};

}  // namespace ash::featured

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_FEATURED_FAKE_FEATURED_CLIENT_H_
