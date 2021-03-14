// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/web_apps/web_app_confirmation_view.h"

#include <memory>
#include <string>
#include <utility>

#include "base/callback_helpers.h"
#include "base/feature_list.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/web_apps/web_app_info_image_source.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/button/radio_button.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/metadata/metadata_impl_macros.h"
#include "ui/views/widget/widget.h"

namespace {

bool g_auto_accept_web_app_for_testing = false;
bool g_auto_check_open_in_window_for_testing = false;

bool ShowRadioButtons() {
  // This UI is only for prototyping and is not intended for shipping.
  DCHECK_EQ(features::kDesktopPWAsTabStrip.default_state,
            base::FEATURE_DISABLED_BY_DEFAULT);
  return base::FeatureList::IsEnabled(features::kDesktopPWAsTabStrip);
}

}  // namespace

WebAppConfirmationView::~WebAppConfirmationView() {}

WebAppConfirmationView::WebAppConfirmationView(
    std::unique_ptr<WebApplicationInfo> web_app_info,
    chrome::AppInstallationAcceptanceCallback callback)
    : web_app_info_(std::move(web_app_info)), callback_(std::move(callback)) {
  DCHECK(web_app_info_);
  SetButtonLabel(ui::DIALOG_BUTTON_OK,
                 l10n_util::GetStringUTF16(IDS_CREATE_SHORTCUTS_BUTTON_LABEL));
  SetModalType(ui::MODAL_TYPE_CHILD);
  SetTitle(IDS_ADD_TO_OS_LAUNCH_SURFACE_BUBBLE_TITLE);
  const ChromeLayoutProvider* layout_provider = ChromeLayoutProvider::Get();
  set_margins(layout_provider->GetDialogInsetsForContentType(views::CONTROL,
                                                             views::TEXT));
  views::GridLayout* layout =
      SetLayoutManager(std::make_unique<views::GridLayout>());
  constexpr int kColumnSetId = 0;

  views::ColumnSet* column_set = layout->AddColumnSet(kColumnSetId);
  column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::CENTER,
                        views::GridLayout::kFixedSize,
                        views::GridLayout::ColumnSize::kUsePreferred, 0, 0);
  column_set->AddPaddingColumn(views::GridLayout::kFixedSize,
                               layout_provider->GetDistanceMetric(
                                   views::DISTANCE_RELATED_CONTROL_HORIZONTAL));
  constexpr int textfield_width = 320;
  column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::CENTER,
                        views::GridLayout::kFixedSize,
                        views::GridLayout::ColumnSize::kFixed, textfield_width,
                        0);

  auto icon_image_view = std::make_unique<views::ImageView>();
  gfx::Size image_size(web_app::kWebAppIconSmall, web_app::kWebAppIconSmall);
  gfx::ImageSkia image(
      std::make_unique<WebAppInfoImageSource>(web_app::kWebAppIconSmall,
                                              web_app_info_->icon_bitmaps.any),
      image_size);
  icon_image_view->SetImageSize(image_size);
  icon_image_view->SetImage(image);
  layout->StartRow(views::GridLayout::kFixedSize, kColumnSetId);
  layout->AddView(std::move(icon_image_view));

  auto title_tf = std::make_unique<views::Textfield>();
  title_tf->SetText(web_app_info_->title);
  title_tf->SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_BOOKMARK_APP_AX_BUBBLE_NAME_LABEL));
  title_tf->set_controller(this);
  title_tf_ = layout->AddView(std::move(title_tf));

  layout->AddPaddingRow(
      views::GridLayout::kFixedSize,
      layout_provider->GetDistanceMetric(DISTANCE_CONTROL_LIST_VERTICAL));

  if (ShowRadioButtons()) {
    constexpr int kRadioGroupId = 1;
    auto open_as_tab_radio = std::make_unique<views::RadioButton>(
        l10n_util::GetStringUTF16(IDS_BOOKMARK_APP_BUBBLE_OPEN_AS_TAB),
        kRadioGroupId);
    auto open_as_window_radio = std::make_unique<views::RadioButton>(
        l10n_util::GetStringUTF16(IDS_BOOKMARK_APP_BUBBLE_OPEN_AS_WINDOW),
        kRadioGroupId);
    auto open_as_tabbed_window_radio = std::make_unique<views::RadioButton>(
        l10n_util::GetStringUTF16(
            IDS_BOOKMARK_APP_BUBBLE_OPEN_AS_TABBED_WINDOW),
        kRadioGroupId);

    layout->StartRow(views::GridLayout::kFixedSize, kColumnSetId);
    layout->SkipColumns(1);
    open_as_tab_radio_ = layout->AddView(std::move(open_as_tab_radio));

    layout->StartRow(views::GridLayout::kFixedSize, kColumnSetId);
    layout->SkipColumns(1);
    open_as_window_radio_ = layout->AddView(std::move(open_as_window_radio));

    layout->StartRow(views::GridLayout::kFixedSize, kColumnSetId);
    layout->SkipColumns(1);
    open_as_tabbed_window_radio_ =
        layout->AddView(std::move(open_as_tabbed_window_radio));

    if (!web_app_info_->open_as_window)
      open_as_tab_radio_->SetChecked(true);
    else if (!web_app_info_->enable_experimental_tabbed_window)
      open_as_window_radio_->SetChecked(true);
    else
      open_as_tabbed_window_radio_->SetChecked(true);
  } else {
    auto open_as_window_checkbox = std::make_unique<views::Checkbox>(
        l10n_util::GetStringUTF16(IDS_BOOKMARK_APP_BUBBLE_OPEN_AS_WINDOW));
    open_as_window_checkbox->SetChecked(web_app_info_->open_as_window);
    layout->StartRow(views::GridLayout::kFixedSize, kColumnSetId);
    layout->SkipColumns(1);
    open_as_window_checkbox_ =
        layout->AddView(std::move(open_as_window_checkbox));
  }

  if (g_auto_check_open_in_window_for_testing) {
    if (ShowRadioButtons())
      open_as_window_radio_->SetChecked(true);
    else
      open_as_window_checkbox_->SetChecked(true);
  }

  title_tf_->SelectAll(true);
  chrome::RecordDialogCreation(chrome::DialogIdentifier::WEB_APP_CONFIRMATION);
}

views::View* WebAppConfirmationView::GetInitiallyFocusedView() {
  return title_tf_;
}

bool WebAppConfirmationView::ShouldShowCloseButton() const {
  return false;
}

void WebAppConfirmationView::WindowClosing() {
  if (callback_) {
    DCHECK(web_app_info_);
    std::move(callback_).Run(false, std::move(web_app_info_));
  }
}

bool WebAppConfirmationView::Accept() {
  DCHECK(web_app_info_);
  web_app_info_->title = GetTrimmedTitle();
  if (ShowRadioButtons()) {
    if (open_as_tabbed_window_radio_->GetChecked()) {
      web_app_info_->open_as_window = true;
      web_app_info_->enable_experimental_tabbed_window = true;
    } else {
      web_app_info_->open_as_window = open_as_window_radio_->GetChecked();
      web_app_info_->enable_experimental_tabbed_window = false;
    }
  } else {
    web_app_info_->open_as_window = open_as_window_checkbox_->GetChecked();
  }
  std::move(callback_).Run(true, std::move(web_app_info_));
  return true;
}

bool WebAppConfirmationView::IsDialogButtonEnabled(
    ui::DialogButton button) const {
  return button == ui::DIALOG_BUTTON_OK ? !GetTrimmedTitle().empty() : true;
}

void WebAppConfirmationView::ContentsChanged(
    views::Textfield* sender,
    const std::u16string& new_contents) {
  DCHECK_EQ(title_tf_, sender);
  DialogModelChanged();
}

std::u16string WebAppConfirmationView::GetTrimmedTitle() const {
  std::u16string title(title_tf_->GetText());
  base::TrimWhitespace(title, base::TRIM_ALL, &title);
  return title;
}

BEGIN_METADATA(WebAppConfirmationView, views::DialogDelegateView)
ADD_READONLY_PROPERTY_METADATA(std::u16string, TrimmedTitle)
END_METADATA

namespace chrome {

void ShowWebAppInstallDialog(content::WebContents* web_contents,
                             std::unique_ptr<WebApplicationInfo> web_app_info,
                             AppInstallationAcceptanceCallback callback) {
  auto* dialog =
      new WebAppConfirmationView(std::move(web_app_info), std::move(callback));
  constrained_window::ShowWebModalDialogViews(dialog, web_contents);

  if (g_auto_accept_web_app_for_testing) {
    dialog->AcceptDialog();
  }
}

void SetAutoAcceptWebAppDialogForTesting(bool auto_accept,
                                         bool auto_open_in_window) {
  g_auto_accept_web_app_for_testing = auto_accept;
  g_auto_check_open_in_window_for_testing = auto_open_in_window;
}

}  // namespace chrome
