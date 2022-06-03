// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_EXTENSIONS_CAST_KIOSK_DELEGATE_H_
#define CHROMECAST_BROWSER_EXTENSIONS_CAST_KIOSK_DELEGATE_H_

#include "extensions/browser/kiosk/kiosk_delegate.h"
#include "extensions/common/extension_id.h"

namespace extensions {

// Delegate in Cast that provides an extension/app API with Kiosk mode
// functionality.
class CastKioskDelegate : public KioskDelegate {
 public:
  CastKioskDelegate();
  ~CastKioskDelegate() override;

  // KioskDelegate overrides:
  bool IsAutoLaunchedKioskApp(const ExtensionId& id) const override;
};

}  // namespace extensions

#endif  // CHROMECAST_BROWSER_EXTENSIONS_CAST_KIOSK_DELEGATE_H_
