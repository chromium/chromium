# Permission Checks and Requests

[TOC]

## PermissionController

The
[PermissionController][PermissionController]
is the entry point for clients of the permissions infrastructure from both the
`//content` and the embedder (e.g. `//chrome`) layers.
[PermissionController][PermissionController]
provides access to the permissions API and can be reached as
`content::BrowserContext::GetPermissionController()` or
`Profile::GetPermissionController()`.

[PermissionController][PermissionController]
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

The [PermissionControllerImpl][PermissionControllerImpl]
is the implementation of the
[PermissionController][PermissionController].
[PermissionControllerImpl][PermissionControllerImpl]
is meant to be used only internally in `//content` and is not available for
external clients.

[PermissionControllerImpl][PermissionControllerImpl]
provides various functionality such as:

*   Reset a permission's state to the default
*   Observe permissions changes
*   Override permission status for DevTools.

## PermissionManager and PermissionContextBase

The
[PermissionManager][PermissionManager]
is an implementation of
[PermissionControllerDelegate][PermissionControllerDelegate].
[PermissionManager][PermissionManager]
is a
[KeyedService][KeyedService]
which means it is attached to a
[BrowserContext][BrowserContext].
It allows to get permission status for
[ContentSettingsType][ContentSettingsType].
That API should be used only to display permission status in UI like PageInfo
and SiteSettings.

Internally,
[PermissionManager][PermissionManager]
holds a list of PermissionsContexts, one per
[ContentSettingsType][ContentSettingsType].
[PermissionContextBase][PermissionContextBase]
is the base class for these contexts, and for every
[ContentSettingsType][ContentSettingsType]
there is a specific `...PermissionsContext` subclass.

> EXAMPLE:
> [NotificationPermissionContext][NotificationPermissionContext]
> handles the
> "[NOTIFICATIONS][NOTIFICATIONS]"
> content setting.

In order to query, set, and reset the state of a permission, the
[HostContentSettingsMap](#HostContentSettingsMap) KeyedService is used, which
internally handles the more complicated things related to Content Settings.

In order to present the user with a permission prompt when a permission is
requested, [PermissionRequestManager](#PermissionRequestManager) is used.

## [HostContentSettingsMap][HostContentSettingsMap]

### Patterns

In order to determine whether a permission is granted, blocked, needs a user
decision, etc, the appropriate content setting is checked. Content settings are
saved and retrieved using a key consisting of a 3-tuple of values:

*   Primary pattern: this is a
    [ContentSettingsPattern][ContentSettingsPattern]
    that represents the primary resource's URL. For permissions this refers to
    the URL of the document requesting/using the permission.
*   Secondary pattern: this is a
    [ContentSettingsPattern][ContentSettingsPattern]
    that is used for some types to provide some context around the primary
    resource. For permissions this refers to the URL of the top-level document
    of the page (except for "NOTIFICATIONS" in which case it's unused).
*   Content type: this is a
    [ContentSettingsType][ContentSettingsType]
    that specifies which setting this operation refers to.

A
[ContentSettingsPattern][ContentSettingsPattern]
is basically a URL where every part (scheme, host port, path) is allowed to be
either `Wildcard` or a specified value. Any other form or regex is not
supported.

A key that has `Wildcard` for both the primary and secondary patterns represents
the "Default" value for a specific
[ContentSettingsType][ContentSettingsType].
This is the least specific content setting that will match anything and serves
as a backup for when no more-specific setting has been set.

### Providers

When setting or retrieving a content setting, the
[HostContentSettingsMap][HostContentSettingsMap]
uses a list of registered
[providers][providers].
This enum is sorted from highest priority to lowest. If a provider is able to
handle a specific operation it will do so and the following providers are
ignored, otherwise the next provider is queried and so on.

The underlying storage mechanism is provider-dependent.

## [PermissionRequestManager][PermissionRequestManager]

The
[PermissionRequestManager][PermissionRequestManager]
facilitates making permission requests via `AddRequest()`. Only one request
prompt is allowed to be in progress at a time, the manager holds a deque of
pending requests for all requests that are kept waiting until the current prompt
is resolved.

The
[PermissionRequestManager][PermissionRequestManager]
is attached and scoped to the lifetime of a
[WebContents][WebContents].
When the
[WebContents][WebContents]
object is destroyed all current and queued requests are finalized as
"[IGNORED][IGNORED]".

It is possible to have more than one request be tied in to the same prompt. This
only happens when the requests are allowed to be grouped together and they all
requested one after another. Currently this is only the case for the Camera and
Microphone permissions which can be grouped into one Camera+Microphone prompt.

### Automatic actions

*   The `--deny-permission-prompts` command line switch will cause all
    permissions to be automatically denied.
*   Notification permission requests that arrive too soon after a previous
    notifications prompt has been resolved are automatically
    "[DENIED][DENIED]".
    When a user denies a notifications permission prompt, the manager enters a
    "notifications cooldown mode" and a user-initiated navigation needs to
    happen before allowing another notifications permission prompt. This
    prevents an abusive pattern observed in the wild where a site is cycling
    through multiple subdomains and asking to show permissions until the user
    gives up or gives in.
*   On ChromeOS in WebKiosk mode, permission requests for the origin of the
    installed app are automatically
    "[GRANTED][GRANTED]".
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
    "[IGNORED][IGNORED]"
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
[PermissionPrompt][PermissionPrompt]
is created which handles the creation and lifetime of the UI element and will
report user actions back to the
[PermissionRequestManager][PermissionRequestManager].

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
[ContextualNotificationPermissionUiSelector][ContextualNotificationPermissionUiSelector]
checks if the quiet UI is enabled in settings (among other things) when choosing
the appropriate UI flavor.

A quiet UI prompt will use a right-side omnibox indicator on desktop or a
mini-infobar on Android.

## Security and Context Verifications

Permission requests and status queries undergo several security and environment-specific verifications before a decision is made. These checks are primarily implemented in `PermissionContextBase` and `PermissionControllerImpl`.

### Frame-Level Verifications

*   **Fenced Frames**: Permissions are denied for documents loaded inside a fenced frame (`content::RenderFrameHost::IsNestedWithinFencedFrame()`). Fenced frames do not currently support permission requests to preserve privacy boundaries.
*   **Inactive or BFCache Frames**: Requests are disallowed if the frame is inactive. Attempting a request on an inactive frame may trigger eviction from the Back/Forward Cache.

### Origin and URL Verifications

*   **Invalid URLs**: Requests require valid requesting and embedding origins. Invalid URLs (e.g., in some popup scenarios) are automatically rejected.
*   **Secure Origins**: Permissions are generally restricted to secure origins. This is verified using `network::IsUrlPotentiallyTrustworthy()`. Some permissions can bypass this check under specific conditions controlled by the embedder.
*   **Virtual URL Mismatch**: Requests are denied if there is an origin mismatch between the document's loaded URL and its virtual URL (the one visible to the user). This prevents phishing or deceptive UI where a site loads content from one origin but presents another to the user.

### Policy and Kill-Switch Verifications

*   **Permissions Policy**: Checked to ensure the feature is allowed for the specific frame. If a policy disables the feature (e.g., camera or geolocation), the request is automatically denied.
*   **Kill Switch**: Individual permissions can be disabled globally or for specific groups via Finch field trials (`PermissionContextBase::IsPermissionKillSwitchOn`).

### Environment and Platform-Specific Verifications

*   **Guest View**: Content inside a `GuestView` (like `<webview>`) may have different permission behavior. Checks ensure that permissions granted inside a guest are not inappropriately shared across separate `StoragePartition`s.
*   **Glic Actor**: Permissions may be auto-rejected if an actor (e.g., an assistant surface) is actively operating on the `WebContents` to prevent ambient eavesdropping or privilege escalation risks.
*   **App-Level Settings (Android)**: For the Notifications permission, verification checks if Chrome has the corresponding system-level permission enabled.

## [PermissionsSecurityModelInteractiveUITest][PermissionsSecurityModelInteractiveUITest]

### Testing

Requesting and verification of permissions does not feature unified behavior in
different environments. In other words, based on the preconditions, a permission
request could be shown or automatically declined. To make sure that all cases
work as intended, we introduced [PermissionsSecurityModelInteractiveUITest][PermissionsSecurityModelInteractiveUITest].

### Add your own test

If you're adding a new environment that requires non-default behavior for
permissions, then you need to add a test in
[PermissionsSecurityModelInteractiveUITest][PermissionsSecurityModelInteractiveUITest].

Steps to follow:

*   Create a new test fixture and activate your environment and get a
    [WebContents][WebContents]
    or a [RenderFrameHost][RenderFrameHost].
*   Create a method  `VerifyPermissionForXYZ`, where `XYZ` is the name of your
    environment. You can use the already defined `VerifyPermission` method if you
    expect to have default behavior for permissions. In `VerifyPermissionForXYZ`
    define a new behavior you expect to have.
*   Call `VerifyPermissionForXYZ` from your newly created test fixture.

> EXAMPLE:
> `PermissionRequestFencedFrameTest` verifies that permissions are disabled
> inside fenced frames. `VerifyPermissionsDeniedForFencedFrame` (line 367)
> encapsulates all logic needed for a particular permission verification.

## Add new permission

See [add_new_permission.md][add_new_permissionmd]


## Permissions Embargo System

The permissions embargo system automatically blocks permission requests from
origins that users have repeatedly dismissed or ignored. When an origin is
placed under embargo, all subsequent permission requests are automatically
denied with `PermissionStatus::DENIED` until the embargo expires. After expiry,
the origin may request permission again — but if the user dismisses or ignores the
prompt again, it will be re-embargoed.

The [PermissionDecisionAutoBlocker][PermissionDecisionAutoBlocker] is
responsible for the core logic of recording actions, incrementing counters, and
determining if an origin should be under embargo. This is orchestrated by the
[PermissionRequestManager](#PermissionRequestManager).

### Embargo Triggers and Expiry

Embargoes are triggered when a user performs a specific number of dismissals or
ignores. The exact threshold values and embargo durations depend on the
permission type (e.g., standard permissions, FedCM) and the UI surface presented
to the user (e.g., loud vs. quiet prompts). There are three types of actions
that can trigger an embargo: dismissals, ignores, and prompt displays.

Thresholds are evaluated using `OR` logic between the general and quiet-UI specific
counters. For example, an embargo is triggered if either the general ignore counter
OR the quiet-UI ignore counter exceeds its respective threshold.

*   **Standard Permissions (Loud UI):** Triggered after
    [`kDefaultDismissalsBeforeBlock`][kDefaultDismissalsBeforeBlock] dismissals
    or [`kDefaultIgnoresBeforeBlock`][kDefaultIgnoresBeforeBlock] ignores. The
    embargo lasts for [`kDefaultEmbargoDays`][kDefaultEmbargoDays] days.
*   **Standard Permissions (Quiet UI):** Triggered after
    [`kDefaultDismissalsBeforeBlockWithQuietUi`][kDefaultDismissalsBeforeBlockWithQuietUi]
    dismissals or
    [`kDefaultIgnoresBeforeBlockWithQuietUi`][kDefaultIgnoresBeforeBlockWithQuietUi]
    ignores. The embargo lasts for
    [`kDefaultEmbargoDays`][kDefaultEmbargoDays] days.
*   **Android Clapper (Loud UI):** When the `kPermissionsAndroidClapperLoud`
    feature is enabled, the ignore threshold drops to
    [`kClapperIgnoresBeforeBlock`][kClapperIgnoresBeforeBlock] ignores.
*   **FedCM:** Triggered after
    [`kFederatedIdentityApiDismissalsBeforeBlock`][kFederatedIdentityApiDismissalsBeforeBlock]
    dismissals. Has escalating embargo durations based on the number of
    consecutive dismissals, defined in
    [`kFederatedIdentityApiEmbargoDurationDismiss`][kFederatedIdentityApiEmbargoDurationDismiss].
*   **FedCM Auto-Reauthn (Display Embargo):** Triggered simply by displaying the
    prompt. The embargo lasts for
    [`kFederatedIdentityAutoReauthnEmbargoDuration`][kFederatedIdentityAutoReauthnEmbargoDuration].

**Action counters persist across embargo expiry.** This means that if an origin
is unembargoed because the time duration expired, a single subsequent dismissal
or ignore will immediately re-trigger the embargo because the underlying counter
still meets the threshold.

When a user explicitly grants a permission (resulting in `GRANTED`/`GRANTED_ONCE`), any
existing embargo and all associated counters for that origin and permission type
are cleared.

### Exemptions

Certain requests bypass the embargo system entirely:
*   **Embedded Permission Elements (PEPC):** Requests initiated from a
    `<permission>` HTML element are not subject to embargo recording.
*   **Automatic Embargo Opt-Out:** Individual `PermissionRequest` subclasses can
    opt out of the embargo system by returning `false` for
    `uses_automatic_embargo()`.

### Key Files

*   [`components/permissions/permission_decision_auto_blocker.h`][componentspermissionspermission_decision_auto_blockerh]
    / [`.cc`][permission_decision_auto_blocker_cc]: Core logic, embargo status
    evaluation, and all constants defining thresholds and durations.
*   [`components/permissions/permission_request_manager.h`][PermissionRequestManager]
    / [`.cc`][permission_request_manager_cc]: Orchestration, action recording,
    and recording quiet UI prompt timeouts as ignores.
*   [`components/permissions/permission_request.h`][componentspermissionspermission_requesth]:
    Defines exemptions (e.g., `uses_automatic_embargo()`).
*   [`components/permissions/features.h`][componentspermissionsfeaturesh]:
    Feature flags controlling new permission UIs and embargo behaviors.

[PermissionController]: https://source.chromium.org/chromium/chromium/src/+/main:content/public/browser/permission_controller.h
[PermissionControllerImpl]: https://source.chromium.org/chromium/chromium/src/+/main:content/browser/permissions/permission_controller_impl.h
[PermissionManager]: https://source.chromium.org/chromium/chromium/src/+/main:components/permissions/permission_manager.h
[PermissionControllerDelegate]: https://source.chromium.org/chromium/chromium/src/+/main:content/public/browser/permission_controller_delegate.h
[KeyedService]: https://source.chromium.org/chromium/chromium/src/+/main:components/keyed_service/core/keyed_service.h
[BrowserContext]: https://source.chromium.org/chromium/chromium/src/+/main:content/public/browser/browser_context.h
[ContentSettingsType]: https://source.chromium.org/chromium/chromium/src/+/main:components/content_settings/core/common/content_settings_types.h
[PermissionContextBase]: https://source.chromium.org/chromium/chromium/src/+/main:components/permissions/permission_context_base.h
[NotificationPermissionContext]: https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/notifications/notification_permission_context.h
[NOTIFICATIONS]: https://source.chromium.org/chromium/chromium/src/+/main:components/content_settings/core/common/content_settings_types.mojom?q=NOTIFICATIONS
[HostContentSettingsMap]: https://source.chromium.org/chromium/chromium/src/+/main:components/content_settings/core/browser/host_content_settings_map.h
[ContentSettingsPattern]: https://source.chromium.org/chromium/chromium/src/+/main:components/content_settings/core/common/content_settings_pattern.h
[providers]: https://source.chromium.org/chromium/chromium/src/+/main:components/content_settings/core/browser/host_content_settings_map.h?q=ProviderType
[PermissionRequestManager]: https://source.chromium.org/chromium/chromium/src/+/main:components/permissions/permission_request_manager.h
[WebContents]: https://source.chromium.org/chromium/chromium/src/+/main:content/public/browser/web_contents.h?q=class+WebContents
[IGNORED]: https://source.chromium.org/chromium/chromium/src/+/main:components/permissions/permission_util.h?q=IGNORED
[DENIED]: https://source.chromium.org/chromium/chromium/src/+/main:components/permissions/permission_util.h?q=DENIED
[GRANTED]: https://source.chromium.org/chromium/chromium/src/+/main:components/permissions/permission_util.h?q=GRANTED
[PermissionPrompt]: https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/ui/permission_bubble/permission_prompt.h
[ContextualNotificationPermissionUiSelector]: https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/permissions/prediction_service/contextual_notification_permission_ui_selector.h
[PermissionsSecurityModelInteractiveUITest]: https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/permissions/permissions_security_model_interactive_uitest.cc
[RenderFrameHost]: https://source.chromium.org/chromium/chromium/src/+/main:content/public/browser/render_frame_host.h
[add_new_permissionmd]: https://source.chromium.org/chromium/chromium/src/+/main:components/permissions/add_new_permission.md
[PermissionDecisionAutoBlocker]: https://source.chromium.org/chromium/chromium/src/+/main:components/permissions/permission_decision_auto_blocker.cc
[componentspermissionspermission_decision_auto_blockerh]: https://source.chromium.org/chromium/chromium/src/+/main:components/permissions/permission_decision_auto_blocker.h
[permission_decision_auto_blocker_cc]: https://source.chromium.org/chromium/chromium/src/+/main:components/permissions/permission_decision_auto_blocker.cc
[permission_request_manager_cc]: https://source.chromium.org/chromium/chromium/src/+/main:components/permissions/permission_request_manager.cc
[componentspermissionspermission_requesth]: https://source.chromium.org/chromium/chromium/src/+/main:components/permissions/permission_request.h
[componentspermissionsfeaturesh]: https://source.chromium.org/chromium/chromium/src/+/main:components/permissions/features.h
[kDefaultDismissalsBeforeBlock]: https://source.chromium.org/chromium/chromium/src/+/main:components/permissions/permission_decision_auto_blocker.cc?q=kDefaultDismissalsBeforeBlock
[kDefaultIgnoresBeforeBlock]: https://source.chromium.org/chromium/chromium/src/+/main:components/permissions/permission_decision_auto_blocker.cc?q=kDefaultIgnoresBeforeBlock
[kDefaultEmbargoDays]: https://source.chromium.org/chromium/chromium/src/+/main:components/permissions/permission_decision_auto_blocker.cc?q=kDefaultEmbargoDays
[kDefaultDismissalsBeforeBlockWithQuietUi]: https://source.chromium.org/chromium/chromium/src/+/main:components/permissions/permission_decision_auto_blocker.cc?q=kDefaultDismissalsBeforeBlockWithQuietUi
[kDefaultIgnoresBeforeBlockWithQuietUi]: https://source.chromium.org/chromium/chromium/src/+/main:components/permissions/permission_decision_auto_blocker.cc?q=kDefaultIgnoresBeforeBlockWithQuietUi
[kClapperIgnoresBeforeBlock]: https://source.chromium.org/chromium/chromium/src/+/main:components/permissions/permission_decision_auto_blocker.cc?q=kClapperIgnoresBeforeBlock
[kFederatedIdentityApiDismissalsBeforeBlock]: https://source.chromium.org/chromium/chromium/src/+/main:components/permissions/permission_decision_auto_blocker.cc?q=kFederatedIdentityApiDismissalsBeforeBlock
[kFederatedIdentityApiEmbargoDurationDismiss]: https://source.chromium.org/chromium/chromium/src/+/main:components/permissions/permission_decision_auto_blocker.cc?q=kFederatedIdentityApiEmbargoDurationDismiss
[kFederatedIdentityAutoReauthnEmbargoDuration]: https://source.chromium.org/chromium/chromium/src/+/main:components/permissions/permission_decision_auto_blocker.cc?q=kFederatedIdentityAutoReauthnEmbargoDuration
