// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_PERMISSIONS_CLIENT_H_
#define COMPONENTS_PERMISSIONS_PERMISSIONS_CLIENT_H_

#include <optional>

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/favicon/core/favicon_service.h"
#include "components/permissions/features.h"
#include "components/permissions/origin_keyed_permission_action_service.h"
#include "components/permissions/permission_prompt.h"
#include "components/permissions/permission_ui_selector.h"
#include "components/permissions/permission_uma_util.h"
#include "components/permissions/permission_util.h"
#include "components/permissions/request_type.h"
#include "content/public/browser/browser_context.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "url/origin.h"

#if BUILDFLAG(IS_ANDROID)
#include "components/messages/android/message_wrapper.h"
#endif

class GURL;
class HostContentSettingsMap;

namespace content {
class BrowserContext;
class WebContents;
}  // namespace content

namespace content_settings {
class CookieSettings;
}

namespace privacy_sandbox {
class TrackingProtectionSettings;
}  // namespace privacy_sandbox

namespace infobars {
class InfoBar;
class InfoBarManager;
}  // namespace infobars

namespace permissions {
class ObjectPermissionContextBase;
class PermissionActionsHistory;
class PermissionDecisionAutoBlocker;
class PermissionPromptAndroid;

// Interface to be implemented by permissions embedder to access embedder
// specific logic.
class PermissionsClient {
 public:
#if BUILDFLAG(IS_ANDROID)
  class PermissionMessageDelegate {
   public:
    virtual ~PermissionMessageDelegate() = default;
  };
#endif

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

  // Retrieves the TrackingProtectionSettings for this context.
  virtual privacy_sandbox::TrackingProtectionSettings*
  GetTrackingProtectionSettings(content::BrowserContext* browser_context) = 0;

  // Retrieves the subresource filter activation from browser website settings.
  virtual bool IsSubresourceFilterActivated(
      content::BrowserContext* browser_context,
      const GURL& url) = 0;

  // Holds and mediates access to an in-memory origin-keyed map, that holds the
  // last PermissionAction and its  timestamp for each Content Setting. Used for
  // metrics collection.
  virtual OriginKeyedPermissionActionService*
  GetOriginKeyedPermissionActionService(
      content::BrowserContext* browser_context) = 0;
  virtual PermissionActionsHistory* GetPermissionActionsHistory(
      content::BrowserContext* browser_context) = 0;
  // Retrieves the PermissionDecisionAutoBlocker for this context. The returned
  // pointer has the same lifetime as |browser_context|.
  virtual PermissionDecisionAutoBlocker* GetPermissionDecisionAutoBlocker(
      content::BrowserContext* browser_context) = 0;

  // Gets the ObjectPermissionContextBase for the given type and context, which
  // must be a
  // *_CHOOSER_DATA value. May return null if the context does not exist.
  virtual ObjectPermissionContextBase* GetChooserContext(
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

  // Returns whether cookie deletion is allowed for |browser_context| and
  // |origin|.
  // TODO(crbug.com/40130734): Remove this method and all code depending on it
  // when a proper fix is landed.
  virtual bool IsCookieDeletionDisabled(
      content::BrowserContext* browser_context,
      const GURL& origin);

  // Retrieves the ukm::SourceId (if any) associated with this
  // |permission_type|, |browser_context|, and |web_contents|. |web_contents|
  // may be null. |callback| will be called with the result, and may be run
  // synchronously if the result is available immediately.
  using GetUkmSourceIdCallback =
      base::OnceCallback<void(std::optional<ukm::SourceId>)>;
  virtual void GetUkmSourceId(ContentSettingsType permission_type,
                              content::BrowserContext* browser_context,
                              content::WebContents* web_contents,
                              const GURL& requesting_origin,
                              GetUkmSourceIdCallback callback);

  // Returns the icon ID that should be used for permissions UI for |type|. If
  // the embedder returns an empty IconId, the default icon for |type| will be
  // used.
  virtual IconId GetOverrideIconId(RequestType request_type);

  // Allows the embedder to provide a list of selectors for choosing the UI to
  // use for permission requests. If the embedder returns an empty list, the
  // normal UI will be used always. Then for each request, if none of the
  // returned selectors prescribe the quiet UI, the normal UI will be used.
  // Otherwise the quiet UI will be used. Selectors at lower indices have higher
  // priority when determining the quiet UI flavor.
  virtual std::vector<std::unique_ptr<PermissionUiSelector>>
  CreatePermissionUiSelectors(content::BrowserContext* browser_context);

  using QuietUiReason = PermissionUiSelector::QuietUiReason;

  virtual void TriggerPromptHatsSurveyIfEnabled(
      content::WebContents* web_contents,
      permissions::RequestType request_type,
      std::optional<permissions::PermissionAction> action,
      permissions::PermissionPromptDisposition prompt_disposition,
      permissions::PermissionPromptDispositionReason prompt_disposition_reason,
      permissions::PermissionRequestGestureType gesture_type,
      std::optional<base::TimeDelta> prompt_display_duration,
      bool is_post_prompt,
      const GURL& gurl,
      std::optional<
          permissions::feature_params::PermissionElementPromptPosition>
          pepc_prompt_position,
      ContentSetting initial_permission_status,
      base::OnceCallback<void()> hats_shown_callback_);

  // Called for each request type when a permission prompt is resolved.
  virtual void OnPromptResolved(
      RequestType request_type,
      PermissionAction action,
      const GURL& origin,
      PermissionPromptDisposition prompt_disposition,
      PermissionPromptDispositionReason prompt_disposition_reason,
      PermissionRequestGestureType gesture_type,
      std::optional<QuietUiReason> quiet_ui_reason,
      base::TimeDelta prompt_display_duration,
      std::optional<
          permissions::feature_params::PermissionElementPromptPosition>
          pepc_prompt_position,
      ContentSetting initial_permission_status,
      content::WebContents* web_contents);

  // Returns true if user has 3 consecutive notifications permission denies,
  // returns false otherwise.
  // Returns std::nullopt if the user is not in the adoptive activation quiet
  // ui dry run experiment group.
  virtual std::optional<bool> HadThreeConsecutiveNotificationPermissionDenies(
      content::BrowserContext* browser_context);

  // Returns whether the |permission| has already been auto-revoked due to abuse
  // at least once for the given |origin|. Returns `nullopt` if permission
  // auto-revocation is not supported for a given permission type.
  virtual std::optional<bool> HasPreviouslyAutoRevokedPermission(
      content::BrowserContext* browser_context,
      const GURL& origin,
      ContentSettingsType permission);

  // If the embedder returns an origin here, any requests matching that origin
  // will be approved. Requests that do not match the returned origin will
  // immediately be finished without granting/denying the permission.
  virtual std::optional<url::Origin> GetAutoApprovalOrigin(
      content::BrowserContext* browser_context);

  // If the embedder returns whether the requesting origin should be able to
  // access browser permissions. The browser permissions would be auto approved.
  virtual std::optional<PermissionAction> GetAutoApprovalStatus(
      content::BrowserContext* browser_context,
      const GURL& origin);

  // Allows the embedder to bypass checking the embedding origin when performing
  // permission availability checks. This is used for example when a permission
  // should only be available on secure origins. Return true to bypass embedding
  // origin checks for the passed in origins.
  virtual bool CanBypassEmbeddingOriginCheck(const GURL& requesting_origin,
                                             const GURL& embedding_origin);

  // Allows embedder to override the canonical origin for a permission request.
  // This is the origin that will be used for requesting/storing/displaying
  // permissions.
  virtual std::optional<GURL> OverrideCanonicalOrigin(
      const GURL& requesting_origin,
      const GURL& embedding_origin);

  // Checks if `requesting_origin` and `embedding_origin` are the new tab page
  // origins.
  virtual bool DoURLsMatchNewTabPage(const GURL& requesting_origin,
                                     const GURL& embedding_origin);

  // Determines the reason why a prompt was ignored.
  virtual permissions::PermissionIgnoredReason DetermineIgnoreReason(
      content::WebContents* web_contents);

#if BUILDFLAG(IS_ANDROID)
  // Returns whether the given origin matches the default search
  // engine (DSE) origin.
  virtual bool IsDseOrigin(content::BrowserContext* browser_context,
                           const url::Origin& origin);

  // Retrieves the InfoBarManager for the web contents. The returned
  // pointer has the same lifetime as |web_contents|.
  virtual infobars::InfoBarManager* GetInfoBarManager(
      content::WebContents* web_contents);

  // Allows the embedder to create an info bar to use as the
  // permission prompt. Might return null based on internal logic
  // (e.g. |type| does not support infobar permission prompts). The
  // returned infobar is owned by the info bar manager.
  virtual infobars::InfoBar* MaybeCreateInfoBar(
      content::WebContents* web_contents,
      ContentSettingsType type,
      base::WeakPtr<PermissionPromptAndroid> prompt);

  // Allows the embedder to create a message UI to use as the
  // permission prompt. Returns the pointer to the message UI if the
  // message UI is successfully created, nullptr otherwise, e.g. if
  // the messages-prompt is not supported for `type`.
  virtual std::unique_ptr<PermissionMessageDelegate> MaybeCreateMessageUI(
      content::WebContents* web_contents,
      ContentSettingsType type,
      base::WeakPtr<PermissionPromptAndroid> prompt);

  using PermissionsUpdatedCallback = base::OnceCallback<void(bool)>;

  // Prompts the user to accept system permissions for
  // |content_settings_types|, after they've already been denied. In
  // Chrome, this shows an infobar. |callback| will be run with
  // |true| for success and |false| otherwise.
  virtual void RepromptForAndroidPermissions(
      content::WebContents* web_contents,
      const std::vector<ContentSettingsType>& content_settings_types,
      const std::vector<ContentSettingsType>& filtered_content_settings_types,
      const std::vector<std::string>& required_permissions,
      const std::vector<std::string>& optional_permissions,
      PermissionsUpdatedCallback callback);

  // Converts the given chromium |resource_id| (e.g.
  // IDR_INFOBAR_TRANSLATE) to an Android drawable resource ID.
  // Returns 0 if a mapping wasn't found.
  virtual int MapToJavaDrawableId(int resource_id);
#else
  // Creates a permission prompt.
  // TODO(crbug.com/40107932): Move the desktop permission prompt
  // implementation into //components/permissions and remove this.
  virtual std::unique_ptr<PermissionPrompt> CreatePrompt(
      content::WebContents* web_contents,
      PermissionPrompt::Delegate* delegate);
#endif

  // Returns true if the browser has the necessary permission(s) from the
  // platform to provide a particular permission-gated capability to sites. This
  // can include both app-specific permissions relevant to the browser and
  // device-wide permissions.
  virtual bool HasDevicePermission(ContentSettingsType type) const;

  // Returns true if the browser is able to request from the platform the
  // necessary permission(s) needed to provide a particular permission-gated
  // capability to sites.
  virtual bool CanRequestDevicePermission(ContentSettingsType type) const;

  virtual favicon::FaviconService* GetFaviconService(
      content::BrowserContext* browser_context);
};

}  // namespace permissions

#endif  // COMPONENTS_PERMISSIONS_PERMISSIONS_CLIENT_H_
