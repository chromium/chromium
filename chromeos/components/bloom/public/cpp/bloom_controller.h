// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_BLOOM_PUBLIC_CPP_BLOOM_CONTROLLER_H_
#define CHROMEOS_COMPONENTS_BLOOM_PUBLIC_CPP_BLOOM_CONTROLLER_H_

#include <memory>

#include "base/component_export.h"
#include "chromeos/components/bloom/public/cpp/bloom_interaction_resolution.h"

namespace chromeos {
namespace bloom {

// Main controller for the Bloom integration.
class COMPONENT_EXPORT(BLOOM) BloomController {
 public:
  BloomController();
  BloomController(const BloomController&) = delete;
  BloomController& operator=(const BloomController&) = delete;
  virtual ~BloomController();

  // Access the existing Bloom controller.
  static BloomController* Get();

  // Starts an interaction. This will ask the user for a screenshot, analyze the
  // content and display the result.
  virtual void StartInteraction() = 0;
  virtual bool HasInteraction() const = 0;
  virtual void StopInteraction(BloomInteractionResolution resolution) = 0;
};

}  // namespace bloom
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_BLOOM_PUBLIC_CPP_BLOOM_CONTROLLER_H_
