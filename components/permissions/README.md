# Permission Checks and Requests

[TOC]

## PermissionController

The
[PermissionController](https://cs.chromium.org/chromium/src/content/public/browser/permission_controller.h)
is the entry point for clients of the permissions infrastructure from both the
`//content` and the embedder (e.g. `//chrome`) layers.
[PermissionController](https://cs.chromium.org/chromium/src/content/public/browser/permission_controller.h)
provides access to the permissions API and can be reached as
`content::BrowserContext::GetPermissionController()` or
`Profile::GetPermissionController()`.

[PermissionController](https://cs.chromium.org/chromium/src/content/public/browser/permission_controller.h)
has the following API:
*   `blink::mojom::PermissionStatus PermissionController::GetPermissionStatusForWorker`
*   `blink::mojom::PermissionStatus PermissionController::GetPermissionStatusForCurrentDocument`
*   `blink::mojom::PermissionStatus PermissionController::GetPermissionStatusForOriginWithoutContext`
    Use this API only in special cases when there is no active document or
    worker. E.g., `PermissionType::PAYMENT_HANDLER` permission verification of
    payment providers in a PWA's manifest.
*   `PermissionController::RequestPermissionFromCurrentDocument`
*   `PermissionController::RequestPermissionsFromCurrentDocument`

## PermissionControllerImpl

The [PermissionControllerImpl](https://cs.chromium.org/chromium/src/content/browser/permissions/permission_controller_impl.h)
is the implementation of the
[PermissionController](https://cs.chromium.org/chromium/src/content/public/browser/permission_controller.h).
[PermissionControllerImpl](https://cs.chromium.org/chromium/src/content/browser/permissions/permission_controller_impl.h)
is meant to be used only internally in `//content` and is not available for
external clients.

[PermissionControllerImpl](https://cs.chromium.org/chromium/src/content/browser/permissions/permission_controller_impl.h)
provides various functionality such as:

*   Reset a permission's state to the default
*   Observe permissions changes
*   Override permission status for DevTools.

## PermissionManager and PermissionContextBase

The
[PermissionManager](https://cs.chromium.org/chromium/src/components/permissions/permission_manager.h)
is an implementation of
[PermissionControllerDelegate](https://cs.chromium.org/chromium/src/content/public/browser/permission_controller_delegate.h).
[PermissionManager](https://cs.chromium.org/chromium/src/components/permissions/permission_manager.h)
is a
[KeyedService](https://cs.chromium.org/chromium/src/components/keyed_service/core/keyed_service.h)
which means it is attached to a
[BrowserContext](https://cs.chromium.org/chromium/src/content/public/browser/browser_context.h).
It allows to get permission status for
[ContentSettingsType](https://cs.chromium.org/chromium/src/components/content_settings/core/common/content_settings_types.h).
That API should be used only to display permission status in UI like PageInfo
and SiteSettings.

Internally,
[PermissionManager](https://cs.chromium.org/chromium/src/components/permissions/permission_manager.h)
holds a list of PermissionsContexts, one per
[ContentSettingType](https://cs.chromium.org/chromium/src/components/content_settings/core/common/content_settings_types.h?l=17).
[PermissionContextBase](https://cs.chromium.org/chromium/src/components/permissions/permission_context_base.h)
is the base class for these contexts, and for every
[ContentSettingsType](https://cs.chromium.org/chromium/src/components/content_settings/core/common/content_settings_types.h)
there is a specific `...PermissionsContext` subclass.

> EXAMPLE:
> [NotificationPermissionContext](https://cs.chromium.org/chromium/src/chrome/browser/notifications/notification_permission_context.h)
> handles the
> "[NOTIFICATIONS](https://cs.chromium.org/chromium/src/components/content_settings/core/common/content_settings_types.h?l=33)"
> content setting.

In order to query, set, and reset the state of a permission, the
[HostContentSettingsMap](#HostContentSettingsMap) KeyedService is used, which
internally handles the more complicated things related to Content Settings.

In order to present the user with a permission prompt when a permission is
requested, [PermissionRequestManager](#PermissionRequestManager) is used.

## [HostContentSettingsMap](https://cs.chromium.org/chromium/src/components/content_settings/core/browser/host_content_settings_map.h)

### Patterns

In order to determine whether a permission is granted, blocked, needs a user
decision, etc, the appropriate content setting is checked. Content settings are
saved and retrieved using a key consisting of a 3-tuple of values:

*   Primary pattern: this is a
    [ContentSettingsPattern](https://cs.chromium.org/chromium/src/components/content_settings/core/common/content_settings_pattern.h)
    that represents the primary resource's URL. For permissions this refers to
    the URL of the document requesting/using the permission.
*   Secondary pattern: this is a
    [ContentSettingsPattern](https://cs.chromium.org/chromium/src/components/content_settings/core/common/content_settings_pattern.h)
    that is used for some types to provide some context around the primary
    resource. For permissions this refers to the URL of the top-level document
    of the page (except for "NOTIFICATIONS" in which case it's unused).
*   Content type: this is a
    [ContentSettingsType](https://cs.chromium.org/chromium/src/components/content_settings/core/common/content_settings_types.h)
    that specifies which setting this operation refers to.

A
[ContentSettingsPattern](https://cs.chromium.org/chromium/src/components/content_settings/core/common/content_settings_pattern.h)
is basically a URL where every part (scheme, host port, path) is allowed to be
either `Wildcard` or a specified value. Any other form or regex is not
supported.

A key that has `Wildcard` for both the primary and secondary patterns represents
the "Default" value for a specific
[ContentSettingsType](https://cs.chromium.org/chromium/src/components/content_settings/core/common/content_settings_types.h).
This is the least specific content setting that will match anything and serves
as a backup for when no more-specific setting has been set.

### Providers

When setting or retrieving a content setting, the
[HostContentSettingsMap](https://cs.chromium.org/chromium/src/components/content_settings/core/browser/host_content_settings_map.h)
uses a list of registered
[providers](https://cs.chromium.org/chromium/src/components/content_settings/core/browser/host_content_settings_map.h?type=cs&g=0&l=54).
This enum is sorted from highest priority to lowest. If a provider is able to
handle a specific operation it will do so and the following providers are
ignored, otherwise the next provider is queried and so on.

The underlying storage mechanism is provider-dependent.

## [PermissionRequestManager](https://cs.chromium.org/chromium/src/components/permissions/permission_request_manager.h)

The
[PermissionRequestManager](https://cs.chromium.org/chromium/src/components/permissions/permission_request_manager.h)
facilitates making permission requests via `AddRequest()`. Only one request
prompt is allowed to be in progress at a time, the manager holds a deque of
pending requests for all requests that are kept waiting until the current prompt
is resolved.

The
[PermissionRequestManager](https://cs.chromium.org/chromium/src/components/permissions/permission_request_manager.h)
is attached and scoped to the lifetime of a
[WebContents](https://cs.chromium.org/chromium/src/content/public/browser/web_contents.h?l=111).
When the
[WebContents](https://cs.chromium.org/chromium/src/content/public/browser/web_contents.h?l=111)
object is destroyed all current and queued requests are finalized as
"[IGNORED](https://cs.chromium.org/chromium/src/components/permissions/permission_util.h?l=26)".

It is possible to have more than one request be tied in to the same prompt. This
only happens when the requests are allowed to be grouped together and they all
requested one after another. Currently this is only the case for the Camera and
Microphone permissions which can be grouped into one Camera+Microphone prompt.

### Automatic actions

*   The `--deny-permission-prompts` command line switch will cause all
    permissions to be automatically denied.
*   Notification permission requests that arrive too soon after a previous
    notifications prompt has been resolved are automatically
    "[DENIED](https://cs.chromium.org/chromium/src/components/permissions/permission_util.h?l=24)".
    When a user denies a notifications permission prompt, the manager enters a
    "notifications cooldown mode" and a user-initiated navigation needs to
    happen before allowing another notifications permission prompt. This
    prevents an abusive pattern observed in the wild where a site is cycling
    through multiple subdomains and asking to show permissions until the user
    gives up or gives in.
*   On ChromeOS in WebKiosk mode, permission requests for the origin of the
    installed app are automatically
    "[GRANTED](https://cs.chromium.org/chromium/src/components/permissions/permission_util.h?l=23)".
    This needs to be done because there is no "user" to grant a permissions when
    the browser is simply used to continuously display some presentation app
    (for example: a TV in a store that displays on screen the camera feed).
*   Requests that duplicate a currently pending requests are put into a separate
    list instead of the regular queue. When a request is resolved, its
    duplicates are also resolved.
*   Based on various indicators (user's behavior, settings, site score etc) a
    request can be downgraded to use a quiet UI or be dropped entirely. For more
    info see the [Quiet permissions prompts](#Quiet-permissions-prompts)
    section.
*   If the current permission prompt is [quiet](#Quiet-permissions-prompts), it
    will be resolved as
    "[IGNORED](https://cs.chromium.org/chromium/src/components/permissions/permission_util.h?l=26)"
    when a new request is added. This prevents the permission requests being
    stuck around a prompt that is easily ignored by the user.

If the request has not been automatically resolved, it is added to deque of
`queued_requests_` from which it will be picked up as appropriate.

### Showing prompts

When a trigger causes the `DequeueRequestIfNeeded` function to be called it will
check if the necessary conditions are met to show a new permission prompt and it
will trigger showing the prompt. The conditions are:

*   The document needs to be loaded and visible
*   There is no permission prompt currently being shown to the user
*   There is a queued request waiting to be shown to the user

`DequeueRequestIfNeeded` is triggered when:

*   The document loads
*   The document becomes visible
*   A new request is added
*   A permission prompt is resolved
*   An async decision about a permission request is made

When the prompt needs to be shown to the user, a platform specific subclass of
[PermissionPrompt](https://cs.chromium.org/chromium/src/chrome/browser/ui/permission_bubble/permission_prompt.h)
is created which handles the creation and lifetime of the UI element and will
report user actions back to the
[PermissionRequestManager](https://cs.chromium.org/chromium/src/components/permissions/permission_request_manager.h).

The PermissionPrompt is responsible for deciding the exact UI surface and text
to present to the user based on information about the request.

### Quiet permissions prompts

For specific permission prompt requests a decision could be made to enforce a
quiet UI version of the permission prompt. Currently this only applies to
NOTIFICATIONS permission requests.

A quiet UI prompt can be triggered if any of these conditions are met:

*   The user has enabled quiet prompts in settings.
*   The site requesting the permissions is marked by Safe Browsing as having a
    bad reputation.

The
[AdaptiveQuietNotificationPermissionUiEnabler](https://cs.chromium.org/chromium/src/chrome/browser/permissions/adaptive_quiet_notification_permission_ui_enabler.h)
is responsible for recording the permission prompts outcomes and, if needed,
enabling the quiet UI in settings. The
[ContextualNotificationPermissionUiSelector](https://cs.chromium.org/chromium/src/chrome/browser/permissions/contextual_notification_permission_ui_selector.h)
checks if the quiet UI is enabled in settings (among other things) when choosing
the appropriate UI flavor.

A quiet UI prompt will use a right-side omnibox indicator on desktop or a
mini-infobar on Android.

## [PermissionsSecurityModelInteractiveUITest](https://cs.chromium.org/chromium/src/chrome/browser/permissions/permissions_security_model_interactive_uitest.cc)

### Testing

Requesting and verification of permissions does not feature unified behavior in
different environments. In other words, based on the preconditions, a permission
request could be shown or automatically declined. To make sure that all cases
work as intended, we introduced [PermissionsSecurityModelInteractiveUITest](https://cs.chromium.org/chromium/src/chrome/browser/permissions/permissions_security_model_interactive_uitest.cc).

### Add your own test

If you're adding a new environment that requires non-default behavior for
permissions, then you need to add a test in
[PermissionsSecurityModelInteractiveUITest](https://cs.chromium.org/chromium/src/chrome/browser/permissions/permissions_security_model_interactive_uitest.cc).

Steps to follow:

*   Create a new test fixture and activate your environment and get a
    [WebContents](https://source.chromium.org/chromium/chromium/src/+/main:content/public/browser/web_contents.h)
    or a [RenderFrameHost](https://source.chromium.org/chromium/chromium/src/+/main:content/public/browser/render_frame_host.h).
*   Create a method  `VerifyPermissionForXYZ`, where `XYZ` is the name of your
    environment. You can use the already defined `VerifyPermission` method if you
    expect to have default behavior for permissions. In `VerifyPermissionForXYZ`
    define a new behavior you expect to have.
*   Call `VerifyPermissionForXYZ` from your newly created test fixture.

> EXAMPLE:
> [PermissionRequestWithPortalTest](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/permissions/permissions_security_model_interactive_uitest.cc;drc=c662f11e160976c04682f41941aaeccad92ace48;bpv=0;bpt=1;l=1063)
> enables the [Portals](https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/common/features.cc;l=179;drc=c662f11e160976c04682f41941aaeccad92ace48).
> The [PortalActivation] test verifies that permissions are disabled in Portals.
> [VerifyPermissionsDeniedForPortal](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/permissions/permissions_security_model_interactive_uitest.cc;drc=c662f11e160976c04682f41941aaeccad92ace48;l=429) incapsulates all logic
> needed for a particular permission verification.

## Add new permission

See [add_new_permission.md](https://source.chromium.org/chromium/chromium/src/+/main:components/permissions/add_new_permission.md)
