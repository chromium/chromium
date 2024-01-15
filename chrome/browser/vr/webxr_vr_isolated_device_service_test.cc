// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>

#include "base/functional/callback_helpers.h"
#include "base/test/bind.h"
#include "build/build_config.h"
#include "chrome/browser/vr/test/mock_xr_device_hook_base.h"
#include "chrome/browser/vr/test/multi_class_browser_test.h"
#include "chrome/browser/vr/test/webxr_vr_browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/xr_test_utils.h"

namespace vr {

// Tests that we can recover from a crash/disconnect on the DeviceService
WEBXR_VR_ALL_RUNTIMES_BROWSER_TEST_F(TestDeviceServiceDisconnect) {
  // Ensure that any time the XR Device Service is started, we have installed
  // a new local hook before the IsolatedDeviceProvider has a chance to issue
  // any enumeration requests.
  std::optional<MockXRDeviceHookBase> device_hook(std::in_place);
  content::SetXRDeviceServiceStartupCallbackForTesting(
      base::BindLambdaForTesting([&] { device_hook.emplace(); }));

  t->LoadFileAndAwaitInitialization("test_isolated_device_service_disconnect");

  t->EnterSessionWithUserGestureOrFail();

  // We don't care how many device changes we've received prior to this point.
  // We should now be at a steady state, so what we really care about is the
  // number of device changes after this point.
  t->RunJavaScriptOrFail("resetDeviceChanges()");

  device_hook->TerminateDeviceServiceProcessForTesting();

  // Ensure that we've actually exited the session.
  t->PollJavaScriptBooleanOrFail(
      "sessionInfos[sessionTypes.IMMERSIVE].currentSession === null",
      WebXrVrBrowserTestBase::kPollTimeoutLong);

  // We expect one change indicating the device was disconnected, and then
  // one more indicating that the device was re-connected.
  t->PollJavaScriptBooleanOrFail("deviceChanges === 2",
                                 WebXrVrBrowserTestBase::kPollTimeoutMedium);

  // One last check now that we have the device change that we can actually
  // still enter an immersive session.
  t->EnterSessionWithUserGestureOrFail();

  content::SetXRDeviceServiceStartupCallbackForTesting(base::NullCallback());
}
}  // namespace vr
