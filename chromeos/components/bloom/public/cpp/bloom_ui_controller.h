// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_BLOOM_PUBLIC_CPP_BLOOM_UI_CONTROLLER_H_
#define CHROMEOS_COMPONENTS_BLOOM_PUBLIC_CPP_BLOOM_UI_CONTROLLER_H_

#include <memory>

#include "base/component_export.h"

namespace chromeos {
namespace bloom {

class BloomUiDelegate;

// Controller for all UI specific elements used by the Bloom feature.
// Implemented as a singleton (which is registered automatically through the
// constructor).
class COMPONENT_EXPORT(BLOOM) BloomUiController {
 public:
  static BloomUiController* Get();

  BloomUiController();
  virtual ~BloomUiController();

  virtual BloomUiDelegate& GetUiDelegate() = 0;

 private:
  static BloomUiController* g_instance_;
};

}  // namespace bloom
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_BLOOM_PUBLIC_CPP_BLOOM_UI_CONTROLLER_H_
