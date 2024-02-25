// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_STARTUP_BROWSER_INIT_PARAMS_H_
#define CHROMEOS_STARTUP_BROWSER_INIT_PARAMS_H_

#include "base/no_destructor.h"
#include "chromeos/crosapi/mojom/crosapi.mojom.h"

namespace chromeos {

// Stores and handles BrowserInitParams.
// This class is not to be used directly - use BrowserParamsProxy instead.
class COMPONENT_EXPORT(CHROMEOS_STARTUP) BrowserInitParams {
 public:
  BrowserInitParams(const BrowserInitParams&) = delete;
  BrowserInitParams& operator=(const BrowserInitParams&) = delete;

  // Returns BrowserInitParams which is passed from ash-chrome.
  // Useful for tests. This should generally be called only after
  // BrowserTestBase::SetUp.
  // Production code always needs to go through BrowserParamsProxy instead.
  static const crosapi::mojom::BrowserInitParams* GetForTests();

  // Sets `init_params_` to the provided value.
  // Useful for tests that cannot setup a full Lacros test environment with a
  // working Mojo connection to Ash.
  static void SetInitParamsForTests(
      crosapi::mojom::BrowserInitParamsPtr init_params);

  // Create Mem FD from `init_params_`.
  static base::ScopedFD CreateStartupData();

  // This will always be false in production.
  static bool IsCrosapiDisabledForTesting();

  // Use sparingly. This should be needed only in exceptional cases. In
  // particular, Lacros unit_tests and browser_tests have crosapi disabled by
  // default and don't need to call this.
  //
  // This action cannot be undone, so it must be used only by tests that run in
  // separate processes. (However, crosapi is only enabled in such tests
  // anyways, hence this is not really a restriction.)
  //
  // See also README.md.
  static void DisableCrosapiForTesting();

 private:
  friend base::NoDestructor<BrowserInitParams>;

  // Needs to access |Get()|.
  friend class BrowserParamsProxy;

  // Returns BrowserInitParams which is passed from ash-chrome. On launching
  // lacros-chrome from ash-chrome, ash-chrome creates a memory backed file,
  // serializes the BrowserInitParams to it, and the forked/executed
  // lacros-chrome process inherits the file descriptor. The data is read
  // in the constructor so is available from the beginning.
  // NOTE: You should use BrowserParamsProxy to access init params instead.
  static const crosapi::mojom::BrowserInitParams* Get();

  static BrowserInitParams* GetInstance();

  BrowserInitParams();
  ~BrowserInitParams();

  static bool IsCrosapiEnabled();

  static std::optional<bool> is_crosapi_enabled_;

  // Parameters passed from ash-chrome (unless crosapi is disabled).
  crosapi::mojom::BrowserInitParamsPtr init_params_;
};

}  // namespace chromeos

#endif  // CHROMEOS_STARTUP_BROWSER_INIT_PARAMS_H_
