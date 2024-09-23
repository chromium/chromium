// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/cast_renderer_block_data.h"

#include "base/logging.h"
#include "chromecast/browser/application_media_info_manager.h"
#include "chromecast/browser/cast_session_id_map.h"
#include "content/public/browser/web_contents.h"

namespace chromecast {
namespace shell {
namespace {

const char kUserDataKey[] = "chromecast.shell.RenderBlockUserData.key";

CastRendererBlockData* GetOrCreateCastRendererBlockData(
    content::WebContents* web_contents) {
  CastRendererBlockData* data = static_cast<CastRendererBlockData*>(
      web_contents->GetUserData(kUserDataKey));
  if (!data) {
    auto cast_renderer_block_data = std::make_unique<CastRendererBlockData>();
    data = cast_renderer_block_data.get();
    web_contents->SetUserData(kUserDataKey,
                              std::move(cast_renderer_block_data));
  }
  return data;
}

}  // namespace

// static
void CastRendererBlockData::SetRendererBlockForWebContents(
    content::WebContents* web_contents,
    bool blocked) {
  DCHECK(web_contents);
  CastRendererBlockData* data = GetOrCreateCastRendererBlockData(web_contents);
  data->SetBlocked(blocked);
}

// static
void CastRendererBlockData::SetApplicationMediaInfoManagerForWebContents(
    content::WebContents* web_contents,
    base::WeakPtr<media::ApplicationMediaInfoManager>
        application_media_info_manager) {
  DCHECK(web_contents);
  CastRendererBlockData* data = GetOrCreateCastRendererBlockData(web_contents);
  data->SetApplicationMediaInfoManager(application_media_info_manager);
}

CastRendererBlockData::CastRendererBlockData() : blocked_(false) {}
CastRendererBlockData::~CastRendererBlockData() = default;

void CastRendererBlockData::SetBlocked(bool blocked) {
  LOG(INFO) << "Setting blocked to: " << blocked << " from " << blocked_;
  blocked_ = blocked;
  if (application_media_info_manager_) {
    application_media_info_manager_->SetRendererBlock(blocked);
  }
}

void CastRendererBlockData::SetApplicationMediaInfoManager(
    base::WeakPtr<media::ApplicationMediaInfoManager>
        application_media_info_manager) {
  DCHECK(application_media_info_manager);
  application_media_info_manager_ = application_media_info_manager;
  application_media_info_manager_->SetRendererBlock(blocked_);
}

}  // namespace shell
}  // namespace chromecast
