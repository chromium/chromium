// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast_receiver/browser/permissions_manager_impl.h"

#include "base/containers/contains.h"
#include "content/public/browser/web_contents.h"

namespace cast_receiver {
namespace {

// Key in the WebContents's UserData where the singleton instance for this
// WebContents will be stored.
// TODO(crbug.com/1382897): Combine with other cast_receiver UserData.
const char kPermissionManagerUserDataKey[] =
    "components/cast_receiver/browser/permissions_manager_impl";

}  // namespace

// static
PermissionsManager* PermissionsManager::GetInstance(
    content::WebContents& web_contents) {
  PermissionsManager* instance = static_cast<PermissionsManagerImpl*>(
      web_contents.GetUserData(&kPermissionManagerUserDataKey));
  return instance;
}

// static
PermissionsManagerImpl* PermissionsManagerImpl::CreateInstance(
    content::WebContents& web_contents,
    std::string app_id) {
  DCHECK(GetInstance(web_contents) == nullptr);

  std::unique_ptr<PermissionsManagerImpl> permission_manager =
      std::make_unique<PermissionsManagerImpl>(std::move(app_id));
  auto* ptr = permission_manager.get();
  web_contents.SetUserData(&kPermissionManagerUserDataKey,
                           std::move(permission_manager));
  return ptr;
}

PermissionsManagerImpl::PermissionsManagerImpl(std::string app_id)
    : app_id_(std::move(app_id)) {}

PermissionsManagerImpl::~PermissionsManagerImpl() = default;

void PermissionsManagerImpl::AddPermission(blink::PermissionType permission) {
  // TODO(crbug.com/1383326): Ensure this each permission is valid per an allow
  // list maintained by this component.
  permissions_.push_back(permission);
}

void PermissionsManagerImpl::AddOrigin(url::Origin origin) {
  CHECK(!origin.opaque());
  additional_origins_.push_back(std::move(origin));
}

const std::string& PermissionsManagerImpl::GetAppId() const {
  return app_id_;
}

blink::mojom::PermissionStatus PermissionsManagerImpl::GetPermissionStatus(
    blink::PermissionType permission,
    const GURL& url) const {
  if (!base::Contains(permissions_, permission)) {
    return blink::mojom::PermissionStatus::DENIED;
  }

  const url::Origin url_origin = url::Origin::Create(url);
  if (app_url_ && url_origin.IsSameOriginWith(app_url_.value())) {
    return blink::mojom::PermissionStatus::GRANTED;
  }

  if (base::Contains(additional_origins_, url_origin)) {
    return blink::mojom::PermissionStatus::GRANTED;
  }

  return blink::mojom::PermissionStatus::DENIED;
}

}  // namespace cast_receiver
