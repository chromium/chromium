// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_PERMISSION_REQUEST_H_
#define COMPONENTS_PERMISSIONS_PERMISSION_REQUEST_H_

#include <string>

#include "base/macros.h"
#include "build/build_config.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/permissions/permission_request_enums.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace permissions {
enum class RequestType;

// Describes the interface a feature making permission requests should
// implement. A class of this type is registered with the permission request
// manager to receive updates about the result of the permissions request
// from the bubble or infobar. It should live until it is unregistered or until
// RequestFinished is called.
// Note that no particular guarantees are made about what exact UI surface
// is presented to the user. The delegate may be coalesced with other bubble
// requests, or depending on the situation, not shown at all.
class PermissionRequest {
 public:
  PermissionRequest();
  virtual ~PermissionRequest() {}

  // The type of this request.
  virtual RequestType GetRequestType() const = 0;

  // Whether |this| and |other_request| are duplicates and therefore don't both
  // need to be shown in the UI.
  virtual bool IsDuplicateOf(PermissionRequest* other_request) const;

#if defined(OS_ANDROID)
  // Returns the full prompt text for this permission. This is currently only
  // used on Android.
  virtual std::u16string GetMessageText() const = 0;
#endif

#if !defined(OS_ANDROID)
  // Returns the short text for the chip button related to this permission.
  virtual absl::optional<std::u16string> GetChipText() const;

  // Returns the shortened prompt text for this permission. The permission
  // bubble may coalesce different requests, and if it does, this text will
  // be displayed next to an image and indicate the user grants the permission.
  virtual std::u16string GetMessageTextFragment() const = 0;
#endif

  // Get the origin on whose behalf this permission request is being made.
  virtual GURL GetOrigin() const = 0;

  // Called when the user has granted the requested permission.
  // If is_one_time is true the permission will last until all tabs of a given
  // |origin| are closed or navigated away from. The permission will
  // automatically expire after 1 day.
  virtual void PermissionGranted(bool is_one_time) = 0;

  // Called when the user has denied the requested permission.
  virtual void PermissionDenied() = 0;

  // Called when the user has cancelled the permission request. This
  // corresponds to a denial, but is segregated in case the context needs to
  // be able to distinguish between an active refusal or an implicit refusal.
  virtual void Cancelled() = 0;

  // The UI this request was associated with was answered by the user.
  // It is safe for the request to be deleted at this point -- it will receive
  // no further message from the permission request system. This method will
  // eventually be called on every request which is not unregistered.
  // It is ok to call this method without actually resolving the request via
  // PermissionGranted(), PermissionDenied() or Canceled(). However, it will not
  // resolve the javascript promise from the requesting origin.
  virtual void RequestFinished() = 0;

  // Used to record UMA for whether requests are associated with a user gesture.
  // To keep things simple this metric is only recorded for the most popular
  // request types.
  virtual PermissionRequestGestureType GetGestureType() const;

  // Used on Android to determine what Android OS permissions are needed for
  // this permission request.
  virtual ContentSettingsType GetContentSettingsType() const;

 private:
  DISALLOW_COPY_AND_ASSIGN(PermissionRequest);
};

}  // namespace permissions

#endif  // COMPONENTS_PERMISSIONS_PERMISSION_REQUEST_H_
