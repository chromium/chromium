// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/extensions/cast_kiosk_delegate.h"

namespace extensions {

CastKioskDelegate::CastKioskDelegate() {}

CastKioskDelegate::~CastKioskDelegate() {}

bool CastKioskDelegate::IsAutoLaunchedKioskApp(const ExtensionId& id) const {
  return false;
}

}  // namespace extensions
