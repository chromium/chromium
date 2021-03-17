// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_LOCATION_BAR_HELPER_H_
#define CHROME_BROWSER_VR_LOCATION_BAR_HELPER_H_

#include "chrome/browser/vr/browser_ui_interface.h"
#include "chrome/browser/vr/model/location_bar_state.h"
#include "chrome/browser/vr/vr_export.h"

class LocationBarModel;
class LocationBarModelDelegate;

namespace vr {

class BrowserUiInterface;

// This class houses an instance of LocationBarModel, and queries it when
// requested, passing a snapshot of the toolbar state to the UI when necessary.
class VR_EXPORT LocationBarHelper {
 public:
  LocationBarHelper(BrowserUiInterface* ui, LocationBarModelDelegate* delegate);
  virtual ~LocationBarHelper();

  // Poll LocationBarModel and post an update to the UI if state has changed.
  void Update();

 private:
  BrowserUiInterface* ui_;
  std::unique_ptr<LocationBarModel> location_bar_model_;
  LocationBarState current_state_;
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_LOCATION_BAR_HELPER_H_
