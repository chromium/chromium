// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_COMMON_CAST_CONTENT_CLIENT_H_
#define CHROMECAST_COMMON_CAST_CONTENT_CLIENT_H_

#include <string_view>
#include <vector>

#include "base/task/sequenced_task_runner.h"
#include "build/build_config.h"
#include "content/public/common/content_client.h"
#include "url/gurl.h"

namespace chromecast {
namespace shell {

class CastContentClient : public content::ContentClient {
 public:
  ~CastContentClient() override;

  // content::ContentClient implementation:
  void SetActiveURL(const GURL& url, std::string top_origin) override;
  void AddAdditionalSchemes(Schemes* schemes) override;
  std::u16string GetLocalizedString(int message_id) override;
  std::string_view GetDataResource(
      int resource_id,
      ui::ResourceScaleFactor scale_factor) override;
  base::RefCountedMemory* GetDataResourceBytes(int resource_id) override;
  std::string GetDataResourceString(int resource_id) override;
  gfx::Image& GetNativeImageNamed(int resource_id) override;
#if BUILDFLAG(IS_ANDROID)
  ::media::MediaDrmBridgeClient* GetMediaDrmBridgeClient() override;
#endif  // BUILDFLAG(IS_ANDROID)
  void ExposeInterfacesToBrowser(
      scoped_refptr<base::SequencedTaskRunner> io_task_runner,
      mojo::BinderMap* binders) override;
  void AddContentDecryptionModules(
      std::vector<content::CdmInfo>* cdms,
      std::vector<::media::CdmHostFilePath>* cdm_host_file_paths) override;

 private:
  GURL last_active_url_;
};

}  // namespace shell
}  // namespace chromecast

#endif  // CHROMECAST_COMMON_CAST_CONTENT_CLIENT_H_
