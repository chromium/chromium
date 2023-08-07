// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/partition_allocator/pointers/raw_ptr.h"
#include "base/i18n/message_formatter.h"
#include "base/types/cxx23_to_underlying.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/web_apps/web_app_info_image_source.h"
#include "chrome/browser/ui/web_applications/sub_apps_install_dialog_controller.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/omnibox/browser/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/bubble/bubble_dialog_model_host.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/layout/box_layout_view.h"

namespace {

constexpr int kSubAppIconSize = 32;

using DialogViewIDForTesting =
    web_app::SubAppsInstallDialogController::DialogViewIDForTesting;

class SubAppsListView : public views::BoxLayoutView {
 public:
  explicit SubAppsListView(
      const std::vector<std::unique_ptr<web_app::WebAppInstallInfo>>& sub_apps);

 private:
  views::ScrollView* AddScrollableArea();
  views::BoxLayoutView* AddListLayout(views::ScrollView* scroll_view);
  void AddSubAppToList(views::BoxLayoutView* sub_app_list,
                       const std::u16string& sub_app_name,
                       const std::map<SquareSizePx, SkBitmap>& icons);

  raw_ptr<ChromeLayoutProvider> layout_provider_ = nullptr;
};

ui::ImageModel GetIcon();
std::u16string GetTitle(int num_sub_apps);
ui::DialogModelLabel DialogDescription(int num_sub_apps,
                                       std::string_view parent_app_name,
                                       std::string_view parent_app_scope);
ui::DialogModelLabel PermissionsExplanation(int num_sub_apps,
                                            std::string_view parent_app_name);
std::unique_ptr<views::BubbleDialogModelHost::CustomView> CreateSubAppListView(
    const std::vector<std::unique_ptr<web_app::WebAppInstallInfo>>& sub_apps);
std::u16string AcceptLabel();
std::u16string CancelLabel();

}  // namespace

namespace chrome {

views::Widget* CreateSubAppsInstallDialogWidget(
    const std::string_view parent_app_name,
    const std::string_view parent_app_scope,
    const std::vector<std::unique_ptr<web_app::WebAppInstallInfo>>& sub_apps,
    gfx::NativeWindow window) {
  auto dialog_builder = ui::DialogModel::Builder();

  dialog_builder.SetInternalName("SubAppsInstallDialogController")
      .SetIcon(GetIcon())
      .SetTitle(GetTitle(sub_apps.size()))
      .AddParagraph(
          DialogDescription(sub_apps.size(), parent_app_name, parent_app_scope))
      .AddCustomField(CreateSubAppListView(sub_apps))
      .AddParagraph(PermissionsExplanation(sub_apps.size(), parent_app_name))
      .AddOkButton(base::DoNothing(),
                   ui::DialogModelButton::Params().SetLabel(AcceptLabel()))
      .AddCancelButton(base::DoNothing(),
                       ui::DialogModelButton::Params().SetLabel(CancelLabel()))
      .OverrideShowCloseButton(false);

  auto dialog = views::BubbleDialogModelHost::CreateModal(
      dialog_builder.Build(), ui::MODAL_TYPE_WINDOW);
  dialog->SetOwnedByWidget(true);
  views::Widget* widget = constrained_window::CreateBrowserModalDialogViews(
      std::move(dialog), window);

  return widget;
}

}  // namespace chrome

namespace {

ui::ImageModel GetIcon() {
  return ui::ImageModel::FromVectorIcon(
      omnibox::kInstallDesktopIcon, ui::kColorIcon,
      ChromeLayoutProvider::Get()->GetDistanceMetric(
          DISTANCE_BUBBLE_HEADER_VECTOR_ICON_SIZE));
}

std::u16string GetTitle(int num_sub_apps) {
  return base::i18n::MessageFormatter::FormatWithNamedArgs(
      l10n_util::GetStringUTF16(IDS_SUB_APPS_INSTALL_DIALOG_TITLE),
      /*name0=*/"NUM_SUB_APP_INSTALLS", num_sub_apps);
}

ui::DialogModelLabel DialogDescription(int num_sub_apps,
                                       std::string_view parent_app_name,
                                       std::string_view parent_app_scope) {
  std::u16string description =
      base::i18n::MessageFormatter::FormatWithNamedArgs(
          l10n_util::GetStringUTF16(IDS_SUB_APPS_INSTALL_DIALOG_DESCRIPTION),
          /*name0=*/"NUM_SUB_APP_INSTALLS", num_sub_apps,
          /*name1=*/"APP_NAME", base::ASCIIToUTF16(parent_app_name), "DOMAIN",
          base::ASCIIToUTF16(parent_app_scope));
  ui::DialogModelLabel label = ui::DialogModelLabel(description);
  label.set_is_secondary().set_allow_character_break();
  return label;
}

ui::DialogModelLabel PermissionsExplanation(int num_sub_apps,
                                            std::string_view parent_app_name) {
  std::u16string description =
      base::i18n::MessageFormatter::FormatWithNamedArgs(
          l10n_util::GetStringUTF16(
              IDS_SUB_APPS_INSTALL_DIALOG_PERMISSIONS_DESCRIPTION),
          /*name0=*/"NUM_SUB_APP_INSTALLS", num_sub_apps,
          /*name1=*/"APP_NAME", base::ASCIIToUTF16(parent_app_name));
  ui::DialogModelLabel label = ui::DialogModelLabel(description);
  label.set_is_secondary().set_allow_character_break();
  return label;
}

std::u16string AcceptLabel() {
  return l10n_util::GetStringUTF16(
      IDS_SUB_APPS_INSTALL_DIALOG_PERMISSIONS_BUTTON);
}

std::u16string CancelLabel() {
  return l10n_util::GetStringUTF16(IDS_SUB_APPS_INSTALL_DIALOG_CANCEL_BUTTON);
}

std::unique_ptr<views::BubbleDialogModelHost::CustomView> CreateSubAppListView(
    const std::vector<std::unique_ptr<web_app::WebAppInstallInfo>>& sub_apps) {
  return std::make_unique<views::BubbleDialogModelHost::CustomView>(
      std::make_unique<SubAppsListView>(sub_apps),
      views::BubbleDialogModelHost::FieldType::kMenuItem);
}

SubAppsListView::SubAppsListView(
    const std::vector<std::unique_ptr<web_app::WebAppInstallInfo>>& sub_apps) {
  layout_provider_ = ChromeLayoutProvider::Get();
  DCHECK(layout_provider_);

  auto* scrollable_area = AddScrollableArea();
  auto* sub_app_list = AddListLayout(scrollable_area);

  for (const std::unique_ptr<web_app::WebAppInstallInfo>& sub_app : sub_apps) {
    AddSubAppToList(sub_app_list, sub_app->title, sub_app->icon_bitmaps.any);
  }
}

views::ScrollView* SubAppsListView::AddScrollableArea() {
  auto* scrollable_area = AddChildView(std::make_unique<views::ScrollView>());

  scrollable_area->SetHorizontalScrollBarMode(
      views::ScrollView::ScrollBarMode::kDisabled);
  scrollable_area->ClipHeightTo(
      0, layout_provider_->GetDistanceMetric(
             views::DISTANCE_DIALOG_SCROLLABLE_AREA_MAX_HEIGHT));

  return scrollable_area;
}

views::BoxLayoutView* SubAppsListView::AddListLayout(
    views::ScrollView* scroll_view) {
  auto* sub_app_list =
      scroll_view->SetContents(std::make_unique<views::BoxLayoutView>());

  sub_app_list->SetOrientation(views::BoxLayout::Orientation::kVertical);
  sub_app_list->SetBetweenChildSpacing(
      layout_provider_->GetDistanceMetric(DISTANCE_CONTROL_LIST_VERTICAL));
  sub_app_list->SetInsideBorderInsets(
      gfx::Insets::TLBR(0,
                        layout_provider_->GetDistanceMetric(
                            DISTANCE_UNRELATED_CONTROL_HORIZONTAL),
                        0, 0));

  return sub_app_list;
}

void SubAppsListView::AddSubAppToList(
    views::BoxLayoutView* sub_app_list,
    const std::u16string& sub_app_name,
    const std::map<SquareSizePx, SkBitmap>& icons) {
  auto* box =
      sub_app_list->AddChildView(std::make_unique<views::BoxLayoutView>());
  box->SetOrientation(views::BoxLayout::Orientation::kHorizontal);
  box->SetBetweenChildSpacing(layout_provider_->GetDistanceMetric(
      views::DISTANCE_RELATED_LABEL_HORIZONTAL));

  auto* sub_app_icon = box->AddChildView(std::make_unique<views::ImageView>());
  sub_app_icon->SetImage(gfx::ImageSkia(
      std::make_unique<WebAppInfoImageSource>(kSubAppIconSize, icons),
      gfx::Size(kSubAppIconSize, kSubAppIconSize)));
  sub_app_icon->SetGroup(
      base::to_underlying(DialogViewIDForTesting::SUB_APP_ICON));

  auto* sub_app_label =
      box->AddChildView(std::make_unique<views::Label>(sub_app_name));
  sub_app_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  sub_app_label->SetMultiLine(true);
  sub_app_label->SetGroup(
      base::to_underlying(DialogViewIDForTesting::SUB_APP_LABEL));
}

}  // namespace
