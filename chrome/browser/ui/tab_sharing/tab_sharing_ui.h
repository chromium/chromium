// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TAB_SHARING_TAB_SHARING_UI_H_
#define CHROME_BROWSER_UI_TAB_SHARING_TAB_SHARING_UI_H_

#include <string>

#include "chrome/browser/media/webrtc/media_stream_capture_indicator.h"
#include "chrome/browser/ui/tab_sharing/tab_sharing_infobar_delegate.h"
#include "content/public/browser/global_routing_id.h"

namespace infobars {
class InfoBar;
}

class TabSharingUI : public MediaStreamUI {
 public:
  TabSharingUI() = default;
  ~TabSharingUI() override = default;

  static std::unique_ptr<TabSharingUI> Create(
      content::GlobalRenderFrameHostId capturer,
      const content::DesktopMediaID& media_id,
      const std::u16string& capturer_name,
      bool favicons_used_for_switch_to_tab_button,
      bool app_preferred_current_tab,
      TabSharingInfoBarDelegate::TabShareType capture_type,
      bool captured_surface_control_active);

  virtual void StartSharing(infobars::InfoBar* infobar) = 0;
  virtual void StopSharing() = 0;
};

#endif  // CHROME_BROWSER_UI_TAB_SHARING_TAB_SHARING_UI_H_
