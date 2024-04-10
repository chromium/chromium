// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/chromeos/crosier/gaia_host_util.h"

#include <string>

#include "chrome/browser/ash/login/test/js_checker.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ash/login/test/oobe_screens_utils.h"
#include "chrome/browser/ash/login/wizard_controller.h"
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

void SkipToGaiaScreenAndWait() {
  ash::test::WaitForOobeJSReady();

  // Skip to Gaia screen and wait it to become current.
  ash::WizardController::default_controller()->SkipToLoginForTesting();
  ash::OobeScreenWaiter(ash::GaiaView::kScreenId).Wait();

  // Wait for Gaia page to be ready and update properties..
  const std::string check_gaia_js = R"((function() {
    gaiaSignin = $('gaia-signin');
    return !gaiaSignin.hidden && gaiaSignin.uiStep === 'online-gaia' &&
        !gaiaSignin.loadingFrameContents && gaiaSignin.showViewProcessed;
  })())";
  ash::test::OobeJS().CreateWaiter(check_gaia_js)->Wait();
}

}  // namespace crosier
