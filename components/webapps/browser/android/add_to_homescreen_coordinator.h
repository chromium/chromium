// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAPPS_BROWSER_ANDROID_ADD_TO_HOMESCREEN_COORDINATOR_H_
#define COMPONENTS_WEBAPPS_BROWSER_ANDROID_ADD_TO_HOMESCREEN_COORDINATOR_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "components/webapps/browser/android/add_to_homescreen_installer.h"

namespace webapps {

struct AddToHomescreenParams;
class AppBannerManager;

// AddToHomescreenCoordinator is the C++ counterpart of org.chromium.chrome.
// browser.webapps.addtohomescreen.AddToHomescreenCoordinator in Java.
class AddToHomescreenCoordinator {
 public:
  // Called for showing the add-to-homescreen UI for AppBannerManager.
  static bool ShowForAppBanner(
      base::WeakPtr<AppBannerManager> weak_manager,
      std::unique_ptr<AddToHomescreenParams> params,
      base::RepeatingCallback<void(AddToHomescreenInstaller::Event,
                                   const AddToHomescreenParams&)>
          event_callback);

  AddToHomescreenCoordinator() = delete;
  AddToHomescreenCoordinator(const AddToHomescreenCoordinator&) = delete;
  AddToHomescreenCoordinator& operator=(const AddToHomescreenCoordinator&) =
      delete;
};

}  // namespace webapps

#endif  // COMPONENTS_WEBAPPS_BROWSER_ANDROID_ADD_TO_HOMESCREEN_COORDINATOR_H_
