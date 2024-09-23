// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/ash/interactive/webui/interactive_uitest_elements.h"

namespace ash::webui::bluetooth {

WebContentsInteractionTestUtil::DeepQuery PairingDialog() {
  return WebContentsInteractionTestUtil::DeepQuery({{
      "bluetooth-pairing-dialog",
  }});
}

WebContentsInteractionTestUtil::DeepQuery PairingDialogDeviceSelectionPage() {
  return PairingDialog() + "bluetooth-pairing-ui" +
         "bluetooth-pairing-device-selection-page";
}

WebContentsInteractionTestUtil::DeepQuery PairingDialogEnterCodePage() {
  return PairingDialog() + "bluetooth-pairing-ui" +
         "bluetooth-pairing-enter-code-page" + "bluetooth-base-page";
}

}  // namespace ash::webui::bluetooth
