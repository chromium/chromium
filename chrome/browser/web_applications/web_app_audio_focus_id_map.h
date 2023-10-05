// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_AUDIO_FOCUS_ID_MAP_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_AUDIO_FOCUS_ID_MAP_H_

#include <map>

#include "components/webapps/common/web_app_id.h"

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
  WebAppAudioFocusIdMap(const WebAppAudioFocusIdMap&) = delete;
  WebAppAudioFocusIdMap& operator=(const WebAppAudioFocusIdMap&) = delete;
  ~WebAppAudioFocusIdMap();

 protected:
  friend class WebAppTabHelper;

  const base::UnguessableToken& CreateOrGetIdForApp(
      const webapps::AppId& app_id);

 private:
  std::map<webapps::AppId, base::UnguessableToken> ids_;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_AUDIO_FOCUS_ID_MAP_H_
