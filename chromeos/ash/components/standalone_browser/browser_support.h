// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_STANDALONE_BROWSER_BROWSER_SUPPORT_H_
#define CHROMEOS_ASH_COMPONENTS_STANDALONE_BROWSER_BROWSER_SUPPORT_H_

#include "base/component_export.h"

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

 private:
  BrowserSupport();
  ~BrowserSupport();
};

}  // namespace ash::standalone_browser

#endif  // CHROMEOS_ASH_COMPONENTS_STANDALONE_BROWSER_BROWSER_SUPPORT_H_
