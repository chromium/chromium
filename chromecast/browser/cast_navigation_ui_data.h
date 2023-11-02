// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_CAST_NAVIGATION_UI_DATA_H_
#define CHROMECAST_BROWSER_CAST_NAVIGATION_UI_DATA_H_

#include <string>

#include "content/public/browser/navigation_ui_data.h"

namespace content {
class WebContents;
}

namespace chromecast {
namespace shell {

class CastNavigationUIData : public content::NavigationUIData {
 public:
  static void SetAppPropertiesForWebContents(content::WebContents* web_contents,
                                             const std::string& session_id,
                                             bool is_audio_app);
  static std::string GetSessionIdForWebContents(
      content::WebContents* web_contents);

  explicit CastNavigationUIData(const std::string& session_id);

  CastNavigationUIData(const CastNavigationUIData&) = delete;
  CastNavigationUIData& operator=(const CastNavigationUIData&) = delete;

  const std::string& session_id() const { return session_id_; }

  std::unique_ptr<content::NavigationUIData> Clone() override;

 private:
  std::string session_id_;
};

}  // namespace shell
}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_CAST_NAVIGATION_UI_DATA_H_
