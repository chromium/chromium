# Security Model for Skills WebUI (`//chrome/browser/ui/webui/skills`)

This component implements the Browser-process `chrome://skills` management page,
the `chrome://skills/dialog` creation and editing modal dialog, main-frame
settings navigation interception, and the Mojo IPC handlers backing the Skills
WebUI on desktop platforms. Sandboxed `chrome://skills` WebUI renderer frames
send Mojo requests to read, create, edit, refine, delete, and invoke skills,
while the unsandboxed Browser process validates requests and reads or mutates
persistent state through `skills::SkillsService`,
`OptimizationGuideKeyedService`, and `Profile` user data. Its core security
invariant is that all skill mutations, AI prompt refinement executions, cookie
synchronizations, and account sign-in triggers are bound to the Browser-verified
`Profile` of the hosting `content::WebContents` and restricted to
`RenderFrameHost` instances bound to `skills::SkillsUI`.

<!-- last-reviewed: 2026-10 -->

## Scope

This policy applies to code in `//chrome/browser/ui/webui/skills/`.

It does not apply to test targets and test files in
`//chrome/browser/ui/webui/skills/` (`*_unittest.cc` and
`*_interactive_uitest.cc`).

The code below sits on the other side of boundaries described in this file, but
is outside its scope. Findings there fall under the owning directory's policy
and the global policy; this file does not exclude them.

- WebUI TypeScript/HTML frontend and `<webview>` postMessage bridge
  (`SkillsWebviewBridge`): `//chrome/browser/resources/skills/`
- Tab and browser-window UI controllers (`SkillsUiTabController`,
  `SkillsUiWindowController`): `//chrome/browser/skills/`
- Core skill storage, sync bridge, and 1P skill download service
  (`SkillsService`): `//components/skills/`
- Guest `<webview>` cookie synchronization helper (`GlicCookieSynchronizer`):
  `//chrome/browser/glic/host/`
- Remote AI model execution for prompt refinement
  (`OptimizationGuideKeyedService`): `//chrome/browser/optimization_guide/`
- Sandboxed image decoding data source (`SanitizedImageSource`):
  `//chrome/browser/ui/webui/sanitized_image/`

## Security Boundaries & Threat Model

### Process type & sandbox

All C++ implementation code in `//chrome/browser/ui/webui/skills/` runs in the
un-sandboxed **Browser process** on desktop platforms (**Windows, macOS, Linux,
and ChromeOS**; excluded on Android and iOS in `//chrome/browser/BUILD.gn` and
`RegisterChromeWebUIConfigs()`). The Mojo interface definitions in
`//chrome/browser/ui/webui/skills:mojo_bindings` (`skills.mojom`) are linked
into the Browser process and compiled into TypeScript bindings served to the
`chrome://skills` WebUI Renderer process:

| Code | Process | Sandbox | Reaches this code via |
| :--- | :--- | :--- | :--- |
| `skills_ui.*`, `skills_page_handler.*`, `skills_page_handler_v2.*`, `skills_dialog_handler.*`, `skills_dialog_view.*`, `skills_navigation_throttle.*` | Browser | None | `SkillsUIConfig` (`chrome://skills`) and `PopulateChromeWebUIFrameBinders()` on Windows, macOS, Linux, and ChromeOS; `SkillsNavigationThrottle::MaybeCreateAndAdd()` in V2 mode only (`features::kSkillsWebViewV2Enabled`) |
| `skills.mojom` (`:mojo_bindings`) | Browser and WebUI Renderer | Renderer sandbox (Renderer side); None (Browser side) | Generated C++ bindings in `//chrome/browser/ui/webui/skills/` and WebUI TypeScript bindings in `//chrome/browser/resources/skills/` |

The threat model for `skills.mojom` assumes the **most privileged** host, the
Browser process.

The component operates in one of two modes on desktop platforms:

| Mode | Implementation | Security property | Where it is used |
| :--- | :--- | :--- | :--- |
| V1 Native WebUI (default) | `SkillsPageHandler` (`skills::mojom::PageHandler`) and `SkillsDialogHandler` (`skills::mojom::DialogHandler`) via `SkillsUI::BindInterface(PageHandlerFactory)` | Enforced by `CHECK(!base::FeatureList::IsEnabled(features::kSkillsWebViewV2Enabled))` in `SkillsUI::BindInterface()`, `CreatePageHandler()`, and `CreateDialogHandler()`; serves local WebUI resources and `SanitizedImageSource` | Windows, macOS, and ChromeOS by default (`features::kSkillsEnabled` is `FEATURE_ENABLED_BY_DEFAULT`); Linux when `features::kSkillsEnabled` (`FEATURE_DISABLED_BY_DEFAULT` on Linux) is enabled |
| V2 WebView Host (migration) | `SkillsPageHandlerV2` (`skills::mojom::SkillsPageHandler`) via `SkillsUI::BindInterface(SkillsPageHandler)`, and `SkillsNavigationThrottle` (`MaybeCreateAndAdd()` returns early unless `features::kSkillsWebViewV2Enabled` is enabled) | Enforced by `CHECK(base::FeatureList::IsEnabled(features::kSkillsWebViewV2Enabled))` in `SkillsUI::BindInterface()`; isolates the embedded `<webview>` guest in an in-memory `StoragePartitionConfig` (`"glicskillspart"`) | Windows, macOS, Linux, and ChromeOS when both `features::kSkillsEnabled` and `features::kSkillsWebViewV2Enabled` (`FEATURE_DISABLED_BY_DEFAULT`) are enabled |

### Execution & caller contexts

- **Privileged `chrome://skills` WebUI frames (`RenderFrameHost`):**
  `PopulateChromeWebUIFrameBinders()` registers
  `skills::mojom::PageHandlerFactory` and `skills::mojom::SkillsPageHandler`
  exclusively for `RenderFrameHost` instances whose active `WebUIController`
  is `skills::SkillsUI`. Ordinary web pages cannot bind these interfaces.
- **Embedded `<webview>` guest frames (V2 mode):** When
  `features::kSkillsWebViewV2Enabled` is active, `chrome://skills` embeds a
  `<webview>` guest hosting the remote Skills web application (defaulting to
  `https://clients5.google.com` via `GetSkillsV2Origin()`). The guest frame
  runs in a separate sandboxed renderer process inside an isolated, in-memory
  `StoragePartition` (`partition_domain = "skills"`,
  `partition_name = "glicskillspart"`) and has no direct access to `SkillsUI`
  Mojo interfaces; it communicates with the embedding `chrome://skills` frame
  via `window.postMessage` (`SkillsWebviewBridge` in
  `//chrome/browser/resources/skills/`).
- **Main-frame navigations (V2 mode only):**
  `SkillsNavigationThrottle::MaybeCreateAndAdd()` returns early unless
  `features::kSkillsWebViewV2Enabled` is enabled. In V2 mode, it adds the
  throttle only for main-frame navigations (`NavigationHandle::IsInMainFrame()`)
  for which `IsTriggeredOnSkillsSettingsLink()` returns true. That requires
  all of the following:
  - the page transition core type is `ui::PAGE_TRANSITION_LINK`;
  - the URL `EqualsIgnoringRef()` `features::kInterceptedSkillsUrl`; and
  - the navigation has an initiator origin, and that origin is either the
    origin of `features::kInterceptedSkillsUrl` (the Skills web client) or
    a `chrome://skills` / `chrome-untrusted://skills` origin.

  `SkillsNavigationThrottle::WillStartRequest()` then cancels the
  navigation (`CANCEL_AND_IGNORE`) and posts a task that opens
  `features::kSkillsSettingsPageUrl` (`chrome://settings/ai/skills`) with
  `content::OpenURLParams::CreateBrowserInitiated()`. If the navigation is
  inside the `<webview>` guest, it opens the URL in the outer (embedder)
  `WebContents` instead of the guest.
- **Web Workers and Browser Extensions:** `skills::mojom::PageHandlerFactory`
  and `skills::mojom::SkillsPageHandler` are registered only as frame binders,
  via `RegisterWebUIControllerInterfaceBinder<..., skills::SkillsUI>` in
  `PopulateChromeWebUIFrameBinders()`
  (`//chrome/browser/chrome_browser_interface_binders_webui.cc`). No binder
  exists for Dedicated, Shared, or Service Worker or extension contexts, so
  they cannot bind these interfaces.

### Inputs

- **Untrusted:** Incoming Mojo calls and arguments from the `chrome://skills`
  WebUI Renderer process over `skills::mojom::PageHandlerFactory`,
  `skills::mojom::PageHandler`, `skills::mojom::DialogHandler`, and
  `skills::mojom::SkillsPageHandler` (including `skills::Skill` fields,
  `skill_id` strings, `prompt` strings, and `PendingEditorData`, which in V2
  mode may be forwarded from the `<webview>` guest via `postMessage`), as well
  as main-frame navigation URLs evaluated by `SkillsNavigationThrottle`.
- **Trusted (validated upstream before reaching this component):** The
  `content::WebUI` and `content::RenderFrameHost` binding association verified
  by `//content` and `RegisterWebUIControllerInterfaceBinder` before invoking
  `SkillsUI::BindInterface()`.
- **Trusted provenance (bound to the `Profile`):** The `Profile` and the
  services the Browser process resolves from it (`Profile::FromWebUI()`,
  `Profile::FromBrowserContext()`, `SkillsServiceFactory::GetForProfile()`,
  `IdentityManagerFactory::GetForProfile()`,
  `OptimizationGuideKeyedServiceFactory::GetForProfile()`, and
  `PendingEditorDataHandler` user data attached to `Profile`).
- **Untrusted content:** Skill names, prompts, and icons stored in
  `SkillsService`. They come from Chrome Sync, the 1P skills download, the
  `EnterprisePublishedSkills` policy, and earlier user edits, so treat them
  as untrusted wherever they are rendered in WebUI or sent to Glic.

### Trust relationships

- Defends against a compromised Renderer: Yes.
- **Properties covered against a compromised Renderer:**
  - *Profile and account binding:* `SkillsUI`, `SkillsPageHandler`,
    `SkillsPageHandlerV2`, and `SkillsDialogHandler` resolve the target
    `Profile`, `SkillsService`, `IdentityManager`, and
    `OptimizationGuideKeyedService` directly from the browser-side
    `content::WebUI` / `content::WebContents`, never from renderer-supplied
    account or profile identifiers.
  - *Isolated `<webview>` storage partition:*
    `SkillsPageHandlerV2::SyncCookies()` signs the profile's accounts into
    the isolated, in-memory `content::StoragePartitionConfig` created for
    host `chrome::kChromeUISkillsHost` and partition `"glicskillspart"`
    through `glic::GlicCookieSynchronizer`. It does not copy the profile's
    cookie jar; see
    [Account cookies in the `<webview>` partition](#account-cookies-in-the-webview-partition).
  - *Protocol and mode invariants:* `SkillsPageHandler::MaybeSave1PSkill()`
    validates that no concurrent save request is in flight and calls
    `receiver_.ReportBadMessage("Save call in progress")` on violation.
    `SkillsUI` enforces V1/V2 handler separation with `CHECK` assertions on
    `features::kSkillsWebViewV2Enabled`.
- **Properties not covered against a compromised `chrome://skills` WebUI
  Renderer (per
  [//docs/security/compromised-renderers.md](//docs/security/compromised-renderers.md)):**
  - A compromised `chrome://skills` WebUI renderer already runs in the
    security context of the `chrome://skills` management UI for the current
    `Profile` and can invoke any bound `PageHandler`, `DialogHandler`, or
    `SkillsPageHandler` method for that profile (reading, creating,
    modifying, or deleting skills in `SkillsService`, requesting prompt
    refinement via `RefineSkill()` / `GenerateNameAndEmoji()`, or sending a
    prompt to the side panel via `SendPrompt()`). `GetProvidedSkill()`
    resolves through `SkillsService::GetSkillById()`, so it returns any
    skill in the profile, not only enterprise-provided skills; this is
    same-profile data that the WebUI can already list.
  - In V2 mode, `window.postMessage` origin and handshake validation between
    the `<webview>` guest and `chrome://skills` (`SkillsWebviewBridge` in
    `//chrome/browser/resources/skills/`) executes in the `chrome://skills`
    WebUI renderer; a compromised `chrome://skills` renderer can bypass
    those client-side checks, though its Browser-process authority remains
    bounded by `SkillsPageHandler` for the current `Profile`.
  - Skill name, prompt, and icon length limits are enforced only in the
    WebUI frontend; see
    [Skill length limits are frontend-only](#skill-length-limits-are-frontend-only).
- **UX guard, not a security property:** In V2 mode only,
  `SkillsUI::BindInterface(mojo::PendingReceiver<skills::mojom::SkillsPageHandler>)`
  refuses to bind the handler and redirects to `chrome://skills` when a user
  navigates directly to `chrome://skills/dialog` without an attached
  `SkillsDialogDelegate` (`url.path() == "/dialog" && !delegate_ &&
  !is_internal_user`). Google-internal accounts
  (`gaia::IsGoogleInternalAccountEmail()`) bypass it, and V1
  `CreateDialogHandler()` has no equivalent. It protects dialog state, not
  against a compromised renderer, which can already bind
  `SkillsPageHandler` from `chrome://skills`.
- **The remote Skills web app can drive Glic (V2 mode):**
  `SkillsPageHandlerV2::SendPrompt()` calls
  `SkillsUiTabController::SendPrompt()`, which calls
  `GlicKeyedService::InvokeWithAutoSubmit()`, so the prompt string is
  auto-submitted to Glic on the target tab. When called from the dialog, the
  target is the active tab of the dialog's browser window, which can be any
  page. `SkillsPageHandlerV2::InvokeSkill()` passes the renderer-supplied
  skill name and icon to `SkillsUiTabController::InvokeSkill()`. In V2 mode,
  these calls can originate from the remote `<webview>` guest through
  `SkillsWebviewBridge` (`//chrome/browser/resources/skills/`), so the
  remote Skills origin can effectively drive Glic; the only check on that
  path is the frontend `postMessage` origin check (see
  [The remote Skills web app can send prompts to Glic](#the-remote-skills-web-app-can-send-prompts-to-glic)).
  Unlike `InvokeSkill()`,
  neither `SkillsPageHandlerV2::SendPrompt()` nor
  `SkillsUiTabController::SendPrompt()` checks
  `SkillsServiceFactory::IsSkillsEnabledForProfile()` (tracked in
  [crbug.com/577144150](https://crbug.com/577144150); see
  [Known Issues](#known-issues)).

### Rule of Two

C++ code in `//chrome/browser/ui/webui/skills/` runs in the unsandboxed Browser
process, but does not perform complex parsing of untrusted binary formats:

- Incoming Renderer IPC payloads are deserialized into fixed-schema Mojo
  structs, strings, and enums (`skills.mojom` and
  `//components/skills/public/skill.mojom`) by generated Mojo bindings.
- In V1 mode, external skill images are loaded through `SanitizedImageSource`
  (`AddSkillsV1Resources()`), which delegates image decoding and re-encoding
  to a sandboxed Utility process before serving the image to
  `chrome://skills`.
- Prompt refinement (`SkillsDialogHandler::RefineSkill()` and
  `SkillsDialogHandler::GenerateNameAndEmoji()`) populates typed
  `optimization_guide::proto::SkillsRequest` protobuf messages and delegates
  model execution and response unpacking
  (`optimization_guide::ParsedAnyMetadata<SkillsResponse>`) to
  `OptimizationGuideKeyedService::ExecuteModel()`.

## Security Boundaries & Explicit Non-bugs

### Security boundaries

- Enforces `SkillsUI` controller-type verification on `PageHandlerFactory` and
  `SkillsPageHandler` Mojo bindings via
  `RegisterWebUIControllerInterfaceBinder`, gated by
  `SkillsUIConfig::IsWebUIEnabled()` (`features::kSkillsEnabled`).
- Binds all skill CRUD operations, `PendingEditorDataHandler` profile data, AI
  model execution requests, and `signin_ui_util` re-authentication flows to
  the Browser-owned `Profile` of the hosting `WebContents`.
- Isolates V2 `<webview>` guest storage and synchronized cookies inside the
  in-memory `"glicskillspart"` `StoragePartitionConfig`.
- Restricts the browser-initiated open of `chrome://settings/ai/skills` by
  `SkillsNavigationThrottle` (V2 mode only) to link navigations to
  `features::kInterceptedSkillsUrl` whose initiator is the Skills web client
  origin or `chrome://skills` / `chrome-untrusted://skills`
  (`IsTriggeredOnSkillsSettingsLink()`). The initiator check is what stops
  arbitrary web pages from using a link to `kInterceptedSkillsUrl` to make
  the Browser open a `chrome://` settings page.

### Out of scope (non-bugs)

This section is **additive** to //docs/security/security-for-agents.md and
//docs/security/rules.md. Absence from this list does not imply a finding is
out of scope.

- Internal C++ preconditions: calling internal helper or lifecycle methods
  such as `SkillsUI::InitializeDialog()`,
  `SkillsDialogHandler::SaveOrUpdateSkill()`,
  `SkillsPageHandler::OnSkillUpdated()`, or
  `SkillsPageHandlerV2::OnProvidedSkillsChanged()` directly with unvalidated
  arguments from a unit test or an in-process caller. The only cross-process
  Renderer-to-Browser entry points into this component are
  `skills::mojom::PageHandlerFactory` (`CreatePageHandler()`,
  `CreateDialogHandler()`), `skills::mojom::PageHandler`
  (`OpenSkillsDialog()`, `GetInitialUserSkills()`, `GetInitial1PSkills()`,
  `Request1PSkills()`, `DeleteSkill()`, `MaybeSave1PSkill()`,
  `RecordSkillsManagementAction()`), `skills::mojom::DialogHandler`
  (`SubmitSkill()`, `DeleteSkill()`, `RefineSkill()`,
  `GenerateNameAndEmoji()`, `CloseDialog()`, `ShowEmojiPicker()`,
  `GetInitialState()`, `GetSignedInEmail()`), and
  `skills::mojom::SkillsPageHandler` (`SetPage()`, `GetProvidedSkill()`,
  `GetProvidedSkills()`, `SyncCookies()`, `ShowSaveToast()`,
  `ShowSaveAndInvokeToast()`, `ShowDeleteToast()`, `InvokeSkill()`,
  `SendPrompt()`, `CloseDialog()`, `GetPendingEditorData()`, `SignIn()`).
- Expected feature disablement: `SkillsUIConfig::IsWebUIEnabled()` returns
  `false` when `features::kSkillsEnabled` is disabled, and
  `SkillsDialogHandler::RefineSkill()` and `GenerateNameAndEmoji()` return
  `nullptr` without executing a model request when
  `OptimizationGuideKeyedService` is unavailable for the profile or the input
  prompt is empty.
- Behavior that requires local developer command-line switches, such as
  `--skills-dev` (enables `devMode` in `chrome://skills`, which acts as a
  security bypass by disabling the bridge's origin allowlist
  (`urlMatchesApiAllowedOrigin()`) and the handshake origin check) or
  `--skills-v2-origin` (overrides the `<webview>` guest origin; when set,
  `SkillsPageHandlerV2::SyncCookies()` returns without syncing any account
  cookies).

## Design Justifications & Accepted Risks

These entries give design rationale with citations. Known gaps with a tracking
bug are under [Known Issues](#known-issues).

### Account cookies in the `<webview>` partition {#account-cookies-in-the-webview-partition}

The V2 Skills web app runs in a `<webview>` guest and needs the user's Google
account session to call the Skills backend
([Skills V2 Angular App design](http://go/skills-v2-angular-app) (Google-internal)).
`SkillsPageHandlerV2::SyncCookies()` provides it through
`glic::GlicCookieSynchronizer::CopyCookiesToWebviewStoragePartition()`, which
does not copy the profile's cookie jar. It signs the profile's accounts into
the in-memory `"glicskillspart"` partition with an OAuth multilogin
(`SetAccountsInCookieForPartition`), and clears that partition's cookies
before the first sync (`kGlicClearCookiesOnFirstSync`). The residual risk is
bounded as follows:

- The cookies live only in the isolated, in-memory `<webview>` partition and
  are not shared with the profile's default storage partition.
- When `--skills-v2-origin` overrides the guest origin, `SyncCookies()`
  returns early, so an overridden origin never receives account cookies.
- The only path that copies existing Google cookies is the development path
  behind `kGlicDevelopmentSyncGoogleCookies`, limited to internal accounts.

### The remote Skills web app can send prompts to Glic {#the-remote-skills-web-app-can-send-prompts-to-glic}

In V2 mode, the Skills web app in the `<webview>` guest must be able to
invoke skills and send prompts to the Glic side panel; this is one of the
stated reasons for hosting it in a `<webview>` that can talk to
`chrome://skills`
([Skills V2 Angular App design](http://go/skills-v2-angular-app) (Google-internal), "Why
webview?"). As a result, `SkillsPageHandlerV2::SendPrompt()` and
`SkillsPageHandlerV2::InvokeSkill()` act on requests that the guest relays
through `SkillsWebviewBridge`, and `SendPrompt()` auto-submits the prompt
to Glic through `GlicKeyedService::InvokeWithAutoSubmit()`. The residual
risk is bounded as follows:

- The guest origin is the Google-served Skills web app
  (`GetSkillsV2Origin()`, default `https://clients5.google.com`). It can
  only be changed with the local `--skills-v2-origin` developer switch.
- The webview may navigate only to the primary origin and Google sign-in
  origins (`accounts.google.com` and internal authentication domains);
  bridge messages are processed only from the primary origin after the
  handshake pins `targetOrigin_`.
- The guest runs in a `<webview>` with its own in-memory storage partition,
  and extensions cannot inject into it, unlike an `<iframe>`
  ([Skills V2 Angular App design](http://go/skills-v2-angular-app) (Google-internal)).

This entry does not cover the missing
`SkillsServiceFactory::IsSkillsEnabledForProfile()` check in `SendPrompt()`,
which is tracked in [crbug.com/577144150](https://crbug.com/577144150) under
[Known Issues](#known-issues).

### Skill length limits are frontend-only {#skill-length-limits-are-frontend-only}

`AddSkillsV1Resources()` exposes `kMaxNameCharCount` (`100`) and
`kMaxPromptCharCount` (`20000`) to the WebUI frontend, which enforces them.
On the Browser side, `SkillsDialogHandler::SaveOrUpdateSkill()` only trims
ASCII whitespace and rejects empty `name` or `prompt` strings. Neither
`SaveOrUpdateSkill()`, `SkillsServiceImpl`, nor `SkillsSyncBridge` enforces
length, and `icon` is not validated. This gives a compromised
`chrome://skills` renderer no extra privilege, because it already has full
create, read, update, and delete access to its own profile's skills. If a
Browser-side check is added later, it should reject oversized input rather
than call `ReportBadMessage()`, because V2 and synced skills can legitimately
exceed the V1 limits.

## Known Issues

These security gaps have a tracking bug. Only list items that affect an actual
security property (confidentiality, integrity, privilege separation, memory
safety); exclude general functional bugs, source-code `TODO(crbug)` cleanup
tasks, or caching optimizations. A report that only restates one duplicates that
bug.

- **`SendPrompt()` does not check
  `SkillsServiceFactory::IsSkillsEnabledForProfile()`.** Unlike
  `SkillsPageHandlerV2::InvokeSkill()` and
  `SkillsUiTabController::InvokeSkill()`, neither
  `SkillsPageHandlerV2::SendPrompt()` nor
  `SkillsUiTabController::SendPrompt()` verifies
  `SkillsServiceFactory::IsSkillsEnabledForProfile()`. Although the V2 WebUI
  frontend (`SkillsWebview.init()`) renders an error page instead of loading
  the `<webview>` when `isSkillsEnabled` is false, `SkillsUI::BindInterface()`
  still binds `SkillsPageHandlerV2`, allowing a compromised `chrome://skills`
  renderer (or an in-flight `SendPrompt()` call before page reload) to
  auto-submit prompts to Glic via `GlicKeyedService::InvokeWithAutoSubmit()`
  while Skills is disabled for the profile. Tracked in
  [crbug.com/577144150](https://crbug.com/577144150).

## Reporting

Report security issues through
[Chrome VRP](https://bughunters.google.com/report/vrp). Before filing, review
[security-for-agents.md](//docs/security/security-for-agents.md).

## Further Documentation

- [Skills V2 Angular App design](http://go/skills-v2-angular-app) (Google-internal): the V2
  architecture, including why `chrome://skills` hosts the Skills web app in a
  `<webview>` rather than an `<iframe>`, and the `SkillsBridgeService`
  `postMessage` handshake.
- `//chrome/browser/resources/skills/`: the `chrome://skills` WebUI frontend.
  The V2 `<webview>` host and its `postMessage` bridge to the guest live in
  `v2/skills_webview.ts`.
- `//chrome/browser/glic/host/glic_cookie_synchronizer.h`: the account cookie
  sync into the `<webview>` storage partition used by
  `SkillsPageHandlerV2::SyncCookies()`.
- Historical security bugs in this component:
  [Chromium issue tracker](https://issues.chromium.org/issues?q=componentid:1970385+type:bug+security)
  (`componentid:1970385 type:bug security`, per `DIR_METADATA`).
