// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TAB_SHARING_TAB_SHARING_UI_H_
#define CHROME_BROWSER_UI_TAB_SHARING_TAB_SHARING_UI_H_

#include "base/strings/string16.h"
#include "chrome/browser/media/webrtc/media_stream_capture_indicator.h"

namespace infobars {
class InfoBar;
}

class TabSharingUI : public MediaStreamUI {
 public:
  TabSharingUI() = default;
  ~TabSharingUI() override = default;

  static std::unique_ptr<TabSharingUI> Create(
      const content::DesktopMediaID& media_id,
      base::string16 app_name);

  virtual void StartSharing(infobars::InfoBar* infobar) = 0;
  virtual void StopSharing() = 0;
};

#endif  // CHROME_BROWSER_UI_TAB_SHARING_TAB_SHARING_UI_H_
