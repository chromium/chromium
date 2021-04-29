// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_MEDIA_APP_UI_MEDIA_APP_GUEST_UI_H_
#define CHROMEOS_COMPONENTS_MEDIA_APP_UI_MEDIA_APP_GUEST_UI_H_

#include "ui/webui/untrusted_web_ui_controller.h"

namespace content {
class WebUIDataSource;
}

namespace chromeos {

// A delegate used during data source creation to expose some //chrome
// functionality to the data source
class MediaAppGuestUIDelegate {
 public:
  // Takes a WebUIDataSource, and populates its load-time data.
  virtual void PopulateLoadTimeData(content::WebUIDataSource* source) = 0;
};

// The webui for chrome-untrusted://media-app.
class MediaAppGuestUI : public ui::UntrustedWebUIController {
 public:
  MediaAppGuestUI(content::WebUI* web_ui, MediaAppGuestUIDelegate* delegate);
  MediaAppGuestUI(const MediaAppGuestUI&) = delete;
  MediaAppGuestUI& operator=(const MediaAppGuestUI&) = delete;
  ~MediaAppGuestUI() override;
};

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_MEDIA_APP_UI_MEDIA_APP_GUEST_UI_H_
