// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_WEB_APP_AUDIO_FOCUS_ID_MAP_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_WEB_APP_AUDIO_FOCUS_ID_MAP_H_

#include <map>

#include "base/macros.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"

namespace base {
class UnguessableToken;
}

namespace web_app {

// WebAppAudioFocusIdMap stores audio focus group ids for web apps. These group
// ids are shared across all media sessions associated with the web app and are
// used so that the app has separate audio focus from the browser.
class WebAppAudioFocusIdMap {
 public:
  WebAppAudioFocusIdMap();
  ~WebAppAudioFocusIdMap();

 protected:
  friend class WebAppTabHelper;

  const base::UnguessableToken& CreateOrGetIdForApp(const AppId& app_id);

 private:
  std::map<AppId, base::UnguessableToken> ids_;

  DISALLOW_COPY_AND_ASSIGN(WebAppAudioFocusIdMap);
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_WEB_APP_AUDIO_FOCUS_ID_MAP_H_
