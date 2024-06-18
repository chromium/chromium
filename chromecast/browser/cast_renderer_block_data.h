// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_CAST_RENDERER_BLOCK_DATA_H_
#define CHROMECAST_BROWSER_CAST_RENDERER_BLOCK_DATA_H_

#include "base/memory/weak_ptr.h"
#include "base/supports_user_data.h"

namespace content {
class WebContents;
}

namespace chromecast {
namespace media {
class ApplicationMediaInfoManager;
}

namespace shell {

class CastRendererBlockData : public base::SupportsUserData::Data {
 public:
  static void SetRendererBlockForWebContents(content::WebContents* web_contents,
                                             bool blocked);
  static void SetApplicationMediaInfoManagerForWebContents(
      content::WebContents* web_contents,
      base::WeakPtr<media::ApplicationMediaInfoManager>
          application_media_info_manager);
  CastRendererBlockData();
  ~CastRendererBlockData() override;

  bool blocked() const { return blocked_; }
  void SetBlocked(bool blocked);
  void SetApplicationMediaInfoManager(
      base::WeakPtr<media::ApplicationMediaInfoManager>
          application_media_info_manager);

 private:
  bool blocked_;
  base::WeakPtr<media::ApplicationMediaInfoManager>
      application_media_info_manager_;
};

}  // namespace shell
}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_CAST_RENDERER_BLOCK_DATA_H_
