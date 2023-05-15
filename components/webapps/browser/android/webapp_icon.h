// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAPPS_BROWSER_ANDROID_WEBAPP_ICON_H_
#define COMPONENTS_WEBAPPS_BROWSER_ANDROID_WEBAPP_ICON_H_

#include <string>
#include "components/webapk/webapk.pb.h"
#include "url/gurl.h"

namespace webapps {

// Information related to WebAPK icon
class WebappIcon {
 public:
  explicit WebappIcon(const GURL& icon_url);
  explicit WebappIcon(const GURL& icon_url,
                      bool is_maskable,
                      webapk::Image::Usage usage);

  WebappIcon(const WebappIcon&);
  WebappIcon& operator=(const WebappIcon&);
  ~WebappIcon();

  const GURL url() const { return url_; }

  void AddUsage(webapk::Image::Usage);

  int GetIdealSizeInPx() const;

 private:
  GURL url_;
  webapk::Image::Purpose purpose_;
  std::set<webapk::Image::Usage> usages_;
};

}  // namespace webapps

#endif  // COMPONENTS_WEBAPPS_BROWSER_ANDROID_WEBAPP_ICON_H_
