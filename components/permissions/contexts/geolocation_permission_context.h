// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_CONTEXTS_GEOLOCATION_PERMISSION_CONTEXT_H_
#define COMPONENTS_PERMISSIONS_CONTEXTS_GEOLOCATION_PERMISSION_CONTEXT_H_

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/permissions/content_setting_permission_context_base.h"
#include "components/permissions/permission_context_base.h"
#include "components/permissions/permission_request_data.h"
#include "content/public/browser/browser_context.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/public/mojom/geolocation_control.mojom.h"

class PrefService;

namespace content {
class WebContents;
}

namespace permissions {

// This context handles geolocation permission requests.

// Geolocation has been represented by a ContentSetting for a long time.
// With approximate location (`features::kApproximateGeolocationPermission`)
// we plan to migrate away from this and rely on a permission state
// supporting options. Currently this context deals with both approaches, as the
// feature has not been launched yet.
//
// There are two separate storage locations for the permission state, and
// whether one or both are used, as well as which ContentSettingType is
// associated with this context, depends on whether
// features::kApproximateGeolocationPermission is active.

// - inactive: The associated ContentSettingsType is GEOLOCATION. Only the
//             ContentSetting for GEOLOCATION is used.
// - active: The associated ContentSettingsType is GEOLOCATION_WITH_OPTIONS. The
//           context manages the permission as a multi-state permission, which
//           is persisted as a PermissionSetting.
class GeolocationPermissionContext : public PermissionContextBase {
 public:
  // Delegate which allows embedders to modify the logic of the geolocation
  // permission context.
  class Delegate {
   public:
    virtual ~Delegate() = default;

    // Allows the delegate to override the context's DecidePermission() logic.
    // If this returns true, the base context's DecidePermission() will not be
    // called.
    virtual bool DecidePermission(const PermissionRequestData& request_data,
                                  BrowserPermissionCallback* callback,
                                  GeolocationPermissionContext* context) = 0;

#if BUILDFLAG(IS_ANDROID)
    // Returns whether or not this |web_contents| is interactable.
    virtual bool IsInteractable(content::WebContents* web_contents) = 0;

    // Gets the pref service associated with |browser_context|.
    virtual PrefService* GetPrefs(content::BrowserContext* browser_context) = 0;

    // Returns whether |requesting_origin| is the default search engine.
    virtual bool IsRequestingOriginDSE(content::BrowserContext* browser_context,
                                       const GURL& requesting_origin) = 0;
#endif
  };

  GeolocationPermissionContext(content::BrowserContext* browser_context,
                               std::unique_ptr<Delegate> delegate);

  GeolocationPermissionContext(const GeolocationPermissionContext&) = delete;
  GeolocationPermissionContext& operator=(const GeolocationPermissionContext&) =
      delete;

  ~GeolocationPermissionContext() override;

  void DecidePermission(
      std::unique_ptr<permissions::PermissionRequestData> request_data,
      BrowserPermissionCallback callback) override;

  void UpdateSetting(const PermissionRequestData& request_data,
                     PermissionSetting setting,
                     bool is_one_time) override;

  base::WeakPtr<GeolocationPermissionContext> GetWeakPtr();

  // Make this public for use by the delegate implementation.
  using PermissionContextBase::NotifyPermissionSet;

 protected:
  std::unique_ptr<Delegate> delegate_;

  std::unique_ptr<PermissionResolver> CreatePermissionResolver(
      const blink::mojom::PermissionDescriptorPtr& permission_descriptor)
      const override;

 private:
  void UpdateTabContext(const PermissionRequestData& request_data,
                        bool allowed) override;

  device::mojom::GeolocationControl* GetGeolocationControl();

  mojo::Remote<device::mojom::GeolocationControl> geolocation_control_;

  base::WeakPtrFactory<GeolocationPermissionContext> weak_factory_{this};
};

}  // namespace permissions

#endif  // COMPONENTS_PERMISSIONS_CONTEXTS_GEOLOCATION_PERMISSION_CONTEXT_H_
