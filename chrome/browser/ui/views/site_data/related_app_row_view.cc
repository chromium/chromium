// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/site_data/related_app_row_view.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/views/accessibility/non_accessible_image_view.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/web_applications/web_app_icon_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/label.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/layout/table_layout.h"
#include "ui/views/vector_icons.h"
#include "ui/views/view_class_properties.h"

DEFINE_CUSTOM_ELEMENT_EVENT_TYPE(kRelatedAppRowMenuItemClicked);

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(RelatedAppRowView, kLinkToAppSettings);

namespace {

// TODO(crbug.com/362922563): replace this with an appropriate uninstall icon.
constexpr int kLinkIconSize = 16;

std::unique_ptr<views::TableLayout> SetupTableLayout() {
  const auto dialog_insets = ChromeLayoutProvider::Get()->GetInsetsMetric(
      views::InsetsMetric::INSETS_DIALOG);
  const int icon_label_spacing = ChromeLayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_RELATED_LABEL_HORIZONTAL);
  auto layout = std::make_unique<views::TableLayout>();
  layout
      ->AddPaddingColumn(views::TableLayout::kFixedSize, dialog_insets.left())
      // App icon.
      .AddColumn(views::LayoutAlignment::kCenter,
                 views::LayoutAlignment::kCenter,
                 views::TableLayout::kFixedSize,
                 views::TableLayout::ColumnSize::kUsePreferred, 0, 0)
      .AddPaddingColumn(views::TableLayout::kFixedSize, icon_label_spacing)
      // App name.
      .AddColumn(views::LayoutAlignment::kStretch,
                 views::LayoutAlignment::kCenter, 1.0f,
                 views::TableLayout::ColumnSize::kUsePreferred, 0, 0)
      .AddPaddingColumn(views::TableLayout::kFixedSize, icon_label_spacing)
      // App settings link button.
      .AddColumn(views::LayoutAlignment::kEnd, views::LayoutAlignment::kStretch,
                 views::TableLayout::kFixedSize,
                 views::TableLayout::ColumnSize::kUsePreferred, 0,
                 kLinkIconSize)
      .AddPaddingColumn(views::TableLayout::kFixedSize, dialog_insets.right())
      .AddRows(1, views::TableLayout::kFixedSize);
  return layout;
}

}  // namespace

RelatedAppRowView::RelatedAppRowView(
    Profile* profile,
    const webapps::AppId& app_id,
    base::RepeatingCallback<void(const webapps::AppId&)>
        open_site_settings_callback_)
    : app_id_(std::move(app_id)),
      open_site_settings_callback_(std::move(open_site_settings_callback_)) {
  web_app::WebAppProvider* provider =
      web_app::WebAppProvider::GetForWebApps(profile);
  web_app::WebAppRegistrar& registrar = provider->registrar_unsafe();
  // Since we're using registrar_unsafe() above (and earlier, in
  // PageSpecificSiteDataDialog::GetInstalledRelatedApps), someone could've
  // removed `app_id` from the registrar since we last checked. However, the
  // chances of this happening are pretty rare. Additionally, using the "safe"
  // method of querying the registrar uses async locks and callbacks, which
  // isn't compatible with our parent view's builder
  // (PageSpecificSiteDataDialog::ShowPageSpecificSiteDataDialog), as we'd
  // have to block the UI building to wait for the safe registrar. Given these 2
  // facts, we will continue using registrar_unsafe(), and add a sanity check
  // here that the app_id we're trying to make a view for is still in the
  // registrar since we last checked. If it's not, early return an empty view.
  if (registrar.IsNotInRegistrar(app_id_)) {
    return;
  }
  web_app::WebAppIconManager& icon_manager = provider->icon_manager();

  const int vertical_padding = ChromeLayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_RELATED_CONTROL_VERTICAL);

  views::TableLayout* layout = SetLayoutManager(SetupTableLayout());
  auto* app_icon_image =
      AddChildView(std::make_unique<NonAccessibleImageView>());
  app_icon_image->SetImage(
      ui::ImageModel::FromImageSkia(icon_manager.GetFaviconImageSkia(app_id)));

  auto* app_name_label = AddChildView(std::make_unique<views::Label>(
      base::UTF8ToUTF16(registrar.GetAppShortName(app_id_))));
  app_name_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);

  auto* uninstall_button_container =
      AddChildView(std::make_unique<views::View>());
  uninstall_button_container->SetUseDefaultFillLayout(true);

  // TODO(crbug.com/362922563): Implement uninstall behavior instead of linking
  // to the app settings page.
  views::ImageButton::PressedCallback callback(base::BindRepeating(
      &RelatedAppRowView::OnAppSettingsLinkClick, base::Unretained(this)));

  auto* app_settings_page_link = uninstall_button_container->AddChildView(
      views::CreateVectorImageButtonWithNativeTheme(
          std::move(callback), views::kLaunchIcon, kLinkIconSize));

  views::InstallCircleHighlightPathGenerator(app_settings_page_link);
  app_settings_page_link->SetTooltipText(l10n_util::GetStringUTF16(
      IDS_PAGE_SPECIFIC_SITE_DATA_DIALOG_RELATED_APPS_SETTINGS_LINK_TOOLTIP));
  app_settings_page_link->SetProperty(views::kElementIdentifierKey,
                                      kLinkToAppSettings);

  layout->AddPaddingRow(views::TableLayout::kFixedSize, vertical_padding);

  install_manager_observation_.Observe(&provider->install_manager());
}

RelatedAppRowView::~RelatedAppRowView() {
  install_manager_observation_.Reset();
}

void RelatedAppRowView::OnWebAppUninstalled(
    const webapps::AppId& app_id,
    webapps::WebappUninstallSource uninstall_source) {
  app_was_uninstalled_ = true;
}

void RelatedAppRowView::OnAppSettingsLinkClick() {
  if (!open_site_settings_callback_) {
    return;
  }

  // Sanity check that the app has not been uninstalled since we checked
  // earlier. If it was, early return to avoid opening a broken link.
  if (app_was_uninstalled_) {
    return;
  }

  open_site_settings_callback_.Run(app_id_);
}

BEGIN_METADATA(RelatedAppRowView)
END_METADATA
