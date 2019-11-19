// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_COMMON_CAST_CONTENT_CLIENT_H_
#define CHROMECAST_COMMON_CAST_CONTENT_CLIENT_H_

#include "content/public/common/content_client.h"
#include "url/gurl.h"

namespace chromecast {
namespace shell {

// TODO(halliwell) Move this function to its own header.
std::string GetUserAgent();

class CastContentClient : public content::ContentClient {
 public:
  ~CastContentClient() override;

  // content::ContentClient implementation:
  void SetActiveURL(const GURL& url, std::string top_origin) override;
  void AddAdditionalSchemes(Schemes* schemes) override;
  base::string16 GetLocalizedString(int message_id) override;
  base::StringPiece GetDataResource(int resource_id,
                                    ui::ScaleFactor scale_factor) override;
  base::RefCountedMemory* GetDataResourceBytes(int resource_id) override;
  gfx::Image& GetNativeImageNamed(int resource_id) override;
#if defined(OS_ANDROID)
  ::media::MediaDrmBridgeClient* GetMediaDrmBridgeClient() override;
#endif  // OS_ANDROID
  void ExposeInterfacesToBrowser(
      scoped_refptr<base::SequencedTaskRunner> io_task_runner,
      mojo::BinderMap* binders) override;

 private:
  GURL last_active_url_;
};

}  // namespace shell
}  // namespace chromecast

#endif  // CHROMECAST_COMMON_CAST_CONTENT_CLIENT_H_
