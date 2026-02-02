// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include <cstddef>
#include <cstdint>

namespace component_updater {
// The SHA256 of the SubjectPublicKeyInfo used to sign the component.
// The component id is: lbimbicckdokpoicboneldipejkhjgdg
extern const uint8_t kTranslateKitPublicKeySHA256[32] = {
    0xb1, 0x8c, 0x18, 0x22, 0xa3, 0xea, 0xfe, 0x82, 0x1e, 0xd4, 0xb3,
    0x8f, 0x49, 0xa7, 0x96, 0x36, 0x55, 0xf3, 0xbc, 0x0d, 0xa5, 0x67,
    0x48, 0x09, 0xcd, 0x7b, 0xa9, 0x5f, 0xd8, 0x7f, 0x53, 0xb4};
}  // namespace component_updater

namespace on_device_translation {

extern const size_t kMaxPendingTaskCount = 1024;

}  // namespace on_device_translation
