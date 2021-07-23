// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/cast_permission_user_data.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "content/public/browser/web_contents.h"

namespace {
const char kCastPermissionUserDataKey[] =
    "chromecast.shell.CastPermissionUserDataKey";
}  // namespace

namespace chromecast {
namespace shell {

CastPermissionUserData::CastPermissionUserData(
    content::WebContents* web_contents,
    const std::string& app_id,
    const GURL& app_web_url)
    : app_id_(app_id), app_web_url_(app_web_url) {
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
