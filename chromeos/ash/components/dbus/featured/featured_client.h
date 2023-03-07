// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_FEATURED_FEATURED_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_FEATURED_FEATURED_CLIENT_H_

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "components/variations/proto/cros_safe_seed.pb.h"

namespace dbus {
class Bus;
}

namespace ash::featured {

// FeaturedClient is used to communicate seed state with featured, which is used
// for enabling and managing platform specific features. Its main user is Chrome
// field trials.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_DBUS_FEATURED) FeaturedClient {
 public:
  // Creates and initializes the global instance. |bus| must not be null.
  static void Initialize(dbus::Bus* bus);

  // Creates and initializes a fake global instance.
  static void InitializeFake();

  // Destroys the global instance.
  static void Shutdown();

  // Returns the global instance which may be null if not initialized.
  static FeaturedClient* Get();

  FeaturedClient(const FeaturedClient&) = delete;
  FeaturedClient& operator=(const FeaturedClient&) = delete;

  // Asynchronously calls featured's `HandleSeedFetched`.
  virtual void HandleSeedFetched(
      const variations::SeedDetails& safe_seed,
      base::OnceCallback<void(bool success)> callback) = 0;

 protected:
  // Initialize/Shutdown should be used instead.
  FeaturedClient();
  virtual ~FeaturedClient();
};
}  // namespace ash::featured

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_FEATURED_FEATURED_CLIENT_H_
