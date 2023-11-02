// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_WIN_WRL_MODULE_INITIALIZER_H_
#define CHROME_UPDATER_WIN_WRL_MODULE_INITIALIZER_H_

#include <wrl/module.h>

namespace updater {

// Allows one time creation of the WRL::Module instance. The WRL library
// contains a global instance of a class, which must be created exactly once.
class WRLModuleInitializer {
 public:
  WRLModuleInitializer() {
    Microsoft::WRL::Module<Microsoft::WRL::OutOfProc>::GetModule();
  }

  static const WRLModuleInitializer& Get() {
    // WRLModuleInitializer has a trivial destructor.
    static const WRLModuleInitializer module;
    return module;
  }
};
}  // namespace updater

#endif  // CHROME_UPDATER_WIN_WRL_MODULE_INITIALIZER_H_
