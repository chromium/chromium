// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CDM_CAST_CDM_FACTORY_H_
#define CHROMECAST_MEDIA_CDM_CAST_CDM_FACTORY_H_

#include "chromecast/media/base/key_systems_common.h"
#include "chromecast/media/common/media_resource_tracker.h"
#include "media/base/cdm_factory.h"
#include "url/origin.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace media {
struct CdmConfig;
}  // namespace media

namespace chromecast {
namespace media {

class CastCdm;

class CastCdmFactory : public ::media::CdmFactory {
 public:
  // CDM factory will use |task_runner| to initialize the CDM.
  CastCdmFactory(scoped_refptr<base::SingleThreadTaskRunner> task_runner,
                 const url::Origin& cdm_origin,
                 MediaResourceTracker* media_resource_tracker);

  CastCdmFactory(const CastCdmFactory&) = delete;
  CastCdmFactory& operator=(const CastCdmFactory&) = delete;

  ~CastCdmFactory() override;

  // ::media::CdmFactory implementation:
  void Create(
      const ::media::CdmConfig& cdm_config,
      const ::media::SessionMessageCB& session_message_cb,
      const ::media::SessionClosedCB& session_closed_cb,
      const ::media::SessionKeysChangeCB& session_keys_change_cb,
      const ::media::SessionExpirationUpdateCB& session_expiration_update_cb,
      ::media::CdmCreatedCB cdm_created_cb) override;

  // Provides a platform-specific BrowserCdm instance.
  virtual scoped_refptr<CastCdm> CreatePlatformBrowserCdm(
      const CastKeySystem& cast_key_system,
      const url::Origin& cdm_origin,
      const ::media::CdmConfig& cdm_config);

 protected:
  MediaResourceTracker* media_resource_tracker_;

 private:
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  const url::Origin cdm_origin_;
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CDM_CAST_CDM_FACTORY_H_
