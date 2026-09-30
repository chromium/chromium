# Sub Apps API (`chrome/browser/web_applications/sub_apps`)

This directory implements the browser-side `blink::mojom::SubAppsService` for
the [Multi-App (Sub Apps) Web API](https://github.com/WICG/sub-apps), enabling
Isolated Web Apps (IWAs) to install, list, and remove sub-applications within
their own origin.

## Companion Documentation

- **Desktop Web Applications Guidelines:**
  [`//chrome/browser/web_applications/AGENTS.md`](/chrome/browser/web_applications/AGENTS.md)
- **Sub Apps Install Dialog Views UI:**
  [`//chrome/browser/ui/views/web_apps/sub_apps`](/chrome/browser/ui/views/web_apps/sub_apps/README.md)
- **Browser IWA Engine:**
  [`//chrome/browser/web_applications/isolated_web_apps`](/chrome/browser/web_applications/isolated_web_apps/README.md)

## Architecture & Security Model

- `SubAppsServiceImpl` (`sub_apps_service_impl.{h,cc}`):
  `content::DocumentService<blink::mojom::SubAppsService>` bound per
  `RenderFrameHost`:
  - **Security Validation:** `CreateIfAllowed()` verifies that the frame is a
    primary main frame (`IsInPrimaryMainFrame()`), meets Isolated Context
    requirements (`content::HasIsolatedContextCapability()`), and permits
    `PermissionsPolicyFeature::kSubApps`. Each operation also verifies that the
    calling frame belongs to an installed non-sub-app
    (`!WebAppFilter::IsIsolatedSubApp()`).
  - **`Add()`:** Fetches and validates `WebAppInstallInfo` in parallel for each
    candidate sub-app via
    `WebAppCommandScheduler::FetchInstallInfoFromInstallUrl()`
    (`FetchInstallInfoFromInstallUrlCommand`), prompts the user for consent via
    `WebAppUiManager::ShowSubAppsInstallDialog()` (unless bypassed by
    `ContentSettingsType::SUB_APPS_WITHOUT_PROMPTS`), and installs approved
    sub-apps via `WebAppCommandScheduler::InstallFromInfoWithParams()`
    (`InstallFromInfoCommand`) with `webapps::WebappInstallSource::SUB_APP`.
  - **`List()` & `Remove()`:** Queries `WebAppRegistrar::GetAllSubAppIds()` and
    schedules `WebAppCommandScheduler::RemoveInstallManagementMaybeUninstall()`
    with `WebAppManagement::Type::kSubApp` (displaying a system notification if
    any sub-apps were uninstalled).
