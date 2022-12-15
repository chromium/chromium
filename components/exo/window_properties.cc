// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/window_properties.h"

DEFINE_UI_CLASS_PROPERTY_TYPE(exo::ProtectedNativePixmapQueryDelegate*)

namespace exo {

DEFINE_OWNED_UI_CLASS_PROPERTY_KEY(std::string, kApplicationIdKey, nullptr)

DEFINE_UI_CLASS_PROPERTY_KEY(bool, kRestoreOrMaximizeExitsFullscreen, false)

DEFINE_UI_CLASS_PROPERTY_KEY(ProtectedNativePixmapQueryDelegate*,
                             kProtectedNativePixmapQueryDelegate,
                             nullptr)

DEFINE_UI_CLASS_PROPERTY_KEY(bool, kMaximumSizeForResizabilityOnly, false)

}  // namespace exo
