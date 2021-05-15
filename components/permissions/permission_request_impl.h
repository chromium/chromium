// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_PERMISSION_REQUEST_IMPL_H_
#define COMPONENTS_PERMISSIONS_PERMISSION_REQUEST_IMPL_H_

#include "base/callback.h"
#include "base/macros.h"
#include "build/build_config.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/permissions/permission_request.h"
#include "components/permissions/permission_request_id.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class GURL;

namespace permissions {
enum class RequestType;

// Default implementation of PermissionRequest, it is assumed that
// the caller owns it and that it can be deleted once the |delete_callback| is
// executed.
class PermissionRequestImpl : public PermissionRequest {
 public:
  using PermissionDecidedCallback =
      base::OnceCallback<void(ContentSetting, bool)>;

  PermissionRequestImpl(const GURL& request_origin,
                        ContentSettingsType content_settings_type,
                        bool has_gesture,
                        PermissionDecidedCallback permission_decided_callback,
                        base::OnceClosure delete_callback);

  ~PermissionRequestImpl() override;

// PermissionRequest:
#if !defined(OS_ANDROID)
  // Implementors can override this method to customize the message text.
  std::u16string GetMessageTextFragment() const override;
#endif

 private:
  RequestType GetRequestType() const override;
#if defined(OS_ANDROID)
  std::u16string GetMessageText() const override;
#else
  absl::optional<std::u16string> GetChipText() const override;
#endif
  GURL GetOrigin() const override;
  void PermissionGranted(bool is_one_time) override;
  void PermissionDenied() override;
  void Cancelled() override;
  void RequestFinished() override;
  PermissionRequestGestureType GetGestureType() const override;
  ContentSettingsType GetContentSettingsType() const override;

  GURL request_origin_;
  ContentSettingsType content_settings_type_;
  bool has_gesture_;

  // Called once a decision is made about the permission.
  PermissionDecidedCallback permission_decided_callback_;

  // Called when the request is no longer in use so it can be deleted by the
  // caller.
  base::OnceClosure delete_callback_;

  DISALLOW_COPY_AND_ASSIGN(PermissionRequestImpl);
};

}  // namespace permissions

#endif  // COMPONENTS_PERMISSIONS_PERMISSION_REQUEST_IMPL_H_
