// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_ASH_INTERACTIVE_WEBUI_INTERACTIVE_UITEST_ELEMENTS_H_
#define CHROME_TEST_BASE_ASH_INTERACTIVE_WEBUI_INTERACTIVE_UITEST_ELEMENTS_H_

#include "chrome/test/interaction/webcontents_interaction_test_util.h"

namespace ash::webui::bluetooth {

// The Bluetooth pairing dialog.
WebContentsInteractionTestUtil::DeepQuery PairingDialog();

// The root element of the list of devices shown in the Bluetooth pairing
// dialog. This is used to execute JavaScript against the device list.
WebContentsInteractionTestUtil::DeepQuery PairingDialogDeviceSelectionPage();

// The root element of the page in the Bluetooth pairing dialog where the user
// is prompted to input a code on the peripheral being paired.
WebContentsInteractionTestUtil::DeepQuery PairingDialogEnterCodePage();

}  // namespace ash::webui::bluetooth

#endif  // CHROME_TEST_BASE_ASH_INTERACTIVE_WEBUI_INTERACTIVE_UITEST_ELEMENTS_H_
