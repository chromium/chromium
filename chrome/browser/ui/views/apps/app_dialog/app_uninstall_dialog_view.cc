// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/apps/app_dialog/app_uninstall_dialog_view.h"

#include <string>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/i18n/message_formatter.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
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
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/image/image_skia_operations.h"
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
 public:
  METADATA_HEADER(UninstallCheckboxView);

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
    checkbox->SetAccessibleName(label.get());
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

BEGIN_METADATA(UninstallCheckboxView, views::View)
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
  if (app_type == apps::AppType::kArc && IsArcShortcutApp(profile, app_id))
    return l10n_util::GetStringUTF16(IDS_EXTENSION_UNINSTALL_PROMPT_TITLE);
#else
  // On non-ChromeOS, only Chrome app and web app types meaningfully exist.
  DCHECK(app_type != apps::AppType::kChromeApp &&
         app_type != apps::AppType::kWeb);
#endif
  return l10n_util::GetStringFUTF16(IDS_PROMPT_APP_UNINSTALL_TITLE,
                                    base::UTF8ToUTF16(app_name));
}

}  // namespace

struct SubApp {
  explicit SubApp(std::u16string short_name)
      : short_name(std::move(short_name)) {}
  SubApp(SubApp&& sub_app) = default;
  SubApp& operator=(SubApp&& sub_app) = default;
  SubApp(const SubApp&) = delete;
  SubApp& operator=(const SubApp&) = delete;

  std::u16string short_name;
};

static views::Widget* CreateAndShowWidget(gfx::NativeWindow parent_window,
                                          AppUninstallDialogView* dialog_view) {
  views::Widget* widget = constrained_window::CreateBrowserModalDialogViews(
      dialog_view, parent_window);
  widget->Show();
  return widget;
}

// static
void apps::UninstallDialog::UiBase::Create(
    Profile* profile,
    apps::AppType app_type,
    const std::string& app_id,
    const std::string& app_name,
    gfx::ImageSkia image,
    gfx::NativeWindow parent_window,
    apps::OnDialogCreatedCallback callback,
    apps::UninstallDialog* uninstall_dialog) {
  new AppUninstallDialogView(profile, app_type, app_id, app_name, image,
                             uninstall_dialog,
                             base::BindOnce(CreateAndShowWidget, parent_window)
                                 .Then(std::move(callback)));
}

AppUninstallDialogView::AppUninstallDialogView(
    Profile* profile,
    apps::AppType app_type,
    const std::string& app_id,
    const std::string& app_name,
    gfx::ImageSkia image,
    apps::UninstallDialog* uninstall_dialog,
    UninstallDialogReadyCallback callback)
    : apps::UninstallDialog::UiBase(uninstall_dialog),
      AppDialogView(ui::ImageModel::FromImageSkia(image)),
      uninstall_dialog_ready_callback_(std::move(callback)),
      profile_(profile) {
  profile_observation_.Observe(profile);

  SetModalType(ui::MODAL_TYPE_WINDOW);
  SetTitle(GetWindowTitleForApp(profile, app_type, app_id, app_name));

  SetCloseCallback(base::BindOnce(&AppUninstallDialogView::OnDialogCancelled,
                                  base::Unretained(this)));
  SetCancelCallback(base::BindOnce(&AppUninstallDialogView::OnDialogCancelled,
                                   base::Unretained(this)));
  SetAcceptCallback(base::BindOnce(&AppUninstallDialogView::OnDialogAccepted,
                                   base::Unretained(this)));

  InitializeView(profile, app_type, app_id);

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
                                            const std::string& app_id) {
  SetButtonLabel(
      ui::DIALOG_BUTTON_OK,
      l10n_util::GetStringUTF16(IDS_EXTENSION_PROMPT_UNINSTALL_APP_BUTTON));

  ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(),
      provider->GetDistanceMetric(views::DISTANCE_RELATED_CONTROL_VERTICAL)));

  switch (app_type) {
    case apps::AppType::kUnknown:
    case apps::AppType::kBuiltIn:
    case apps::AppType::kMacOs:
    case apps::AppType::kStandaloneBrowser:
    case apps::AppType::kRemote:
    case apps::AppType::kExtension:
    case apps::AppType::kStandaloneBrowserExtension:
      NOTREACHED_NORETURN();
    case apps::AppType::kStandaloneBrowserChromeApp:
      // Do nothing special for kStandaloneBrowserChromeApp.
      break;
    case apps::AppType::kArc:
#if BUILDFLAG(IS_CHROMEOS_ASH)
      InitializeViewForArcApp(profile, app_id);
      break;
#else
      NOTREACHED_NORETURN();
#endif
    case apps::AppType::kPluginVm:
#if BUILDFLAG(IS_CHROMEOS_ASH)
      InitializeViewWithMessage(
          l10n_util::GetStringUTF16(IDS_PLUGIN_VM_UNINSTALL_PROMPT_BODY));
      break;
#else
      NOTREACHED_NORETURN();
#endif
    case apps::AppType::kBorealis:
#if BUILDFLAG(IS_CHROMEOS_ASH)
      if (app_id == borealis::kClientAppId) {
        InitializeViewWithMessage(l10n_util::GetStringUTF16(
            IDS_BOREALIS_CLIENT_UNINSTALL_CONFIRM_BODY));
      } else {
        InitializeViewWithMessage(l10n_util::GetStringUTF16(
            IDS_BOREALIS_APPLICATION_UNINSTALL_CONFIRM_BODY));
      }
      break;
#else
      NOTREACHED_NORETURN();
#endif
    case apps::AppType::kCrostini:
#if BUILDFLAG(IS_CHROMEOS_ASH)
      InitializeViewWithMessage(l10n_util::GetStringUTF16(
          IDS_CROSTINI_APPLICATION_UNINSTALL_CONFIRM_BODY));
      break;
#else
      NOTREACHED_NORETURN();
#endif
    case apps::AppType::kBruschetta:
#if BUILDFLAG(IS_CHROMEOS_ASH)
      // TODO(b/247636749): Implement Bruschetta uninstall.
      break;
#else
      NOTREACHED_NORETURN();
#endif

    case apps::AppType::kWeb:
    case apps::AppType::kSystemWeb:
#if BUILDFLAG(IS_CHROMEOS)
      async_ = true;
      CheckForSubAppsThenInitializeViewForWebApp(app_id);
      return;
#else
      InitializeViewForWebApp(app_id, /*sub_apps=*/{});
      break;
#endif
    case apps::AppType::kChromeApp:
      InitializeViewForExtension(profile, app_id);
      break;
  }

  std::move(uninstall_dialog_ready_callback_).Run(this);
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
    if (!domain.empty())
      domain[0] = base::ToUpperASCII(domain[0]);

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

#if BUILDFLAG(IS_CHROMEOS)
void AppUninstallDialogView::InitializeSubAppList(
    const std::string& short_app_name,
    const std::vector<SubApp>& sub_apps) {
  ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();
  std::u16string description =
      base::i18n::MessageFormatter::FormatWithNamedArgs(
          l10n_util::GetStringUTF16(
              IDS_APP_UNINSTALL_PROMPT_ADDITIONAL_UNINSTALLS_MESSAGE),
          /*name0=*/"NUM_SUB_APPS", static_cast<int>(sub_apps.size()),
          /*name1=*/"APP_NAME", base::ASCIIToUTF16(short_app_name));

  auto* description_label =
      AddChildView(std::make_unique<views::Label>(description));
  description_label->SetMultiLine(/*multi_line=*/true);
  description_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);

  auto sub_apps_container = std::make_unique<views::BoxLayoutView>();
  sub_apps_container->SetOrientation(views::BoxLayout::Orientation::kVertical);
  sub_apps_container->SetBetweenChildSpacing(
      provider->GetDistanceMetric(DISTANCE_CONTROL_LIST_VERTICAL));
  sub_apps_container->SetInsideBorderInsets(gfx::Insets::TLBR(
      0, provider->GetDistanceMetric(DISTANCE_UNRELATED_CONTROL_HORIZONTAL), 0,
      0));

  for (const SubApp& sub_app : sub_apps) {
    auto box = std::make_unique<views::BoxLayoutView>();
    box->SetOrientation(views::BoxLayout::Orientation::kHorizontal);
    auto* sub_app_label =
        box->AddChildView(std::make_unique<views::Label>(sub_app.short_name));

    sub_app_label->SetGroup(static_cast<int>(DialogViewID::SUB_APP_LABEL));

    sub_app_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    sub_app_label->SetMultiLine(true);
    sub_apps_container->AddChildView(std::move(box));
  }

  std::unique_ptr<views::ScrollView> scroll_view =
      std::make_unique<views::ScrollView>();
  scroll_view->SetContents(std::move(sub_apps_container));
  scroll_view->SetHorizontalScrollBarMode(
      views::ScrollView::ScrollBarMode::kDisabled);

  scroll_view->ClipHeightTo(
      0, provider->GetDistanceMetric(
             views::DISTANCE_DIALOG_SCROLLABLE_AREA_MAX_HEIGHT));
  AddChildView(std::move(scroll_view));
  AddChildView(std::make_unique<views::Separator>());
}

void AppUninstallDialogView::LoadSubAppIds(const std::string& parent_app_id,
                                           GetSubAppsCallback callback) {
  auto* provider = web_app::WebAppProvider::GetForWebApps(profile_);
  if (provider) {
    provider->scheduler().ScheduleCallbackWithLock<web_app::AppLock>(
        "AppUninstallDialogView::LoadSubAppIds",
        std::make_unique<web_app::AppLockDescription>(parent_app_id),
        base::BindOnce(
            [](const std::string& parent_app_id, web_app::AppLock& lock) {
              return lock.registrar().GetAllSubAppIds(parent_app_id);
            },
            parent_app_id)
            .Then(base::BindOnce(&AppUninstallDialogView::GetSubAppsInfo,
                                 weak_ptr_factory_.GetWeakPtr(),
                                 std::move(callback))));
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
    std::move(callback).Run({});
    return;
  }

  web_app_provider_bridge->GetSubAppIds(
      parent_app_id,
      base::BindOnce(&AppUninstallDialogView::GetSubAppsInfo,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
#endif
}

void AppUninstallDialogView::GetSubAppsInfo(
    GetSubAppsCallback callback,
    const std::vector<std::string>& sub_app_ids) {
  std::vector<SubApp> sub_apps;
  for (const std::string& sub_app_id : sub_app_ids) {
    apps::AppServiceProxyFactory::GetForProfile(profile_)
        ->AppRegistryCache()
        .ForOneApp(sub_app_id, [&sub_apps](const apps::AppUpdate& update) {
          sub_apps.emplace_back(base::UTF8ToUTF16(update.ShortName()));
        });
  }
  std::move(callback).Run(std::move(sub_apps));
}

void AppUninstallDialogView::CheckForSubAppsThenInitializeViewForWebApp(
    const std::string& app_id) {
  LoadSubAppIds(app_id,
                base::BindOnce(&AppUninstallDialogView::InitializeViewForWebApp,
                               weak_ptr_factory_.GetWeakPtr(), app_id));
}

#endif

void AppUninstallDialogView::InitializeViewForWebApp(
    const std::string& app_id,
    std::vector<SubApp> sub_apps) {
  // For web apps, publisher id is the start url.
  GURL app_start_url;
  std::string short_app_name;
  apps::AppServiceProxyFactory::GetForProfile(profile_)
      ->AppRegistryCache()
      .ForOneApp(app_id, [&app_start_url,
                          &short_app_name](const apps::AppUpdate& update) {
        app_start_url = GURL(update.PublisherId());
        short_app_name = update.ShortName();
      });
  DCHECK(app_start_url.is_valid());

#if BUILDFLAG(IS_CHROMEOS)
  if (!sub_apps.empty()) {
    InitializeSubAppList(short_app_name, sub_apps);
  }
#endif

  // Isolated Web Apps will always have their data cleared as part of
  // uninstallation.
  if (!app_start_url.SchemeIs(chrome::kIsolatedAppScheme)) {
    InitializeCheckbox(app_start_url);
  }

  if (async_) {
    std::move(uninstall_dialog_ready_callback_).Run(this);
  }
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
void AppUninstallDialogView::InitializeViewForArcApp(
    Profile* profile,
    const std::string& app_id) {
  if (IsArcShortcutApp(profile, app_id)) {
    SetButtonLabel(
        ui::DIALOG_BUTTON_OK,
        l10n_util::GetStringUTF16(IDS_EXTENSION_PROMPT_UNINSTALL_BUTTON));
  } else {
    InitializeViewWithMessage(l10n_util::GetStringUTF16(
        IDS_ARC_APP_UNINSTALL_PROMPT_DATA_REMOVAL_WARNING));
  }
}

void AppUninstallDialogView::InitializeViewWithMessage(
    const std::u16string& message) {
  auto* label = AddChildView(std::make_unique<views::Label>(message));
  label->SetMultiLine(true);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label->SetAllowCharacterBreak(true);
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
