// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/apps/app_info_dialog/app_info_dialog_views.h"

#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/apps/app_info_dialog.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/scoped_tabbed_browser_displayer.h"
#include "chrome/browser/ui/views/apps/app_info_dialog/app_info_dialog_container.h"
#include "chrome/browser/ui/views/apps/app_info_dialog/app_info_footer_panel.h"
#include "chrome/browser/ui/views/apps/app_info_dialog/app_info_header_panel.h"
#include "chrome/browser/ui/views/apps/app_info_dialog/app_info_permissions_panel.h"
#include "chrome/browser/ui/views/apps/app_info_dialog/app_info_summary_panel.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/chrome_switches.h"
#include "components/app_constants/constants.h"
#include "components/constrained_window/constrained_window_views.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_handlers/app_display_info.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/border.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_delegate.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/components/arc/app/arc_app_constants.h"
#include "chrome/browser/ash/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ash/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/ui/views/apps/app_info_dialog/arc_app_info_links_panel.h"
#endif

namespace {

constexpr gfx::Size kDialogSize = gfx::Size(380, 490);

}  // namespace

bool CanPlatformShowAppInfoDialog() {
#if BUILDFLAG(IS_MAC)
  return false;
#else
  return true;
#endif
}

bool CanShowAppInfoDialog(Profile* profile, const std::string& extension_id) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  auto* system_web_app_manager = ash::SystemWebAppManager::Get(profile);
  if (system_web_app_manager &&
      system_web_app_manager->IsSystemWebApp(extension_id)) {
    return false;
  }

  const extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(profile);
  const extensions::Extension* extension =
      registry->GetInstalledExtension(extension_id);

  if (!extension) {
    return false;
  }

  // App Management only displays apps that are displayed in the launcher.
  if (!extensions::AppDisplayInfo::ShouldDisplayInAppLauncher(*extension)) {
    return false;
  }
#endif
  return CanPlatformShowAppInfoDialog();
}

void ShowAppInfoInNativeDialog(content::WebContents* web_contents,
                               Profile* profile,
                               const extensions::Extension* app,
                               base::OnceClosure close_callback) {
  views::DialogDelegate* dialog = CreateDialogContainerForView(
      std::make_unique<AppInfoDialog>(profile, app), kDialogSize,
      std::move(close_callback));
  views::Widget* dialog_widget;
  if (dialog->GetModalType() == ui::mojom::ModalType::kChild) {
    dialog_widget =
        constrained_window::ShowWebModalDialogViews(dialog, web_contents);
  } else {
    gfx::NativeWindow window = web_contents->GetTopLevelNativeWindow();
    dialog_widget =
        constrained_window::CreateBrowserModalDialogViews(dialog, window);
    dialog_widget->Show();
  }
}

base::WeakPtr<AppInfoDialog>& AppInfoDialog::GetLastDialogForTesting() {
  static base::NoDestructor<base::WeakPtr<AppInfoDialog>> last_dialog;
  return *last_dialog;
}

AppInfoDialog::AppInfoDialog(Profile* profile, const extensions::Extension* app)
    : profile_(profile), app_id_(app->id()) {
  views::BoxLayout* layout =
      SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical));

  // Make a vertically stacked view of all the panels we want to display in the
  // dialog.
  auto dialog_body_contents = std::make_unique<views::View>();
  const ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();
  dialog_body_contents->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical,
      provider->GetInsetsMetric(views::INSETS_DIALOG_SUBSECTION),
      provider->GetDistanceMetric(views::DISTANCE_UNRELATED_CONTROL_VERTICAL)));
  dialog_body_contents->AddChildView(
      std::make_unique<AppInfoSummaryPanel>(profile, app));
  dialog_body_contents->AddChildView(
      std::make_unique<AppInfoPermissionsPanel>(profile, app));

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // When Google Play Store is enabled and the Settings app is available, show
  // the "Manage supported links" link for Chrome.
  if (app->id() == app_constants::kChromeAppId &&
      arc::IsArcPlayStoreEnabledForProfile(profile)) {
    const ArcAppListPrefs* arc_app_list_prefs = ArcAppListPrefs::Get(profile);
    if (arc_app_list_prefs &&
        arc_app_list_prefs->IsRegistered(arc::kSettingsAppId)) {
      arc_app_info_links_ = dialog_body_contents->AddChildView(
          std::make_unique<ArcAppInfoLinksPanel>(profile, app));
    }
  }
#endif

  // Clip the scrollable view so that the scrollbar appears. As long as this
  // is larger than the height of the dialog, it will be resized to the dialog's
  // actual height.
  // TODO(sashab): Add ClipHeight() as a parameter-less method to
  // views::ScrollView() to mimic this behaviour.
  const int kMaxDialogHeight = 1000;
  auto dialog_body = std::make_unique<views::ScrollView>();
  dialog_body->ClipHeightTo(kMaxDialogHeight, kMaxDialogHeight);
  dialog_body->SetContents(std::move(dialog_body_contents));

  dialog_header_ =
      AddChildView(std::make_unique<AppInfoHeaderPanel>(profile, app));

  dialog_body_ = AddChildView(std::move(dialog_body));
  layout->SetFlexForView(dialog_body_, 1);

  auto dialog_footer = AppInfoFooterPanel::CreateFooterPanel(profile, app);
  if (dialog_footer) {
    dialog_footer_ = AddChildView(std::move(dialog_footer));
  }

  // Close the dialog if the app is uninstalled, unloaded, or if the profile is
  // destroyed.
  StartObservingExtensionRegistry();

  GetLastDialogForTesting() = weak_ptr_factory_.GetWeakPtr();
}

AppInfoDialog::~AppInfoDialog() {
  StopObservingExtensionRegistry();
}

void AppInfoDialog::Close() {
  GetWidget()->Close();
}

void AppInfoDialog::StartObservingExtensionRegistry() {
  DCHECK(!extension_registry_);

  extension_registry_ = extensions::ExtensionRegistry::Get(profile_);
  extension_registry_->AddObserver(this);
}

void AppInfoDialog::StopObservingExtensionRegistry() {
  if (extension_registry_) {
    extension_registry_->RemoveObserver(this);
  }
  extension_registry_ = nullptr;
}

void AppInfoDialog::OnThemeChanged() {
  views::View::OnThemeChanged();

  constexpr int kHorizontalSeparatorHeight = 1;
  const SkColor color = GetColorProvider()->GetColor(ui::kColorSeparator);
  dialog_header_->SetBorder(views::CreateSolidSidedBorder(
      gfx::Insets::TLBR(0, 0, kHorizontalSeparatorHeight, 0), color));
  if (dialog_footer_) {
    dialog_footer_->SetBorder(views::CreateSolidSidedBorder(
        gfx::Insets::TLBR(kHorizontalSeparatorHeight, 0, 0, 0), color));
  }
}

void AppInfoDialog::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension,
    extensions::UnloadedExtensionReason reason) {
  if (extension->id() != app_id_) {
    return;
  }

  Close();
}

void AppInfoDialog::OnExtensionUninstalled(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension,
    extensions::UninstallReason reason) {
  if (extension->id() != app_id_) {
    return;
  }

  Close();
}

void AppInfoDialog::OnShutdown(extensions::ExtensionRegistry* registry) {
  DCHECK_EQ(extension_registry_, registry);
  StopObservingExtensionRegistry();
  Close();
}

BEGIN_METADATA(AppInfoDialog)
END_METADATA
