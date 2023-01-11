// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAPPS_BROWSER_ANDROID_ADD_TO_HOMESCREEN_INSTALLER_H_
#define COMPONENTS_WEBAPPS_BROWSER_ANDROID_ADD_TO_HOMESCREEN_INSTALLER_H_

#include "base/functional/callback_forward.h"
#include "components/webapps/browser/android/add_to_homescreen_params.h"

namespace content {
class WebContents;
}

namespace webapps {

// Helper class for installing a web app or an Android native app and recording
// related UMA.
class AddToHomescreenInstaller {
 public:
  enum class Event {
    INSTALL_STARTED,
    INSTALL_FAILED,
    INSTALL_REQUEST_FINISHED,
    NATIVE_INSTALL_OR_OPEN_FAILED,
    NATIVE_INSTALL_OR_OPEN_SUCCEEDED,
    NATIVE_DETAILS_SHOWN,
    UI_SHOWN,
    UI_CANCELLED,
  };

  // Installs the app referenced by the data in this object.
  // |event_callback| will be run to inform the caller of the progress of the
  // installation. It is guaranteed that |event_callback| will only be called
  // before the add-to-homescreen prompt is dismissed.
  static void Install(
      content::WebContents* web_contents,
      const AddToHomescreenParams& params,
      const base::RepeatingCallback<void(Event, const AddToHomescreenParams&)>&
          event_callback);

 private:
  static void InstallOrOpenNativeApp(
      content::WebContents* web_contents,
      const AddToHomescreenParams& params,
      const base::RepeatingCallback<void(Event, const AddToHomescreenParams&)>&
          event_callback);

  AddToHomescreenInstaller() = delete;
  AddToHomescreenInstaller(const AddToHomescreenInstaller&) = delete;
  AddToHomescreenInstaller& operator=(const AddToHomescreenInstaller&) = delete;
};

}  // namespace webapps

#endif  // COMPONENTS_WEBAPPS_BROWSER_ANDROID_ADD_TO_HOMESCREEN_INSTALLER_H_
