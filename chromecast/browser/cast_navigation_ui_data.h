// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_CAST_NAVIGATION_UI_DATA_H_
#define CHROMECAST_BROWSER_CAST_NAVIGATION_UI_DATA_H_

#include <string>

#include "base/macros.h"
#include "content/public/browser/navigation_ui_data.h"

namespace content {
class WebContents;
}

namespace chromecast {
namespace shell {

class CastNavigationUIData : public content::NavigationUIData {
 public:
  static void SetSessionIdForWebContents(content::WebContents* web_contents,
                                         const std::string& session_id);
  static std::string GetSessionIdForWebContents(
      content::WebContents* web_contents);

  explicit CastNavigationUIData(const std::string& session_id);

  const std::string& session_id() const { return session_id_; }

  std::unique_ptr<content::NavigationUIData> Clone() override;

 private:
  std::string session_id_;

  DISALLOW_COPY_AND_ASSIGN(CastNavigationUIData);
};

}  // namespace shell
}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_CAST_NAVIGATION_UI_DATA_H_
