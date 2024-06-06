// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_ASH_INTERACTIVE_SETTINGS_INTERACTIVE_UITEST_ELEMENTS_H_
#define CHROME_TEST_BASE_ASH_INTERACTIVE_SETTINGS_INTERACTIVE_UITEST_ELEMENTS_H_

#include "chrome/test/interaction/webcontents_interaction_test_util.h"

namespace ash::settings {

// Top-level internet page.
WebContentsInteractionTestUtil::DeepQuery InternetPage();

namespace cellular {

// The message shown to the user that provides information about the current
// step of the eSIM installation flow.
WebContentsInteractionTestUtil::DeepQuery EsimDialogInstallingMessage();

// The first eSIM profile in the list of eSIM profiles found via an SM-DS scan.
WebContentsInteractionTestUtil::DeepQuery EsimDialogFirstProfile();

// The "forward" button of the "add eSIM" dialog. When pressed, this button will
// either navigate the user forward in the eSIM installation flow.
WebContentsInteractionTestUtil::DeepQuery EsimDialogForwardButton();

// The title of the "add eSIM" dialog.
WebContentsInteractionTestUtil::DeepQuery EsimDialogTitle();

// The "add eSIM" dialog.
WebContentsInteractionTestUtil::DeepQuery EsimDialog();

// The "add eSIM" button on the cellular page.
WebContentsInteractionTestUtil::DeepQuery AddEsimButton();

// The cellular "row" on the top-level internet page.
WebContentsInteractionTestUtil::DeepQuery CellularSummaryItem();

}  // namespace cellular

namespace ethernet {
// The ethernet "row" on the top-level internet page.
WebContentsInteractionTestUtil::DeepQuery EthernetSummaryItem();
}  // namespace ethernet

namespace hotspot {
// The hotspot "row" on the top-level internet page.
WebContentsInteractionTestUtil::DeepQuery HotspotSummaryItem();
}  // namespace hotspot

namespace wifi {
// The wifi "row" on the top-level internet page.
WebContentsInteractionTestUtil::DeepQuery WifiSummaryItem();
}  // namespace wifi

namespace vpn {
// The vpn "row" on the top-level internet page.
WebContentsInteractionTestUtil::DeepQuery VpnSummaryItem();
}  // namespace vpn

}  // namespace ash::settings

#endif  // CHROME_TEST_BASE_ASH_INTERACTIVE_SETTINGS_INTERACTIVE_UITEST_ELEMENTS_H_
