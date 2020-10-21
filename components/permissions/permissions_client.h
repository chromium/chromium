// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_PERMISSIONS_CLIENT_H_
#define COMPONENTS_PERMISSIONS_PERMISSIONS_CLIENT_H_

#include "base/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "build/build_config.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/permissions/notification_permission_ui_selector.h"
#include "components/permissions/permission_prompt.h"
#include "components/permissions/permission_util.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "url/origin.h"

class GURL;
class HostContentSettingsMap;

namespace content {
class BrowserContext;
class WebContents;
}  // namespace content

namespace content_settings {
class CookieSettings;
}

namespace infobars {
class InfoBar;
class InfoBarManager;
}  // namespace infobars

namespace permissions {
class ChooserContextBase;
class PermissionDecisionAutoBlocker;
class PermissionManager;
class PermissionPromptAndroid;

// Interface to be implemented by permissions embedder to access embedder
// specific logic.
class PermissionsClient {
 public:
  PermissionsClient(const PermissionsClient&) = delete;
  PermissionsClient& operator=(const PermissionsClient&) = delete;

  PermissionsClient();
  virtual ~PermissionsClient();

  // Return the permissions client.
  static PermissionsClient* Get();

  // Retrieves the HostContentSettingsMap for this context. The returned pointer
  // has the same lifetime as |browser_context|.
  virtual HostContentSettingsMap* GetSettingsMap(
      content::BrowserContext* browser_context) = 0;

  // Retrieves the CookieSettings for this context.
  virtual scoped_refptr<content_settings::CookieSettings> GetCookieSettings(
      content::BrowserContext* browser_context) = 0;

  // Retrieves the subresource filter activation from browser website settings.
  virtual bool IsSubresourceFilterActivated(
      content::BrowserContext* browser_context,
      const GURL& url) = 0;

  // Retrieves the PermissionDecisionAutoBlocker for this context. The returned
  // pointer has the same lifetime as |browser_context|.
  virtual PermissionDecisionAutoBlocker* GetPermissionDecisionAutoBlocker(
      content::BrowserContext* browser_context) = 0;

  // Retrieves the PermissionManager for this context. The returned
  // pointer has the same lifetime as |browser_context|.
  virtual PermissionManager* GetPermissionManager(
      content::BrowserContext* browser_context) = 0;

  // Gets the ChooserContextBase for the given type and context, which must be a
  // *_CHOOSER_DATA value. May return null if the context does not exist.
  virtual ChooserContextBase* GetChooserContext(
      content::BrowserContext* browser_context,
      ContentSettingsType type) = 0;

  // Gets the embedder defined engagement score for this |origin|.
  virtual double GetSiteEngagementScore(
      content::BrowserContext* browser_context,
      const GURL& origin);

  // Determines whether some origins are "important". |origins| is an in-out
  // param that passes in the list of origins which need judgment as the first
  // item in each pair, and the determination of importance should be stored in
  // the second item in the pair (true meaning important).
  virtual void AreSitesImportant(
      content::BrowserContext* browser_context,
      std::vector<std::pair<url::Origin, bool>>* origins);

#if defined(OS_ANDROID) || defined(OS_CHROMEOS)
  // Returns whether cookie deletion is allowed for |browser_context| and
  // |origin|.
  // TODO(crbug.com/1081944): Remove this method and all code depending on it
  // when a proper fix is landed.
  virtual bool IsCookieDeletionDisabled(
      content::BrowserContext* browser_context,
      const GURL& origin);
#endif

  // Retrieves the ukm::SourceId (if any) associated with this |browser_context|
  // and |web_contents|. |web_contents| may be null. |callback| will be called
  // with the result, and may be run synchronously if the result is available
  // immediately.
  using GetUkmSourceIdCallback =
      base::OnceCallback<void(base::Optional<ukm::SourceId>)>;
  virtual void GetUkmSourceId(content::BrowserContext* browser_context,
                              const content::WebContents* web_contents,
                              const GURL& requesting_origin,
                              GetUkmSourceIdCallback callback);

  // Returns the icon ID that should be used for permissions UI for |type|. If
  // the embedder returns an empty IconId, the default icon for |type| will be
  // used.
  virtual PermissionRequest::IconId GetOverrideIconId(ContentSettingsType type);

  // Allows the embedder to provide a selector for chossing the UI to use for
  // notification permission requests. If the embedder returns null here, the
  // normal UI will be used.
  virtual std::unique_ptr<NotificationPermissionUiSelector>
  CreateNotificationPermissionUiSelector(
      content::BrowserContext* browser_context);

  using QuietUiReason = NotificationPermissionUiSelector::QuietUiReason;
  // Called for each request type when a permission prompt is resolved.
  virtual void OnPromptResolved(content::BrowserContext* browser_context,
                                PermissionRequestType request_type,
                                PermissionAction action,
                                const GURL& origin,
                                base::Optional<QuietUiReason> quiet_ui_reason);

  // Returns true if user has 3 consecutive notifications permission denies,
  // returns false otherwise.
  // Returns base::nullopt if the user is not in the adoptive activation quiet
  // ui dry run experiment group.
  virtual base::Optional<bool> HadThreeConsecutiveNotificationPermissionDenies(
      content::BrowserContext* browser_context);

  // Returns whether the |permission| has already been auto-revoked due to abuse
  // at least once for the given |origin|. Returns `nullopt` if permission
  // auto-revocation is not supported for a given permission type.
  virtual base::Optional<bool> HasPreviouslyAutoRevokedPermission(
      content::BrowserContext* browser_context,
      const GURL& origin,
      ContentSettingsType permission);

  // If the embedder returns an origin here, any requests matching that origin
  // will be approved. Requests that do not match the returned origin will
  // immediately be finished without granting/denying the permission.
  virtual base::Optional<url::Origin> GetAutoApprovalOrigin();

  // Allows the embedder to bypass checking the embedding origin when performing
  // permission availability checks. This is used for example when a permission
  // should only be available on secure origins. Return true to bypass embedding
  // origin checks for the passed in origins.
  virtual bool CanBypassEmbeddingOriginCheck(const GURL& requesting_origin,
                                             const GURL& embedding_origin);

  // Allows embedder to override the canonical origin for a permission request.
  // This is the origin that will be used for requesting/storing/displaying
  // permissions.
  virtual base::Optional<GURL> OverrideCanonicalOrigin(
      const GURL& requesting_origin,
      const GURL& embedding_origin);

#if defined(OS_ANDROID)
  // Returns whether the permission is controlled by the default search
  // engine (DSE). For example, in Chrome, making a search engine default
  // automatically grants notification permissions for the associated origin.
  virtual bool IsPermissionControlledByDse(
      content::BrowserContext* browser_context,
      ContentSettingsType type,
      const url::Origin& origin);

  // Resets the permission if it's controlled by the default search
  // engine (DSE). The return value is true if the permission was reset.
  virtual bool ResetPermissionIfControlledByDse(
      content::BrowserContext* browser_context,
      ContentSettingsType type,
      const url::Origin& origin);

  // Retrieves the InfoBarManager for the web contents. The returned
  // pointer has the same lifetime as |web_contents|.
  virtual infobars::InfoBarManager* GetInfoBarManager(
      content::WebContents* web_contents);

  // Allows the embedder to create an info bar to use as the permission prompt.
  // Might return null based on internal logic (e.g. |type| does not support
  // infobar permission prompts). The returned infobar is owned by the info bar
  // manager.
  virtual infobars::InfoBar* MaybeCreateInfoBar(
      content::WebContents* web_contents,
      ContentSettingsType type,
      base::WeakPtr<PermissionPromptAndroid> prompt);

  using PermissionsUpdatedCallback = base::OnceCallback<void(bool)>;

  // Prompts the user to accept system permissions for |content_settings_types|,
  // after they've already been denied. In Chrome, this shows an infobar.
  // |callback| will be run with |true| for success and |false| otherwise.
  virtual void RepromptForAndroidPermissions(
      content::WebContents* web_contents,
      const std::vector<ContentSettingsType>& content_settings_types,
      PermissionsUpdatedCallback callback);

  // Converts the given chromium |resource_id| (e.g. IDR_INFOBAR_TRANSLATE) to
  // an Android drawable resource ID. Returns 0 if a mapping wasn't found.
  virtual int MapToJavaDrawableId(int resource_id);
#else
  // Creates a permission prompt.
  // TODO(crbug.com/1025609): Move the desktop permission prompt implementation
  // into //components/permissions and remove this.
  virtual std::unique_ptr<PermissionPrompt> CreatePrompt(
      content::WebContents* web_contents,
      PermissionPrompt::Delegate* delegate);
#endif
};

}  // namespace permissions

#endif  // COMPONENTS_PERMISSIONS_PERMISSIONS_CLIENT_H_
