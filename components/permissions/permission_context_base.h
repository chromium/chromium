// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_PERMISSION_CONTEXT_BASE_H_
#define COMPONENTS_PERMISSIONS_PERMISSION_CONTEXT_BASE_H_

#include <memory>
#include <unordered_map>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/content_settings/core/browser/content_settings_observer.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/permissions/permission_request.h"
#include "components/permissions/permission_request_data.h"
#include "content/public/browser/permission_result.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy_feature.mojom-forward.h"

class GURL;

namespace permissions {
class PermissionRequestID;
}

namespace content {
class BrowserContext;
class RenderFrameHost;
class WebContents;
}  // namespace content

namespace permissions {

class Observer : public base::CheckedObserver {
 public:
  // Called whenever there is a potential permission status change for the
  // specified patterns. This function being called does not necessarily mean
  // that the result of |GetPermissionStatus| has changed for any particular
  // origin.
  virtual void OnPermissionChanged(
      const ContentSettingsPattern& primary_pattern,
      const ContentSettingsPattern& secondary_pattern,
      ContentSettingsTypeSet content_type_set) = 0;
};

// A one time grant will never last longer than this value.
static constexpr base::TimeDelta kOneTimePermissionMaximumLifetime =
    base::Hours(16);

using BrowserPermissionCallback = base::OnceCallback<void(ContentSetting)>;

// This base class contains common operations for granting permissions.
// It offers the following functionality:
//   - Creates a permission request when needed.
//   - If accepted/denied the permission is saved in content settings for
//     future uses (for the domain that requested it).
//   - If dismissed the permission is not saved but it's considered denied for
//     this one request
//   - In any case the BrowserPermissionCallback is executed once a decision
//     about the permission is made by the user.
// The bare minimum you need to create a new permission request is
//   - Define your new permission in the ContentSettingsType enum.
//   - Create a class that inherits from PermissionContextBase and passes the
//     new permission.
//   - Edit the PermissionRequest methods to add the new text.
//   - Make sure to update
//     third_party/blink/public/devtools_protocol/browser_protocol.pdl
//     even if you don't intend to do anything DevTools-specific; you will
//     run into problems with generated code otherwise.
//   - Hit several asserts for the missing plumbing and fix them :)
// After this you can override several other methods to customize behavior,
// in particular it is advised to override UpdateTabContext in order to manage
// the permission from the omnibox.
// See midi_permission_context.h/cc or push_permission_context.cc/h for some
// examples.

class PermissionContextBase : public content_settings::Observer {
 public:
  PermissionContextBase(
      content::BrowserContext* browser_context,
      ContentSettingsType content_settings_type,
      blink::mojom::PermissionsPolicyFeature permissions_policy_feature);
  ~PermissionContextBase() override;

  // A field trial used to enable the global permissions kill switch.
  // This is public so permissions that don't yet inherit from
  // PermissionContextBase can use it.
  static const char kPermissionsKillSwitchFieldStudy[];

  // The field trial param to enable the global permissions kill switch.
  // This is public so permissions that don't yet inherit from
  // PermissionContextBase can use it.
  static const char kPermissionsKillSwitchBlockedValue[];

  // |callback| is called upon resolution of the request, but not if a prompt
  // is shown and ignored.
  virtual void RequestPermission(PermissionRequestData request_data,
                                 BrowserPermissionCallback callback);

  // Returns whether the permission has been granted, denied etc.
  // |render_frame_host| may be nullptr if the call is coming from a context
  // other than a specific frame.
  content::PermissionResult GetPermissionStatus(
      content::RenderFrameHost* render_frame_host,
      const GURL& requesting_origin,
      const GURL& embedding_origin) const;

  // Returns whether the permission is usable by requesting/embedding origins.
  bool IsPermissionAvailableToOrigins(const GURL& requesting_origin,
                                      const GURL& embedding_origin) const;

  // Update |result| with any modifications based on the device state. For
  // example, if |result| is ALLOW but Chrome does not have the relevant
  // permission at the device level, but will prompt the user, return ASK.
  // This function updates the cached device permission status which can result
  // in the permission status changing and observers being notified.
  virtual content::PermissionResult UpdatePermissionStatusWithDeviceStatus(
      content::PermissionResult result,
      const GURL& requesting_origin,
      const GURL& embedding_origin);

  // Resets the permission to its default value.
  virtual void ResetPermission(const GURL& requesting_origin,
                               const GURL& embedding_origin);

  // Whether the permission status should take into account the device status
  // which can be provided by |UpdatePermissionStatusWithDeviceStatus|.
  virtual bool AlwaysIncludeDeviceStatus() const;

  // Whether the kill switch has been enabled for this permission.
  // public for permissions that do not use RequestPermission, like
  // camera and microphone, and for testing.
  bool IsPermissionKillSwitchOn() const;

  void AddObserver(permissions::Observer* permission_observer);
  void RemoveObserver(permissions::Observer* permission_observer);

  void MaybeUpdatePermissionStatusWithDeviceStatus();

  ContentSettingsType content_settings_type() const {
    return content_settings_type_;
  }

  void set_has_device_permission_for_test(std::optional<bool> has_permission) {
    has_device_permission_for_test_ = has_permission;
  }

 protected:
  virtual ContentSetting GetPermissionStatusInternal(
      content::RenderFrameHost* render_frame_host,
      const GURL& requesting_origin,
      const GURL& embedding_origin) const;

  // Called if generic checks (existing content setting, embargo, etc.) fail to
  // resolve a permission request. The default implementation prompts the user.
  virtual void DecidePermission(PermissionRequestData request_data,
                                BrowserPermissionCallback callback);

  // Updates stored content setting if persist is set, updates tab indicators
  // and runs the callback to finish the request.
  virtual void NotifyPermissionSet(const PermissionRequestID& id,
                                   const GURL& requesting_origin,
                                   const GURL& embedding_origin,
                                   BrowserPermissionCallback callback,
                                   bool persist,
                                   ContentSetting content_setting,
                                   bool is_one_time,
                                   bool is_final_decision);

  // Implementors can override this method to update the icons on the
  // url bar with the result of the new permission.
  virtual void UpdateTabContext(const PermissionRequestID& id,
                                const GURL& requesting_origin,
                                bool allowed) {}

  // Returns the browser context associated with this permission context.
  content::BrowserContext* browser_context() const;

  // Store the decided permission as a content setting.
  // virtual since the permission might be stored with different restrictions
  // (for example for desktop notifications).
  virtual void UpdateContentSetting(const GURL& requesting_origin,
                                    const GURL& embedding_origin,
                                    ContentSetting content_setting,
                                    bool is_one_time);

  // Whether the permission should be restricted to secure origins.
  virtual bool IsRestrictedToSecureOrigins() const;

  // Called by PermissionDecided when the user has made a permission decision.
  // Subclasses may override this method to perform context-specific logic
  // before the content setting is changed and the permission callback is run.
  virtual void UserMadePermissionDecision(const PermissionRequestID& id,
                                          const GURL& requesting_origin,
                                          const GURL& embedding_origin,
                                          ContentSetting content_setting);

  // content_settings::Observer:
  void OnContentSettingChanged(
      const ContentSettingsPattern& primary_pattern,
      const ContentSettingsPattern& secondary_pattern,
      ContentSettingsTypeSet content_type_set) override;

  // Implementors can override this method to use a different PermissionRequest
  // implementation.
  virtual std::unique_ptr<PermissionRequest> CreatePermissionRequest(
      content::WebContents* web_contents,
      PermissionRequestData request_data,
      PermissionRequest::PermissionDecidedCallback permission_decided_callback,
      base::OnceClosure delete_callback) const;

  // Implementors can override this method to avoid using automatic embargo.
  virtual bool UsesAutomaticEmbargo() const;

  base::ObserverList<permissions::Observer> permission_observers_;

  // Set by subclasses to inform the base class that they will handle adding
  // and removing themselves as observers to the HostContentSettingsMap.
  bool content_setting_observer_registered_by_subclass_ = false;

 private:
  friend class PermissionContextBaseTests;

  bool PermissionAllowedByPermissionsPolicy(
      content::RenderFrameHost* rfh) const;

  // Called when a request is no longer used so it can be cleaned up.
  void CleanUpRequest(const PermissionRequestID& id,
                      bool embedded_permission_element_initiated);

  // This is the callback for PermissionRequest and is called once the user
  // allows/blocks/dismisses a permission prompt.
  void PermissionDecided(const PermissionRequestID& id,
                         const GURL& requesting_origin,
                         const GURL& embedding_origin,
                         ContentSetting content_setting,
                         bool is_one_time,
                         bool is_final_decision);

  void NotifyObservers(const ContentSettingsPattern& primary_pattern,
                       const ContentSettingsPattern& secondary_pattern,
                       ContentSettingsTypeSet content_type_set) const;

  raw_ptr<content::BrowserContext> browser_context_;
  const ContentSettingsType content_settings_type_;
  const blink::mojom::PermissionsPolicyFeature permissions_policy_feature_;
  std::unordered_map<
      std::string,
      std::pair<std::unique_ptr<PermissionRequest>, BrowserPermissionCallback>>
      pending_requests_;

  mutable std::optional<bool> last_has_device_permission_result_ = std::nullopt;

  std::optional<bool> has_device_permission_for_test_;

  // Must be the last member, to ensure that it will be
  // destroyed first, which will invalidate weak pointers
  base::WeakPtrFactory<PermissionContextBase> weak_factory_{this};
};

}  // namespace permissions

#endif  // COMPONENTS_PERMISSIONS_PERMISSION_CONTEXT_BASE_H_
