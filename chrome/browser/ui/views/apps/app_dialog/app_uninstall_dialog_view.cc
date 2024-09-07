// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/apps/app_dialog/app_uninstall_dialog_view.h"

#include <string>
#include <vector>

#include "base/barrier_callback.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/i18n/message_formatter.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/types/cxx23_to_underlying.h"
#include "base/values.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/extensions/extension_management.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/web_applications/locks/app_lock.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/extensions/manifest_handlers/app_launch_info.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/google/core/common/google_util.h"
#include "components/strings/grit/components_strings.h"
#include "components/url_formatter/elide_url.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/constants.h"
#include "extensions/common/manifest_url_handlers.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/separator.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/table_layout.h"
#include "ui/views/view_class_properties.h"

#if defined(USE_AURA)
#include "ui/aura/window.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ash/borealis/borealis_util.h"
#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/crosapi/web_app_service_ash.h"
#include "chromeos/crosapi/mojom/web_app_service.mojom.h"
#endif

namespace {

AppUninstallDialogView* g_app_uninstall_dialog_view = nullptr;

class UninstallCheckboxView : public views::View,
                              public views::ViewTargeterDelegate {
  METADATA_HEADER(UninstallCheckboxView, views::View)

 public:
  class CheckboxTargeter : public views::ViewTargeterDelegate {
   public:
    CheckboxTargeter() = default;
    ~CheckboxTargeter() override = default;

    // views::ViewTargeterDelegate:
    bool DoesIntersectRect(const views::View* target,
                           const gfx::Rect& rect) const override {
      return true;
    }
  };

  explicit UninstallCheckboxView(std::unique_ptr<views::StyledLabel> label) {
    SetEventTargeter(std::make_unique<views::ViewTargeter>(this));

    views::TableLayout* layout =
        SetLayoutManager(std::make_unique<views::TableLayout>());
    layout
        ->AddColumn(views::LayoutAlignment::kStretch,
                    views::LayoutAlignment::kStretch,
                    views::TableLayout::kFixedSize,
                    views::TableLayout::ColumnSize::kUsePreferred, 0, 0)
        .AddPaddingColumn(views::TableLayout::kFixedSize,
                          ChromeLayoutProvider::Get()->GetDistanceMetric(
                              views::DISTANCE_RELATED_LABEL_HORIZONTAL))
        .AddColumn(views::LayoutAlignment::kStretch,
                   views::LayoutAlignment::kStretch, 1.0f,
                   views::TableLayout::ColumnSize::kFixed, 0, 0)
        .AddRows(1, views::TableLayout::kFixedSize);

    auto checkbox = std::make_unique<views::Checkbox>();
    checkbox->GetViewAccessibility().SetName(*label.get());
    checkbox->SetEventTargeter(std::make_unique<views::ViewTargeter>(
        std::make_unique<CheckboxTargeter>()));
    checkbox_ = AddChildView(std::move(checkbox));
    AddChildView(std::move(label));
  }
  ~UninstallCheckboxView() override = default;

  // views::ViewTargeterDelegate:
  View* TargetForRect(View* root, const gfx::Rect& rect) override {
    views::View* target =
        views::ViewTargeterDelegate::TargetForRect(root, rect);
    if (target->parent() == this || target->parent() == checkbox_) {
      return checkbox_;
    }
    return target;
  }

  views::Checkbox* checkbox() { return checkbox_; }

 private:
  raw_ptr<views::Checkbox> checkbox_;
};

BEGIN_METADATA(UninstallCheckboxView)
END_METADATA

#if BUILDFLAG(IS_CHROMEOS_ASH)
bool IsArcShortcutApp(Profile* profile, const std::string& app_id) {
  ArcAppListPrefs* arc_prefs = ArcAppListPrefs::Get(profile);
  DCHECK(arc_prefs);

  std::unique_ptr<ArcAppListPrefs::AppInfo> app_info =
      arc_prefs->GetApp(app_id);
  DCHECK(app_info);
  return app_info->shortcut;
}
#endif

std::u16string GetWindowTitleForApp(Profile* profile,
                                    apps::AppType app_type,
                                    const std::string& app_id,
                                    const std::string& app_name) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // On ChromeOS, all app types exist, but Arc shortcut apps get the regular
  // extension uninstall title.
  if (app_type == apps::AppType::kArc && IsArcShortcutApp(profile, app_id)) {
    return l10n_util::GetStringUTF16(IDS_EXTENSION_UNINSTALL_PROMPT_TITLE);
  }
#else
  // On non-ChromeOS, only Chrome app and web app types meaningfully exist.
  DCHECK(app_type != apps::AppType::kChromeApp &&
         app_type != apps::AppType::kWeb);
#endif
  return l10n_util::GetStringFUTF16(IDS_PROMPT_APP_UNINSTALL_TITLE,
                                    base::UTF8ToUTF16(app_name));
}

void ResizeWidgetToContents(views::Widget* widget) {
  CHECK(widget);
  gfx::Rect bounds = widget->GetWindowBoundsInScreen();
  bounds.set_size(widget->GetRootView()->GetPreferredSize());
  widget->SetBounds(bounds);
}

}  // namespace

struct SubApp {
  explicit SubApp(std::u16string app_name, apps::IconValuePtr icon)
      : app_name(std::move(app_name)), icon(std::move(icon)) {}
  SubApp(SubApp&& sub_app) = default;
  SubApp& operator=(SubApp&& sub_app) = default;
  SubApp(const SubApp&) = delete;
  SubApp& operator=(const SubApp&) = delete;

  std::u16string app_name;
  apps::IconValuePtr icon;
};

// static
views::Widget* apps::UninstallDialog::UiBase::Create(
    Profile* profile,
    apps::AppType app_type,
    const std::string& app_id,
    const std::string& app_name,
    gfx::ImageSkia image,
    gfx::NativeWindow parent_window,
    apps::UninstallDialog* uninstall_dialog) {
  views::Widget* widget = constrained_window::CreateBrowserModalDialogViews(
      new AppUninstallDialogView(profile, app_type, app_id, app_name, image,
                                 uninstall_dialog),
      parent_window);
  widget->Show();
  return widget;
}

AppUninstallDialogView::AppUninstallDialogView(
    Profile* profile,
    apps::AppType app_type,
    const std::string& app_id,
    const std::string& app_name,
    gfx::ImageSkia image,
    apps::UninstallDialog* uninstall_dialog)
    : apps::UninstallDialog::UiBase(uninstall_dialog),
      AppDialogView(ui::ImageModel::FromImageSkia(image)),
      profile_(profile) {
  profile_observation_.Observe(profile);

  SetModalType(ui::mojom::ModalType::kWindow);

  SetCloseCallback(base::BindOnce(&AppUninstallDialogView::OnDialogCancelled,
                                  base::Unretained(this)));
  SetCancelCallback(base::BindOnce(&AppUninstallDialogView::OnDialogCancelled,
                                   base::Unretained(this)));
  SetAcceptCallback(base::BindOnce(&AppUninstallDialogView::OnDialogAccepted,
                                   base::Unretained(this)));

  InitializeView(profile, app_type, app_id, app_name);

  g_app_uninstall_dialog_view = this;
}

AppUninstallDialogView::~AppUninstallDialogView() {
  g_app_uninstall_dialog_view = nullptr;
}

// static
AppUninstallDialogView* AppUninstallDialogView::GetActiveViewForTesting() {
  return g_app_uninstall_dialog_view;
}

void AppUninstallDialogView::OnProfileWillBeDestroyed(Profile* profile) {
  weak_ptr_factory_.InvalidateWeakPtrs();
}

void AppUninstallDialogView::InitializeView(Profile* profile,
                                            apps::AppType app_type,
                                            const std::string& app_id,
                                            const std::string& app_name) {
  SetButtonLabel(
      ui::mojom::DialogButton::kOk,
      l10n_util::GetStringUTF16(IDS_EXTENSION_PROMPT_UNINSTALL_APP_BUTTON));

  ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(),
      provider->GetDistanceMetric(views::DISTANCE_RELATED_CONTROL_VERTICAL)));

  AddTitle(GetWindowTitleForApp(profile, app_type, app_id, app_name));

  switch (app_type) {
    case apps::AppType::kUnknown:
    case apps::AppType::kBuiltIn:
    case apps::AppType::kStandaloneBrowser:
    case apps::AppType::kRemote:
    case apps::AppType::kExtension:
    case apps::AppType::kStandaloneBrowserExtension:
      NOTREACHED();
    case apps::AppType::kStandaloneBrowserChromeApp:
      // Do nothing special for kStandaloneBrowserChromeApp.
      break;
    case apps::AppType::kArc:
#if BUILDFLAG(IS_CHROMEOS_ASH)
      InitializeViewForArcApp(profile, app_id);
      break;
#else
      NOTREACHED();
#endif
    case apps::AppType::kPluginVm:
#if BUILDFLAG(IS_CHROMEOS_ASH)
      AddSubtitle(
          l10n_util::GetStringUTF16(IDS_PLUGIN_VM_UNINSTALL_PROMPT_BODY));
      break;
#else
      NOTREACHED();
#endif
    case apps::AppType::kBorealis:
#if BUILDFLAG(IS_CHROMEOS_ASH)
      if (app_id == borealis::kClientAppId) {
        AddSubtitle(l10n_util::GetStringUTF16(
            IDS_BOREALIS_CLIENT_UNINSTALL_CONFIRM_BODY));
      } else {
        AddSubtitle(l10n_util::GetStringUTF16(
            IDS_BOREALIS_APPLICATION_UNINSTALL_CONFIRM_BODY));
      }
      break;
#else
      NOTREACHED();
#endif
    case apps::AppType::kCrostini:
#if BUILDFLAG(IS_CHROMEOS_ASH)
      AddSubtitle(l10n_util::GetStringUTF16(
          IDS_CROSTINI_APPLICATION_UNINSTALL_CONFIRM_BODY));
      break;
#else
      NOTREACHED();
#endif
    case apps::AppType::kBruschetta:
#if BUILDFLAG(IS_CHROMEOS_ASH)
      // TODO(b/247636749): Implement Bruschetta uninstall.
      break;
#else
      NOTREACHED();
#endif

    case apps::AppType::kWeb:
    case apps::AppType::kSystemWeb:
      InitializeViewForWebApp(app_id);
      break;
    case apps::AppType::kChromeApp:
      InitializeViewForExtension(profile, app_id);
      break;
  }
}

void AppUninstallDialogView::InitializeCheckbox(const GURL& app_start_url) {
  std::vector<std::u16string> replacements;
  replacements.push_back(url_formatter::FormatUrlForSecurityDisplay(
      app_start_url, url_formatter::SchemeDisplay::OMIT_CRYPTOGRAPHIC));

  const bool is_google = google_util::IsGoogleDomainUrl(
      app_start_url, google_util::ALLOW_SUBDOMAIN,
      google_util::ALLOW_NON_STANDARD_PORTS);
  if (!is_google) {
    auto domain = net::registry_controlled_domains::GetDomainAndRegistry(
        app_start_url,
        net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
    if (!domain.empty()) {
      domain[0] = base::ToUpperASCII(domain[0]);
    }

    replacements.push_back(base::ASCIIToUTF16(domain));
  }

  std::u16string learn_more_text =
      l10n_util::GetStringUTF16(IDS_APP_UNINSTALL_PROMPT_LEARN_MORE);
  replacements.push_back(learn_more_text);

  auto checkbox_label = std::make_unique<views::StyledLabel>();
  std::vector<size_t> offsets;
  checkbox_label->SetText(l10n_util::GetStringFUTF16(
      is_google ? IDS_APP_UNINSTALL_PROMPT_REMOVE_DATA_CHECKBOX_FOR_GOOGLE
                : IDS_APP_UNINSTALL_PROMPT_REMOVE_DATA_CHECKBOX_FOR_NON_GOOGLE,
      replacements, &offsets));
  DCHECK_EQ(replacements.size(), offsets.size());
  const size_t offset = offsets.back();

  checkbox_label->AddStyleRange(
      gfx::Range(offset, offset + learn_more_text.length()),
      views::StyledLabel::RangeStyleInfo::CreateForLink(base::BindRepeating(
          [](Profile* profile) {
            NavigateParams params(
                profile,
                GURL("https://support.google.com/chromebook/?p=uninstallpwa"),
                ui::PAGE_TRANSITION_LINK);
            Navigate(&params);
          },
          profile_)));
  views::StyledLabel::RangeStyleInfo checkbox_style;
  checkbox_style.text_style = views::style::STYLE_PRIMARY;
  gfx::Range before_link_range(0, offset);
  checkbox_label->AddStyleRange(before_link_range, checkbox_style);

  // Shift the text down to align with the checkbox.
  checkbox_label->SetBorder(
      views::CreateEmptyBorder(gfx::Insets::TLBR(3, 0, 0, 0)));

  // Create a view to hold the checkbox and the text.
  auto checkbox_view =
      std::make_unique<UninstallCheckboxView>(std::move(checkbox_label));
  clear_site_data_checkbox_ = checkbox_view->checkbox();
  AddChildView(std::move(checkbox_view));
}

void AppUninstallDialogView::InitializeViewForExtension(
    Profile* profile,
    const std::string& app_id) {
  const extensions::Extension* extension =
      extensions::ExtensionRegistry::Get(profile)->GetInstalledExtension(
          app_id);
  DCHECK(extension);

  extensions::ExtensionManagement* extension_management =
      extensions::ExtensionManagementFactory::GetForBrowserContext(profile);
  if (extension_management->UpdatesFromWebstore(*extension)) {
    auto report_abuse_checkbox = std::make_unique<views::Checkbox>(
        l10n_util::GetStringUTF16(IDS_EXTENSION_PROMPT_UNINSTALL_REPORT_ABUSE));
    report_abuse_checkbox->SetMultiLine(true);
    report_abuse_checkbox_ = AddChildView(std::move(report_abuse_checkbox));
  }
}

void AppUninstallDialogView::InitializeSubAppList(
    const std::string& app_name,
    const std::vector<SubApp>& sub_apps) {
  ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();
  std::u16string description =
      base::i18n::MessageFormatter::FormatWithNamedArgs(
          l10n_util::GetStringUTF16(
              IDS_APP_UNINSTALL_PROMPT_ADDITIONAL_UNINSTALLS_MESSAGE),
          /*name0=*/"NUM_SUB_APPS", static_cast<int>(sub_apps.size()),
          /*name1=*/"APP_NAME", base::UTF8ToUTF16(app_name));

  sub_apps_description_->SetText(description);
  sub_apps_description_->SetMultiLine(/*multi_line=*/true);
  sub_apps_description_->SetHorizontalAlignment(gfx::ALIGN_LEFT);

  auto sub_apps_container = std::make_unique<views::BoxLayoutView>();
  sub_apps_container->SetOrientation(views::BoxLayout::Orientation::kVertical);
  sub_apps_container->SetBetweenChildSpacing(
      provider->GetDistanceMetric(DISTANCE_CONTROL_LIST_VERTICAL));
  sub_apps_container->SetInsideBorderInsets(gfx::Insets::TLBR(
      0,
      provider->GetDistanceMetric(views::DISTANCE_UNRELATED_CONTROL_HORIZONTAL),
      0, 0));

  for (const SubApp& sub_app : sub_apps) {
    auto box = std::make_unique<views::BoxLayoutView>();
    box->SetOrientation(views::BoxLayout::Orientation::kHorizontal);
    auto* sub_app_label =
        box->AddChildView(std::make_unique<views::Label>(sub_app.app_name));

    sub_app_label->SetGroup(base::to_underlying(DialogViewID::SUB_APP_LABEL));

    sub_app_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    sub_app_label->SetMultiLine(true);

    auto* sub_app_icon =
        box->AddChildView(std::make_unique<views::ImageView>());
    sub_app_icon->SetImage(
        ui::ImageModel::FromImageSkia(sub_app.icon->uncompressed));
    sub_app_icon->SetGroup(base::to_underlying(DialogViewID::SUB_APP_ICON));

    box->SetBetweenChildSpacing(
        provider->GetDistanceMetric(views::DISTANCE_RELATED_LABEL_HORIZONTAL));

    sub_apps_container->AddChildView(std::move(box));
  }

  sub_apps_scroll_view_->SetContents(std::move(sub_apps_container));
  sub_apps_scroll_view_->SetHorizontalScrollBarMode(
      views::ScrollView::ScrollBarMode::kDisabled);

  sub_apps_scroll_view_->ClipHeightTo(
      0, provider->GetDistanceMetric(
             views::DISTANCE_DIALOG_SCROLLABLE_AREA_MAX_HEIGHT));
  AddChildView(std::make_unique<views::Separator>());

  sub_apps_scroll_view_->SetVisible(!sub_apps.empty());
  sub_apps_description_->SetVisible(!sub_apps.empty());
  ResizeWidgetToContents(sub_apps_scroll_view_->GetWidget());
}

void AppUninstallDialogView::LoadSubAppIds(const std::string& short_app_name,
                                           const std::string& parent_app_id) {
  auto* provider = web_app::WebAppProvider::GetForWebApps(profile_);
  if (provider) {
    provider->scheduler().ScheduleCallbackWithResult(
        "AppUninstallDialogView::LoadSubAppIds",
        web_app::AppLockDescription(parent_app_id),
        base::BindOnce(
            [](const std::string& parent_app_id, web_app::AppLock& lock,
               base::Value::Dict& debug_value) {
              return lock.registrar().GetAllSubAppIds(parent_app_id);
            },
            parent_app_id),
        base::BindOnce(&AppUninstallDialogView::GetSubAppsInfo,
                       weak_ptr_factory_.GetWeakPtr(), short_app_name),
        /*arg_for_shutdown=*/std::vector<std::string>());
    return;
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  crosapi::mojom::WebAppProviderBridge* web_app_provider_bridge =
      crosapi::CrosapiManager::Get()
          ->crosapi_ash()
          ->web_app_service_ash()
          ->GetWebAppProviderBridge();

  if (!web_app_provider_bridge) {
    LOG(ERROR) << "Could not find WebAppProviderBridge.";
    return;
  }

  web_app_provider_bridge->GetSubAppIds(
      parent_app_id,
      base::BindOnce(&AppUninstallDialogView::GetSubAppsInfo,
                     weak_ptr_factory_.GetWeakPtr(), short_app_name));
#endif
}

void AppUninstallDialogView::GetSubAppsInfo(
    const std::string& short_app_name,
    const std::vector<std::string>& sub_app_ids) {
  apps::AppServiceProxy* app_service_proxy =
      apps::AppServiceProxyFactory::GetForProfile(profile_);

  const auto sub_app_info_collector = base::BarrierCallback<SubApp>(
      sub_app_ids.size(),
      base::BindOnce(&AppUninstallDialogView::InitializeSubAppList,
                     weak_ptr_factory_.GetWeakPtr(), short_app_name));

  for (const std::string& sub_app_id : sub_app_ids) {
    std::u16string sub_app_name;
    app_service_proxy->AppRegistryCache().ForOneApp(
        sub_app_id, [&sub_app_name](const apps::AppUpdate& update) {
          sub_app_name = base::UTF8ToUTF16(update.Name());
        });

    app_service_proxy->LoadIcon(
        sub_app_id, apps::IconType::kUncompressed, web_app::kWebAppIconSmall,
        /*allow_placeholder_icon=*/false,
        base::BindOnce(
            [](std::u16string sub_app_name, apps::IconValuePtr icon_value_ptr) {
              return SubApp(sub_app_name, std::move(icon_value_ptr));
            },
            sub_app_name)
            .Then(sub_app_info_collector));
  }
}

void AppUninstallDialogView::InitializeViewForWebApp(
    const std::string& app_id) {
  // For web apps, publisher id is the start url.
  GURL app_start_url;
  std::string app_name;
  apps::AppServiceProxyFactory::GetForProfile(profile_)
      ->AppRegistryCache()
      .ForOneApp(app_id,
                 [&app_start_url, &app_name](const apps::AppUpdate& update) {
                   app_start_url = GURL(update.PublisherId());
                   app_name = update.Name();
                 });
  DCHECK(app_start_url.is_valid());

  // Sub apps are currently only supported for Isolated Web Apps.
  if (app_start_url.SchemeIs(chrome::kIsolatedAppScheme)) {
    sub_apps_description_ = AddChildView(std::make_unique<views::Label>());
    sub_apps_scroll_view_ = AddChildView(std::make_unique<views::ScrollView>());
    sub_apps_description_->SetVisible(false);
    sub_apps_scroll_view_->SetVisible(false);
    LoadSubAppIds(app_name, app_id);
  } else {
    // Isolated Web Apps will always have their data cleared as part of
    // uninstallation.
    InitializeCheckbox(app_start_url);
  }
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
void AppUninstallDialogView::InitializeViewForArcApp(
    Profile* profile,
    const std::string& app_id) {
  if (IsArcShortcutApp(profile, app_id)) {
    SetButtonLabel(
        ui::mojom::DialogButton::kOk,
        l10n_util::GetStringUTF16(IDS_EXTENSION_PROMPT_UNINSTALL_BUTTON));
  } else {
    AddSubtitle(l10n_util::GetStringUTF16(
        IDS_ARC_APP_UNINSTALL_PROMPT_DATA_REMOVAL_WARNING));
  }
}

#endif

void AppUninstallDialogView::OnDialogCancelled() {
  uninstall_dialog()->OnDialogClosed(false /* uninstall */,
                                     false /* clear_site_data */,
                                     false /* report_abuse */);
}

void AppUninstallDialogView::OnDialogAccepted() {
  const bool clear_site_data =
      clear_site_data_checkbox_ && clear_site_data_checkbox_->GetChecked();
  const bool report_abuse_checkbox =
      report_abuse_checkbox_ && report_abuse_checkbox_->GetChecked();
  uninstall_dialog()->OnDialogClosed(true /* uninstall */, clear_site_data,
                                     report_abuse_checkbox);
}

void AppUninstallDialogView::OnWidgetInitialized() {
  AppDialogView::OnWidgetInitialized();
  GetOkButton()->SetProperty(views::kElementIdentifierKey,
                             kAppUninstallDialogOkButtonId);
}

BEGIN_METADATA(AppUninstallDialogView)
END_METADATA
