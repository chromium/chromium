// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/web_apps/deprecated_apps_dialog_view.h"

#include <utility>

#include "base/i18n/rtl.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/vector_icons/vector_icons.h"
#include "components/web_modal/web_contents_modal_dialog_host.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_icon_image.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_icon_set.h"
#include "extensions/common/manifest_handlers/icons_handler.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/table_model.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/favicon_size.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/widget/widget.h"

class DeprecatedAppsDialogView::DeprecatedAppsTableModel
    : public ui::TableModel,
      public extensions::IconImage::Observer {
 public:
  DeprecatedAppsTableModel(
      const std::set<extensions::ExtensionId>& deprecated_app_ids,
      content::WebContents* web_contents,
      base::RepeatingClosure on_icon_updated)
      : on_icon_updated_(on_icon_updated) {
    for (extensions::ExtensionId app_id : deprecated_app_ids) {
      auto* browser_context = web_contents->GetBrowserContext();
      const extensions::Extension* extension =
          extensions::ExtensionRegistry::Get(browser_context)
              ->GetInstalledExtension(app_id);
      DCHECK(extension);
      const gfx::ImageSkia default_icon = gfx::CreateVectorIcon(
          vector_icons::kExtensionIcon, gfx::kFaviconSize, gfx::kGoogleGrey700);

      auto app_icon = std::make_unique<extensions::IconImage>(
          browser_context, extension,
          extension ? extensions::IconsInfo::GetIcons(extension)
                    : ExtensionIconSet(),
          extension_misc::EXTENSION_ICON_BITTY, default_icon, this);

      Row row;
      row.app_name = extension->name();
      row.icon = std::move(app_icon);
      rows_.push_back(std::move(row));
    }
  }

  DeprecatedAppsTableModel(const DeprecatedAppsTableModel&) = delete;
  DeprecatedAppsTableModel& operator=(const DeprecatedAppsTableModel&) = delete;
  ~DeprecatedAppsTableModel() override = default;

  // ui::TableModel implementations:
  int RowCount() override { return rows_.size(); }

  std::u16string GetText(int index, int column_id) override {
    DCHECK(index >= 0 && index < RowCount());
    return base::UTF8ToUTF16(rows_[index].app_name);
  }

  ui::ImageModel GetIcon(int index) override {
    return ui::ImageModel::FromImageSkia(rows_[index].icon->image_skia());
  }

  //  IconImage::Observer implementations:
  void OnExtensionIconImageChanged(extensions::IconImage* image) override {
    on_icon_updated_.Run();
  }

  void SetObserver(ui::TableModelObserver* observer) override {}

  void Reset() { rows_.clear(); }

 private:
  // Store information to fill per row of the DeprecatedAppsTableModel.
  struct Row {
    std::string app_name;
    std::unique_ptr<extensions::IconImage> icon;
  };

  std::vector<Row> rows_;
  base::RepeatingClosure on_icon_updated_;
};

DeprecatedAppsDialogView::~DeprecatedAppsDialogView() {
  deprecated_apps_table_view_->SetModel(nullptr);
}

// static
DeprecatedAppsDialogView* DeprecatedAppsDialogView::CreateAndShowDialog(
    const std::set<extensions::ExtensionId>& deprecated_app_ids,
    content::WebContents* web_contents) {
  DeprecatedAppsDialogView* view =
      new DeprecatedAppsDialogView(deprecated_app_ids, web_contents);
  view->InitDialog();
  constrained_window::ShowWebModalDialogViews(view, web_contents);
  return view;
}

base::WeakPtr<DeprecatedAppsDialogView> DeprecatedAppsDialogView::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

std::u16string DeprecatedAppsDialogView::GetWindowTitle() const {
  return l10n_util::GetPluralStringFUTF16(
      IDS_DEPRECATED_APPS_RENDERER_TITLE,
      deprecated_apps_table_model_->RowCount());
}

DeprecatedAppsDialogView::DeprecatedAppsDialogView(
    const std::set<extensions::ExtensionId>& deprecated_app_ids,
    content::WebContents* web_contents)
    : deprecated_app_ids_(deprecated_app_ids), web_contents_(web_contents) {
  deprecated_apps_table_model_ = std::make_unique<DeprecatedAppsTableModel>(
      deprecated_app_ids, web_contents,
      base::BindRepeating(&DeprecatedAppsDialogView::OnIconsLoadedForTable,
                          base::Unretained(this)));
}

void DeprecatedAppsDialogView::InitDialog() {
  SetCanResize(false);
  SetModalType(ui::MODAL_TYPE_CHILD);

  set_margins(ChromeLayoutProvider::Get()->GetDialogInsetsForContentType(
      views::DialogContentType::kText, views::DialogContentType::kControl));

  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(),
      ChromeLayoutProvider::Get()->GetDistanceMetric(
          views::DISTANCE_UNRELATED_CONTROL_VERTICAL)));
  set_fixed_width(views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH));

  // Set up buttons.
  SetButtonLabel(ui::DIALOG_BUTTON_OK,
                 l10n_util::GetPluralStringFUTF16(
                     IDS_DEPRECATED_APPS_OK_LABEL,
                     deprecated_apps_table_model_->RowCount()));
  SetButtonLabel(ui::DIALOG_BUTTON_CANCEL,
                 l10n_util::GetStringUTF16(IDS_DEPRECATED_APPS_CANCEL_LABEL));
  SetDefaultButton(ui::DIALOG_BUTTON_NONE);
  SetCancelCallback(base::BindOnce(&DeprecatedAppsDialogView::CloseDialog,
                                   base::Unretained(this)));
  SetAcceptCallback(base::BindOnce(
      &DeprecatedAppsDialogView::UninstallExtensions, base::Unretained(this)));

  info_label_ = AddChildView(
      std::make_unique<views::Label>(l10n_util::GetPluralStringFUTF16(
          IDS_DEPRECATED_APPS_MONITOR_RENDERER,
          deprecated_apps_table_model_->RowCount())));
  info_label_->SetMultiLine(true);
  info_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);

  auto* learn_more = AddChildView(std::make_unique<views::Link>(
      l10n_util::GetStringUTF16(IDS_DEPRECATED_APPS_LEARN_MORE)));
  learn_more->SetCallback(base::BindRepeating(
      [](content::WebContents* web_contents, const ui::Event& event) {
        web_contents->OpenURL(content::OpenURLParams(
            GURL(chrome::kChromeAppsDeprecationLearnMoreURL),
            content::Referrer(),
            ui::DispositionFromEventFlags(
                event.flags(), WindowOpenDisposition::NEW_FOREGROUND_TAB),
            ui::PAGE_TRANSITION_LINK, /*is_renderer_initiated=*/false));
      },
      web_contents_));
  learn_more->SetAccessibleName(l10n_util::GetStringUTF16(
      IDS_FORCE_INSTALLED_DEPRECATED_APPS_LEARN_MORE_AX_LABEL));
  learn_more->SetHorizontalAlignment(gfx::ALIGN_LEFT);

  // Set up the table view.
  std::vector<ui::TableColumn> columns;
  columns.emplace_back(ui::TableColumn());

  auto table = std::make_unique<views::TableView>(
      deprecated_apps_table_model_.get(), columns, views::ICON_AND_TEXT,
      /*single_selection=*/true);
  deprecated_apps_table_view_ = table.get();
  table->SetID(DEPRECATED_APPS_TABLE);
  auto* scroll_view = AddChildView(
      views::TableView::CreateScrollViewWithTable(std::move(table)));

  // Set up the scroll view to display a maximum of 8 items before scrolling.
  DCHECK(scroll_view);
  constexpr int kMaxRowsInTableView = 8;
  scroll_view->ClipHeightTo(
      0, deprecated_apps_table_view_->GetRowHeight() * kMaxRowsInTableView);
}

void DeprecatedAppsDialogView::CloseDialog() {
  deprecated_apps_table_model_->Reset();
  GetWidget()->Close();
}

void DeprecatedAppsDialogView::OnIconsLoadedForTable() {
  deprecated_apps_table_view_->SchedulePaint();
}

void DeprecatedAppsDialogView::UninstallExtensions() {
  for (extensions::ExtensionId id : deprecated_app_ids_) {
    extensions::ExtensionSystem::Get(web_contents_->GetBrowserContext())
        ->extension_service()
        ->UninstallExtension(id, extensions::UNINSTALL_REASON_USER_INITIATED,
                             /*error=*/nullptr);
  }
  CloseDialog();
}

BEGIN_METADATA(DeprecatedAppsDialogView, views::DialogDelegateView)
END_METADATA
