// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef COMPONENTS_ON_DEVICE_TRANSLATION_CONSTANTS_H_
#define COMPONENTS_ON_DEVICE_TRANSLATION_CONSTANTS_H_

#include <array>
#include <cstdint>

#include "crypto/hash.h"

namespace component_updater {
// The SHA256 of the SubjectPublicKeyInfo used to sign the component.
// The component id is: lbimbicckdokpoicboneldipejkhjgdg
extern const std::array<uint8_t, crypto::hash::kSha256Size>
    kTranslateKitPublicKeySHA256;
}  // namespace component_updater


namespace on_device_translation {

// The maximum number of pending tasks in the task queue in
// OnDeviceTranslationServiceController. When the number of pending tasks will
// exceed this limit, the request will fail.
extern const size_t kMaxPendingTaskCount;

}  // namespace on_device_translation

#endif  // COMPONENTS_ON_DEVICE_TRANSLATION_CONSTANTS_H_
