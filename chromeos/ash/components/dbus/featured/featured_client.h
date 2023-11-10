// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_FEATURED_FEATURED_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_FEATURED_FEATURED_CLIENT_H_

#include "base/component_export.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/metrics/field_trial.h"
#include "chromeos/ash/components/dbus/featured/featured.pb.h"

namespace dbus {
class Bus;
}

namespace ash::featured {

// FeaturedClient is used to:
//  * communicate seed state with featured, which is used for enabling and
//  managing platform specific features.
//  * record early-boot trials to UMA by listening for early-boot trial files
//  written by featured.
//
// Its main user is Chrome field trials.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_DBUS_FEATURED) FeaturedClient {
 public:
  // Callback type for ListenForActiveEarlyBootTrials(), which takes in the
  // trial name and group name of a field trial.
  using ListenForTrialCallback =
      base::RepeatingCallback<void(const std::string& trial_name,
                                   const std::string& group_name)>;

  // Creates and initializes the global instance. |bus| must not be null.
  static void Initialize(dbus::Bus* bus);

  // Creates and initializes the global instance for testing. |bus| must not be
  // null.
  static void InitializeForTesting(dbus::Bus* bus,
                                   const base::FilePath& expected_dir,
                                   ListenForTrialCallback callback);

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
      const ::featured::SeedDetails& safe_seed,
      base::OnceCallback<void(bool success)> callback) = 0;

  // Returns true if the base component of |path| is of the format
  // `TrialName,GroupName`. |active_group| will contain the trial name and group
  // name specified by the filename. If the return value is false,
  // |active_group| will not be modified.
  //
  // The platform-side logic that writes these files can be found in
  // feature::PlatformFeatures::RecordActiveTrial() in chromiumos.
  static bool ParseTrialFilename(const base::FilePath& path,
                                 base::FieldTrial::ActiveGroup& active_group);

 protected:
  // Initialize/Shutdown should be used instead.
  FeaturedClient();
  virtual ~FeaturedClient();

 private:
  friend class FeaturedClientTest;
};
}  // namespace ash::featured

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_FEATURED_FEATURED_CLIENT_H_
