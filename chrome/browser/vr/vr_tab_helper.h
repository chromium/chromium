// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_VR_TAB_HELPER_H_
#define CHROME_BROWSER_VR_VR_TAB_HELPER_H_

#include "base/memory/raw_ref.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "components/tabs/public/tab_interface.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

namespace content {
class WebContents;
}

namespace vr {

class VrTabHelper {
 public:
  DECLARE_USER_DATA(VrTabHelper);

  explicit VrTabHelper(tabs::TabInterface& tab);
  VrTabHelper(const VrTabHelper&) = delete;
  VrTabHelper& operator=(const VrTabHelper&) = delete;

  ~VrTabHelper();

  static VrTabHelper* From(tabs::TabInterface* tab);
  static VrTabHelper* FromWebContents(content::WebContents* contents);

  class Observer : public base::CheckedObserver {
   public:
    virtual void OnIsContentDisplayedInHeadsetChanged(bool state) = 0;
  };

  bool is_in_vr() const { return is_in_vr_; }

  // Called by VrShell when we enter and exit vr mode. It finds us by looking us
  // up on the WebContents.
  void SetIsInVr(bool is_in_vr);

  bool is_content_displayed_in_headset() const {
    return is_content_displayed_in_headset_;
  }

  void SetIsContentDisplayedInHeadset(bool state);

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  static bool IsInVr(content::WebContents* contents);
  static bool IsContentDisplayedInHeadset(content::WebContents* contents);
  static void SetIsContentDisplayedInHeadset(content::WebContents* contents,
                                             bool state);

  static void ExitVrPresentation();

 private:
  const raw_ref<tabs::TabInterface> tab_;

  // If is_in_vr_ is true, that means that the only content displayed is
  // inside vr (for example, VR browsing or immersive experience
  // on an Android phone with headset on).
  // If is_content_displayed_in_headset_ is true, it means content is displayed
  // both in VR and on the desktop (example: presenting WebXR
  // content from a browser on Windows).
  //
  // If we have external monitors on Android, we'd want
  // is_content_displayed_in_headset_ to be true there, and likewise
  // is_in_vr_ could be true if content was only available in-headset on
  // Windows.
  // TODO(cassew): Rename below vars to more intuitive names.
  bool is_in_vr_ = false;
  bool is_content_displayed_in_headset_ = false;

  base::ObserverList<Observer> observers_;

  ui::ScopedUnownedUserData<VrTabHelper> scoped_unowned_user_data_;
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_VR_TAB_HELPER_H_
