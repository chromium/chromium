// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_UI_CONTACTS_PICKER_ANDROID_CONTACTS_PICKER_IMAGE_DECODER_H_
#define COMPONENTS_BROWSER_UI_CONTACTS_PICKER_ANDROID_CONTACTS_PICKER_IMAGE_DECODER_H_

#include <cstdint>

#include "third_party/skia/include/core/SkBitmap.h"

namespace browser_ui {

// Resizes `bitmap` to `desired_size` x `desired_size` if `desired_size > 0`.
// Any non-empty `bitmap` must have `kN32_SkColorType` (as guaranteed by
// `data_decoder::DecodeImage` via `skia.mojom.BitmapN32`).
SkBitmap ScaleContactIconBitmap(const SkBitmap& bitmap, int32_t desired_size);

}  // namespace browser_ui

#endif  // COMPONENTS_BROWSER_UI_CONTACTS_PICKER_ANDROID_CONTACTS_PICKER_IMAGE_DECODER_H_
