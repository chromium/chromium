// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/cast_permission_user_data.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"

namespace {
const char kCastPermissionUserDataKey[] =
    "chromecast.shell.CastPermissionUserDataKey";
}  // namespace

namespace chromecast {
namespace shell {

CastPermissionUserData::CastPermissionUserData(
    content::WebContents* web_contents,
    const std::string& app_id,
    const GURL& app_web_url,
    bool enforce_feature_permissions,
    std::vector<int32_t> feature_permissions,
    std::vector<std::string> additional_feature_permission_origins)
    : app_id_(app_id),
      app_web_url_(app_web_url),
      enforce_feature_permissions_(enforce_feature_permissions),
      feature_permissions_(std::move(feature_permissions)),
      additional_feature_permission_origins_(
          std::move(additional_feature_permission_origins)) {
  feature_permissions_.insert(
      static_cast<int32_t>(blink::PermissionType::PROTECTED_MEDIA_IDENTIFIER));
  web_contents->SetUserData(&kCastPermissionUserDataKey,
                            base::WrapUnique(this));
}

CastPermissionUserData::~CastPermissionUserData() {}

// static
CastPermissionUserData* CastPermissionUserData::FromWebContents(
    content::WebContents* web_contents) {
  return static_cast<CastPermissionUserData*>(
      web_contents->GetUserData(&kCastPermissionUserDataKey));
}

}  // namespace shell
}  // namespace chromecast
