// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_STANDALONE_BROWSER_BROWSER_SUPPORT_H_
#define CHROMEOS_ASH_COMPONENTS_STANDALONE_BROWSER_BROWSER_SUPPORT_H_

#include "base/auto_reset.h"
#include "base/component_export.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash::standalone_browser {

// Class encapsulating the state of Lacros browser support.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_STANDALONE_BROWSER)
    BrowserSupport {
 public:
  // Initializes the global instance of BrowserSupport.
  static void Initialize();
  // Destroys the global instance of BrowserSupport.
  static void Shutdown();

  // Returns the global instance of BrowserSupport.
  static BrowserSupport* Get();

  // Returns whether CPU of this device is capable to run standalone browser.
  // Can be called even before Initialize() is called.
  static bool IsCpuSupported();

  // Directly sets the value to be returned by IsCpuSupported for testing.
  // Setting nullopt unsets the overridden behavior of IsCpuSupported.
  static void SetCpuSupportedForTesting(absl::optional<bool> value);

 private:
  BrowserSupport();
  ~BrowserSupport();
};

}  // namespace ash::standalone_browser

#endif  // CHROMEOS_ASH_COMPONENTS_STANDALONE_BROWSER_BROWSER_SUPPORT_H_
