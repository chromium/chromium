// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/chromeos/crosier/gaia_host_util.h"

#include "chrome/browser/ash/login/test/gaia_page_event_waiter.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ui/webui/ash/login/gaia_screen_handler.h"
#include "chrome/browser/ui/webui/signin/signin_utils.h"
#include "content/public/browser/render_frame_host.h"

namespace crosier {

content::RenderFrameHost* GetGaiaHost() {
  constexpr char kGaiaFrameParentId[] = "signin-frame";
  return signin::GetAuthFrame(
      ash::LoginDisplayHost::default_host()->GetOobeWebContents(),
      kGaiaFrameParentId);
}

ash::test::JSChecker GaiaFrameJS() {
  return ash::test::JSChecker(GetGaiaHost());
}

void WaitForGaia() {
  // Waits for Gaia screen.
  ash::OobeScreenWaiter(ash::GaiaView::kScreenId).Wait();

  // Wait for Gaia page to be ready and update properties..
  constexpr char kAuthenticatorId[] = "$('gaia-signin').authenticator";
  ash::GaiaPageEventWaiter(kAuthenticatorId, "ready").Wait();
  ash::GaiaPageEventWaiter(kAuthenticatorId, "backButton").Wait();
}

}  // namespace crosier
