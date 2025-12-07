// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/ui_manager/update_dialog_types.h"

#include <optional>
#include <string>

#include "ui/gfx/image/image.h"
#include "url/gurl.h"

namespace web_app {

WebAppIdentity::WebAppIdentity() = default;
WebAppIdentity::WebAppIdentity(const std::u16string& title,
                               const gfx::Image& icon,
                               const GURL& start_url)
    : title(title), icon(icon), start_url(start_url) {}
WebAppIdentity::~WebAppIdentity() = default;
WebAppIdentity::WebAppIdentity(const WebAppIdentity&) = default;
WebAppIdentity& WebAppIdentity::operator=(const WebAppIdentity&) = default;

WebAppIdentityUpdate::WebAppIdentityUpdate() = default;
WebAppIdentityUpdate::~WebAppIdentityUpdate() = default;
WebAppIdentityUpdate::WebAppIdentityUpdate(const WebAppIdentityUpdate&) =
    default;
WebAppIdentityUpdate& WebAppIdentityUpdate::operator=(
    const WebAppIdentityUpdate&) = default;

WebAppIdentity WebAppIdentityUpdate::MakeOldIdentity() const {
  return WebAppIdentity(old_title, old_icon, old_start_url);
}
WebAppIdentity WebAppIdentityUpdate::MakeNewIdentity() const {
  return WebAppIdentity(new_title.value_or(old_title),
                        new_icon.value_or(old_icon),
                        new_start_url.value_or(old_start_url));
}

}  // namespace web_app
