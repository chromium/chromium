# Sub Apps Install Dialog Views UI (`chrome/browser/ui/views/web_apps/sub_apps`)

This directory implements the user confirmation dialog displayed when an
Isolated Web App (IWA) requests installation of one or more sub-applications via
`navigator.subApps.add()`.

## Companion Documentation

- **Sub Apps Browser Service:**
  [`//chrome/browser/web_applications/sub_apps`](/chrome/browser/web_applications/sub_apps/README.md)
- **Desktop Web Apps Views Guidelines:**
  [`//chrome/browser/ui/views/web_apps/AGENTS.md`](/chrome/browser/ui/views/web_apps/AGENTS.md)

## Key Components

- `sub_apps_install_dialog.cc` (`ShowSubAppsInstallDialog` & `SubAppsListView`):
  Builds the modal `ui::DialogModel` (hosted in a
  `views::BubbleDialogModelHost`) requested via
  `WebAppUiManager::ShowSubAppsInstallDialog()`, rendering the scrollable list
  of candidate sub-apps (icons and titles), warning about shared permissions
  with the parent app, and linking to the App Management settings page.
- `SubAppsInstallDialogController`
  (`sub_apps_install_dialog_controller.{h,cc}`): `ui::DialogModelDelegate`
  tracking dialog widget view IDs (`SubAppsInstallDialogViewID`) and test
  override actions (`DialogActionForTesting`) for the sub-apps installation
  prompt.
- `sub_apps_install_dialog_controller_browsertest.cc`: Browser tests verifying
  dialog construction, sub-app list rendering, settings link handling, and
  accept/cancel callbacks.
