// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_PERMISSION_REQUEST_H_
#define COMPONENTS_PERMISSIONS_PERMISSION_REQUEST_H_

#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/permissions/permission_request_data.h"
#include "components/permissions/permission_request_enums.h"
#include "components/permissions/request_type.h"
#include "content/public/browser/global_routing_id.h"
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
  // If `result` is CONTENT_SETTING_ALLOW, the permission was granted by the
  // user. If it's CONTENT_SETTING_BLOCK, the permission was blocked by the
  // user. If it's CONTENT_SETTING_DEFAULT, the permission was ignored or
  // dismissed without an explicit decision. No other ContentSetting value will
  // be passed into this callback.
  // If `is_one_time` is true, the decision will last until all tabs of
  // `requesting_origin_` are closed or navigated away from.
  using PermissionDecidedCallback =
      base::RepeatingCallback<void(ContentSetting /*result*/,
                                   bool /*is_one_time*/,
                                   bool /*is_final_decision*/)>;

  // `permission_decided_callback` is called when the permission request is
  // resolved by the user (see comment on PermissionDecidedCallback above).
  // `delete_callback` is called when the permission request is no longer needed
  // by the permission system. Therefore, it is safe to delete `this` inside
  // `delete_callback`. It will always be called eventually by the permission
  // system.
  // `delete_callback` may be called before `permission_decided_callback`, for
  // example if the tab is closed without user interaction. In this case, the
  // javascript promise from the requesting origin will not be resolved.
  PermissionRequest(const GURL& requesting_origin,
                    RequestType request_type,
                    bool has_gesture,
                    PermissionDecidedCallback permission_decided_callback,
                    base::OnceClosure delete_callback);

  PermissionRequest(PermissionRequestData request_data,
                    PermissionDecidedCallback permission_decided_callback,
                    base::OnceClosure delete_callback,
                    bool uses_automatic_embargo);

  PermissionRequest(const PermissionRequest&) = delete;
  PermissionRequest& operator=(const PermissionRequest&) = delete;

  enum ChipTextType {
    LOUD_REQUEST,
    QUIET_REQUEST,
    ALLOW_CONFIRMATION,
    ALLOW_ONCE_CONFIRMATION,
    BLOCKED_CONFIRMATION,
    ACCESSIBILITY_ALLOWED_CONFIRMATION,
    ACCESSIBILITY_ALLOWED_ONCE_CONFIRMATION,
    ACCESSIBILITY_BLOCKED_CONFIRMATION
  };

  virtual ~PermissionRequest();

  GURL requesting_origin() const { return data_.requesting_origin; }
  RequestType request_type() const;

  // Whether |this| and |other_request| are duplicates and therefore don't both
  // need to be shown in the UI.
  virtual bool IsDuplicateOf(PermissionRequest* other_request) const;

#if BUILDFLAG(IS_ANDROID)
  // A message text with formatting information.
  struct AnnotatedMessageText {
    // |text| specifies the text string itself.
    // |bolded_ranges| defines a (potentially empty) list of ranges represented
    // as pairs of <textOffset, rangeSize>, which shall be used by the UI to
    // format the specified ranges as bold text.
    AnnotatedMessageText(std::u16string text,
                         std::vector<std::pair<size_t, size_t>> bolded_ranges);
    ~AnnotatedMessageText();
    AnnotatedMessageText(const AnnotatedMessageText& other) = delete;
    AnnotatedMessageText& operator=(const AnnotatedMessageText& other) = delete;

    std::u16string text;

    // A list of ranges defined as pairs of <offset, size> which
    // will be used by Clank to format the ranges in |text| as bold.
    std::vector<std::pair<size_t, size_t>> bolded_ranges;
  };

  virtual AnnotatedMessageText GetDialogAnnotatedMessageText(
      const GURL& embedding_origin) const;

  // Returns prompt text appropriate for displaying in an Android dialog.
  static AnnotatedMessageText GetDialogAnnotatedMessageText(
      std::u16string requesting_origin_formatted_for_display,
      int message_id,
      bool format_origin_bold);
#endif

  // Returns a weak pointer to this instance.
  base::WeakPtr<PermissionRequest> GetWeakPtr();

#if !BUILDFLAG(IS_ANDROID)
  // Returns whether displaying a confirmation chip for the request is
  // supported.
  bool IsConfirmationChipSupported();

  // Returns prompt icon appropriate for displaying on the chip button in the
  // location bar.
  IconId GetIconForChip();

  // Returns prompt icon appropriate for displaying on the quiet chip button in
  // the location bar.
  IconId GetBlockedIconForChip();

  // Returns prompt text appropriate for displaying on the chip button in the
  // location bar.
  std::optional<std::u16string> GetRequestChipText(ChipTextType type) const;

  // Returns prompt text appropriate for displaying under the dialog title
  // "[domain] wants to:".
  virtual std::u16string GetMessageTextFragment() const;
#endif

  // Returns the text to be used in the "allow always" button of the
  // permission prompt.
  // If not provided, the generic text for this button will be used instead.
  // The default implementation returns std::nullopt (ie, use generic text).
  virtual std::optional<std::u16string> GetAllowAlwaysText() const;

  // Whether the request was initiated by the user clicking on the permission
  // element.
  bool IsEmbeddedPermissionElementInitiated() const;

  // Returns the position of the element that caused the prompt to open.
  std::optional<gfx::Rect> GetAnchorElementPosition() const;

  // Returns true if the request has two origins and should use the two origin
  // prompt. Returns false otherwise.
  bool ShouldUseTwoOriginPrompt() const;

  // Called when the user has granted the requested permission.
  // If |is_one_time| is true the permission will last until all tabs of
  // |origin| are closed or navigated away from, and then the permission will
  // automatically expire after 1 day.
  void PermissionGranted(bool is_one_time);

  // Called when the user has denied the requested permission.
  void PermissionDenied();

  // Called when the user has cancelled the permission request. This
  // corresponds to a denial, but is segregated in case the context needs to
  // be able to distinguish between an active refusal or an implicit refusal.
  void Cancelled(bool is_final_decision = true);

  // The UI this request was associated with was answered by the user.
  // It is safe for the request to be deleted at this point -- it will receive
  // no further message from the permission request system. This method will
  // eventually be called on every request which is not unregistered.
  // It is ok to call this method without actually resolving the request via
  // PermissionGranted(), PermissionDenied() or Canceled(). However, it will not
  // resolve the javascript promise from the requesting origin.
  void RequestFinished();

  // Used to record UMA for whether requests are associated with a user gesture.
  // To keep things simple this metric is only recorded for the most popular
  // request types.
  PermissionRequestGestureType GetGestureType() const;

  virtual const std::vector<std::string>& GetRequestedAudioCaptureDeviceIds()
      const;
  virtual const std::vector<std::string>& GetRequestedVideoCaptureDeviceIds()
      const;

  // Used on Android to determine what Android OS permissions are needed for
  // this permission request.
  ContentSettingsType GetContentSettingsType() const;

  void set_requesting_frame_id(content::GlobalRenderFrameHostId id) {
    data_.id.set_global_render_frame_host_id(id);
  }

  const content::GlobalRenderFrameHostId& get_requesting_frame_id() {
    return data_.id.global_render_frame_host_id();
  }

  // Permission name text fragment which can be used in permission prompts to
  // identify the permission being requested.
  virtual std::u16string GetPermissionNameTextFragment() const;

  bool uses_automatic_embargo() const { return uses_automatic_embargo_; }

 protected:
  // Sets whether this request is permission element initiated, for testing
  // subclasses only.
  void SetEmbeddedPermissionElementInitiatedForTesting(
      bool embedded_permission_element_initiated);

 private:
  PermissionRequestData data_;

  // Called once a decision is made about the permission.
  PermissionDecidedCallback permission_decided_callback_;

  // Called when the request is no longer in use so it can be deleted by the
  // caller.
  base::OnceClosure delete_callback_;

  const bool uses_automatic_embargo_ = true;

  base::WeakPtrFactory<PermissionRequest> weak_factory_{this};
};

}  // namespace permissions

#endif  // COMPONENTS_PERMISSIONS_PERMISSION_REQUEST_H_
