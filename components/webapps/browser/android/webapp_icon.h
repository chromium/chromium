// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAPPS_BROWSER_ANDROID_WEBAPP_ICON_H_
#define COMPONENTS_WEBAPPS_BROWSER_ANDROID_WEBAPP_ICON_H_

#include <set>
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

  WebappIcon(const WebappIcon&) = delete;
  WebappIcon& operator=(const WebappIcon&) = delete;
  ~WebappIcon();

  int GetIdealSizeInPx() const;

  const GURL url() const { return url_; }

  void AddUsage(webapk::Image::Usage);
  const std::set<webapk::Image::Usage>& usages() const { return usages_; }

  webapk::Image::Purpose purpose() const { return purpose_; }

  const std::string unsafe_data() const { return unsafe_data_; }
  bool has_unsafe_data() const { return has_unsafe_data_; }
  void SetData(std::string&& data);
  std::string&& ExtractData();

  const std::string& hash() const { return hash_; }
  void set_hash(const std::string& hash) { hash_ = hash; }

 private:
  GURL url_;
  webapk::Image::Purpose purpose_ = webapk::Image::ANY;
  std::set<webapk::Image::Usage> usages_;

  // The result of fetching the |icon|. This is untrusted data from the web
  // and should not be processed or decoded by the browser process.
  std::string unsafe_data_;
  bool has_unsafe_data_ = false;

  // The murmur2 hash of |unsafe_data|.
  std::string hash_;
};

}  // namespace webapps

#endif  // COMPONENTS_WEBAPPS_BROWSER_ANDROID_WEBAPP_ICON_H_
