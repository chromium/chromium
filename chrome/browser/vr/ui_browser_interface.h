// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_UI_BROWSER_INTERFACE_H_
#define CHROME_BROWSER_VR_UI_BROWSER_INTERFACE_H_

#include "chrome/browser/vr/vr_base_export.h"
#include "ui/gfx/geometry/size_f.h"
#include "url/gurl.h"

namespace vr {

// An interface for the VR UI to communicate with VrShell. Many of the functions
// in this interface are proxies to methods on VrShell.
class VR_BASE_EXPORT UiBrowserInterface {
 public:
  virtual ~UiBrowserInterface() = default;

  virtual void ExitPresent() = 0;
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_UI_BROWSER_INTERFACE_H_
