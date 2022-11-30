// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/model/platform_toast.h"

namespace vr {

PlatformToast::PlatformToast() = default;

PlatformToast::PlatformToast(std::u16string t) : text(t) {}

}  // namespace vr
