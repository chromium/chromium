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
#include "components/permissions/permission_decision.h"
#include "components/permissions/permission_request.h"
#include "components/permissions/permission_request_data.h"
#include "components/permissions/resolvers/permission_resolver.h"
#include "content/public/browser/permission_result.h"
#include "services/network/public/mojom/permissions_policy/permissions_policy_feature.mojom.h"
#include "third_party/blink/public/mojom/permissions/permission.mojom-forward.h"

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

// Default one time permission expiration time.
static constexpr base::TimeDelta kOneTimePermissionTimeout = base::Minutes(5);

// A one time grant will never last longer than this value.
static constexpr base::TimeDelta kOneTimePermissionMaximumLifetime =
    base::Hours(16);

using BrowserPermissionCallback =
    base::OnceCallback<void(content::PermissionResult)>;

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
      network::mojom::PermissionsPolicyFeature permissions_policy_feature);
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
  virtual void RequestPermission(
      std::unique_ptr<PermissionRequestData> request_data,
      BrowserPermissionCallback callback);

  // Called in a permission request flow, to retrieve the current permission
  // status with given a request_data. |render_frame_host| may be nullptr.
  content::PermissionResult GetPermissionStatus(
      const PermissionRequestData& request_data,
      content::RenderFrameHost* render_frame_host) const;

  // Returns whether the permission has been granted, denied etc. given a
  // PermissionResolver. |render_frame_host| may be nullptr if the call is
  // coming from a context other than a specific frame.
  content::PermissionResult GetPermissionStatus(
      const PermissionResolver& resolver,
      content::RenderFrameHost* render_frame_host,
      const GURL& requesting_origin,
      const GURL& embedding_origin) const;

  // Returns whether the permission has been granted, denied etc. given a
  // PermissionDescriptorPtr. |render_frame_host| may be nullptr if the call is
  // coming from a context other than a specific frame.
  content::PermissionResult GetPermissionStatus(
      const blink::mojom::PermissionDescriptorPtr& permission_descriptor,
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
      content::WebContents* web_contents,
      content::PermissionResult result,
      const GURL& requesting_origin,
      const GURL& embedding_origin);

  // Resets the permission.
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

  // Creates a PermissionResolver for the PermissionDescriptorPtr. The default
  // implementation creates a ContentSettingPermissionResolver.
  virtual std::unique_ptr<PermissionResolver> CreatePermissionResolver(
      const blink::mojom::PermissionDescriptorPtr& permission_descriptor) const;

  // Update the value of `last_has_device_permission_result_` and notify
  // observers if it changes.
  void MaybeUpdateCachedHasDevicePermission(content::WebContents* web_contents);

  ContentSettingsType content_settings_type() const {
    return content_settings_type_;
  }

  void set_has_device_permission_for_test(std::optional<bool> has_permission) {
    has_device_permission_for_test_ = has_permission;
  }

 protected:
  // Retrieves the current permission status. |render_frame_host| may be
  // nullptr.
  virtual PermissionSetting GetPermissionStatusInternal(
      content::RenderFrameHost* render_frame_host,
      const GURL& requesting_origin,
      const GURL& embedding_origin) const;

  // Called if generic checks (existing content setting, embargo, etc.) fail to
  // resolve a permission request. The default implementation prompts the user.
  virtual void DecidePermission(
      std::unique_ptr<PermissionRequestData> request_data,
      BrowserPermissionCallback callback);

  // Updates stored setting if persist is set, updates tab indicators
  // and runs the callback to finish the request.
  virtual void NotifyPermissionSet(const PermissionRequestData& request_data,
                                   BrowserPermissionCallback callback,
                                   bool persist,
                                   PermissionDecision decision,
                                   bool is_final_decision);

  // Implementors can override this method to update the icons on the
  // url bar with the result of the new permission.
  virtual void UpdateTabContext(const PermissionRequestData& request_data,
                                bool allowed) {}

  // Returns the browser context associated with this permission context.
  content::BrowserContext* browser_context() const;

  // Store the decided permission state. Virtual since the permission might be
  // stored with different restrictions (for example for desktop notifications).
  virtual void UpdateSetting(const PermissionRequestData& request_data,
                             PermissionSetting setting,
                             bool is_one_time);

  // Whether the permission should be restricted to secure origins.
  virtual bool IsRestrictedToSecureOrigins() const;

  // Called by PermissionDecided when the user has made a permission decision.
  // Subclasses may override this method to perform context-specific logic
  // before the content setting is changed and the permission callback is run.
  virtual void UserMadePermissionDecision(const PermissionRequestID& id,
                                          const GURL& requesting_origin,
                                          const GURL& embedding_origin,
                                          PermissionDecision decision);

  // content_settings::Observer:
  void OnContentSettingChanged(
      const ContentSettingsPattern& primary_pattern,
      const ContentSettingsPattern& secondary_pattern,
      ContentSettingsTypeSet content_type_set) override;

  // Implementors can override this method to use a different PermissionRequest
  // implementation.
  virtual std::unique_ptr<PermissionRequest> CreatePermissionRequest(
      content::WebContents* web_contents,
      std::unique_ptr<PermissionRequestData> request_data,
      PermissionRequest::PermissionDecidedCallback permission_decided_callback,
      base::OnceClosure request_finished_callback) const;

  // Implementors can override this method to avoid using automatic embargo.
  virtual bool UsesAutomaticEmbargo() const;

  // Derived classes can use this function to find some particular permission
  // request.
  const PermissionRequest* FindPermissionRequest(
      const PermissionRequestID& id) const;

  // Implementors can override this method to use a different embedding origin.
  // TODO(crbug.com/40220500): This should return a url::Origin instead.
  virtual GURL GetEffectiveEmbedderOrigin(content::RenderFrameHost* rfh) const;

  base::ObserverList<permissions::Observer> permission_observers_;

  // Set by subclasses to inform the base class that they will handle adding
  // and removing themselves as observers to the HostContentSettingsMap.
  bool content_setting_observer_registered_by_subclass_ = false;

#if BUILDFLAG(IS_ANDROID)
  std::optional<bool> enabled_app_level_notification_permission_for_testing_ =
      std::nullopt;
#endif  // BUILDFLAG(IS_ANDROID)

 private:
  friend class PermissionContextBaseTests;

  bool PermissionAllowedByPermissionsPolicy(
      content::RenderFrameHost* rfh) const;

  // Called when a request is no longer used so it can be cleaned up.
  void CleanUpRequest(content::WebContents* web_contents,
                      const PermissionRequestID& id);
  void CleanUpRequestEmbeddedPermissionElement(
      content::WebContents* web_contents,
      const PermissionRequestID& id,
      BrowserPermissionCallback callback,
      PermissionDecision decision,
      PermissionSetting new_value);
  // This is the callback for PermissionRequest and is called once the user
  // allows/blocks/dismisses a permission prompt.
  void PermissionDecided(PermissionDecision decision,
                         bool is_final_decision,
                         const PermissionRequestData& request_data);

  void NotifyObservers(const ContentSettingsPattern& primary_pattern,
                       const ContentSettingsPattern& secondary_pattern,
                       ContentSettingsTypeSet content_type_set) const;

  raw_ptr<content::BrowserContext> browser_context_;
  const ContentSettingsType content_settings_type_;
  const network::mojom::PermissionsPolicyFeature permissions_policy_feature_;
  std::unordered_map<
      std::string,
      std::pair<base::WeakPtr<PermissionRequest>, BrowserPermissionCallback>>
      pending_requests_;

  mutable std::optional<bool> last_has_device_permission_result_ = std::nullopt;

  std::optional<bool> has_device_permission_for_test_;

  // Must be the last member, to ensure that it will be
  // destroyed first, which will invalidate weak pointers
  base::WeakPtrFactory<PermissionContextBase> weak_factory_{this};
};

}  // namespace permissions

#endif  // COMPONENTS_PERMISSIONS_PERMISSION_CONTEXT_BASE_H_
