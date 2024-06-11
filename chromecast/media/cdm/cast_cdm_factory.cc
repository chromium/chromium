// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cdm/cast_cdm_factory.h"

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/task/bind_post_task.h"
#include "base/task/single_thread_task_runner.h"
#include "chromecast/base/metrics/cast_metrics_helper.h"
#include "chromecast/media/cdm/cast_cdm.h"
#include "media/base/cdm_config.h"
#include "media/base/cdm_key_information.h"
#include "url/origin.h"

namespace chromecast {
namespace media {

CastCdmFactory::CastCdmFactory(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    const url::Origin& cdm_origin,
    MediaResourceTracker* media_resource_tracker)
    : media_resource_tracker_(media_resource_tracker),
      task_runner_(task_runner),
      cdm_origin_(cdm_origin) {
  DCHECK(media_resource_tracker_);
  DCHECK(task_runner_);
}

CastCdmFactory::~CastCdmFactory() {}

void CastCdmFactory::Create(
    const ::media::CdmConfig& cdm_config,
    const ::media::SessionMessageCB& session_message_cb,
    const ::media::SessionClosedCB& session_closed_cb,
    const ::media::SessionKeysChangeCB& session_keys_change_cb,
    const ::media::SessionExpirationUpdateCB& session_expiration_update_cb,
    ::media::CdmCreatedCB cdm_created_cb) {
  // Bound |cdm_created_cb| so we always fire it asynchronously.
  ::media::CdmCreatedCB bound_cdm_created_cb =
      base::BindPostTaskToCurrentDefault(std::move(cdm_created_cb));

  CastKeySystem cast_key_system(GetKeySystemByName(cdm_config.key_system));

  DCHECK((cast_key_system == chromecast::media::KEY_SYSTEM_PLAYREADY) ||
         (cast_key_system == chromecast::media::KEY_SYSTEM_WIDEVINE));

  scoped_refptr<chromecast::media::CastCdm> cast_cdm =
      CreatePlatformBrowserCdm(cast_key_system, cdm_origin_, cdm_config);

  if (!cast_cdm) {
    LOG(INFO) << "No matching key system found: " << cast_key_system;
    std::move(bound_cdm_created_cb)
        .Run(nullptr, ::media::CreateCdmStatus::kUnsupportedKeySystem);
    return;
  }

  const int packed_cdm_config = (cdm_config.allow_distinctive_identifier << 2) |
                                (cdm_config.allow_persistent_state << 1) |
                                cdm_config.use_hw_secure_codecs;
  metrics::CastMetricsHelper::GetInstance()->RecordApplicationEventWithValue(
      "Cast.Platform.CreateCdm." + cdm_config.key_system, packed_cdm_config);

  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &CastCdm::Initialize, base::Unretained(cast_cdm.get()),
          base::BindPostTaskToCurrentDefault(session_message_cb),
          base::BindPostTaskToCurrentDefault(session_closed_cb),
          base::BindPostTaskToCurrentDefault(session_keys_change_cb),
          base::BindPostTaskToCurrentDefault(session_expiration_update_cb)));
  std::move(bound_cdm_created_cb)
      .Run(cast_cdm, ::media::CreateCdmStatus::kSuccess);
}

scoped_refptr<CastCdm> CastCdmFactory::CreatePlatformBrowserCdm(
    const CastKeySystem& cast_key_system,
    const url::Origin& cdm_origin,
    const ::media::CdmConfig& cdm_config) {
  return nullptr;
}

}  // namespace media
}  // namespace chromecast
