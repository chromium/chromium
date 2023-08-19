// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_STARTUP_BROWSER_POSTLOGIN_PARAMS_H_
#define CHROMEOS_STARTUP_BROWSER_POSTLOGIN_PARAMS_H_

#include "base/files/platform_file.h"
#include "base/no_destructor.h"
#include "chromeos/crosapi/mojom/crosapi.mojom.h"

namespace chromeos {

// Stores and handles BrowserPostLoginParams.
// This class is not to be used directly - use BrowserParamsProxy instead.
class COMPONENT_EXPORT(CHROMEOS_STARTUP) BrowserPostLoginParams {
 public:
  BrowserPostLoginParams(const BrowserPostLoginParams&) = delete;
  BrowserPostLoginParams& operator=(const BrowserPostLoginParams&) = delete;

  // Wait for the user to login and post-login parameters to be available.
  // NOTE: This needs to be called before parameters are accessed.
  // Please note that this method is not thread-safe and should be called
  // before any threads are created in the browser process.
  static void WaitForLogin();

  // Sets `postlogin_params_` to the provided value.
  // Useful for tests that cannot setup a full Lacros test environment with a
  // working Mojo connection to Ash.
  static void SetPostLoginParamsForTests(
      crosapi::mojom::BrowserPostLoginParamsPtr postlogin_params);

  // Create Mem FD from `postlogin_params_`. This must be called after
  // `postlogin_params_` has initialized by calling GetInstance().
  static base::ScopedFD CreatePostLoginData();

 private:
  friend base::NoDestructor<BrowserPostLoginParams>;

  // Needs to access |Get()|.
  friend class BrowserParamsProxy;

  // Returns BrowserPostLoginParams which is passed from ash-chrome. On
  // launching lacros-chrome from ash-chrome, ash-chrome creates an anonymous
  // pipe and the forked/executed lacros-chrome process inherits the file
  // descriptor. The serialized BrowserPostLoginParams is written in the pipe
  // after login.
  // NOTE: You should use BrowserProxyParams to access parameters instead.
  static const crosapi::mojom::BrowserPostLoginParams* Get();

  static BrowserPostLoginParams* GetInstanceInternal();

  BrowserPostLoginParams();
  ~BrowserPostLoginParams();

  // Parameters passed from ash-chrome.
  crosapi::mojom::BrowserPostLoginParamsPtr postlogin_params_;
};

}  // namespace chromeos

#endif  // CHROMEOS_STARTUP_BROWSER_POSTLOGIN_PARAMS_H_
