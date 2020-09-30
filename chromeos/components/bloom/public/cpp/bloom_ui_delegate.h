// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_BLOOM_PUBLIC_CPP_BLOOM_UI_DELEGATE_H_
#define CHROMEOS_COMPONENTS_BLOOM_PUBLIC_CPP_BLOOM_UI_DELEGATE_H_

#include "chromeos/components/bloom/public/cpp/bloom_interaction_resolution.h"

namespace chromeos {
namespace bloom {

class BloomUiDelegate {
 public:
  virtual ~BloomUiDelegate() = default;

  virtual void OnInteractionStarted() = 0;

  // Called when the Assistant UI should be shown.
  virtual void OnShowUI() = 0;

  // Called when the result is ready.
  virtual void OnShowResult(const std::string& html) = 0;

  virtual void OnInteractionFinished(BloomInteractionResolution resolution) = 0;
};

}  // namespace bloom
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_BLOOM_PUBLIC_CPP_BLOOM_UI_DELEGATE_H_
