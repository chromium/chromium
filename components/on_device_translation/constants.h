// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef COMPONENTS_ON_DEVICE_TRANSLATION_CONSTANTS_H_
#define COMPONENTS_ON_DEVICE_TRANSLATION_CONSTANTS_H_

#include <cstdint>
#include <iterator>

#include "crypto/sha2.h"

namespace component_updater {
// The SHA256 of the SubjectPublicKeyInfo used to sign the component.
// The component id is: lbimbicckdokpoicboneldipejkhjgdg
extern const uint8_t kTranslateKitPublicKeySHA256[32];
}  // namespace component_updater

static_assert(std::size(component_updater::kTranslateKitPublicKeySHA256) ==
                  crypto::kSHA256Length,
              "Wrong hash length");

namespace on_device_translation {

// The maximum number of pending tasks in the task queue in
// OnDeviceTranslationServiceController. When the number of pending tasks will
// exceed this limit, the request will fail.
extern const size_t kMaxPendingTaskCount;

}  // namespace on_device_translation

#endif  // COMPONENTS_ON_DEVICE_TRANSLATION_CONSTANTS_H_
