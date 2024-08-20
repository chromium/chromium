// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/web_apps/create_shortcut_confirmation_view.h"

#include <memory>
#include <string>
#include <utility>

#include "base/feature_list.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/web_apps/web_app_info_image_source.h"
#include "chrome/browser/ui/views/web_apps/web_app_install_dialog_delegate.h"
#include "chrome/browser/ui/web_applications/web_app_dialogs.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/strings/grit/components_strings.h"
#include "components/webapps/browser/installable/ml_install_operation_tracker.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "third_party/blink/public/common/features.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/button/radio_button.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/layout/table_layout.h"
#include "ui/views/widget/widget.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chromeos/constants/chromeos_features.h"
#endif

namespace {

CreateShortcutConfirmationView* g_dialog_for_testing = nullptr;
bool g_auto_accept_web_app_for_testing = false;
bool g_auto_check_open_in_window_for_testing = false;
const char* g_title_to_use_for_app = nullptr;

bool ShowRadioButtons() {
  // This UI is only for prototyping and is not intended for shipping.
  DCHECK_EQ(features::kDesktopPWAsTabStripSettings.default_state,
            base::FEATURE_DISABLED_BY_DEFAULT);
  return base::FeatureList::IsEnabled(blink::features::kDesktopPWAsTabStrip) &&
         base::FeatureList::IsEnabled(features::kDesktopPWAsTabStripSettings);
}

}  // namespace

// static
CreateShortcutConfirmationView*
CreateShortcutConfirmationView::GetDialogForTesting() {
  return g_dialog_for_testing;
}

CreateShortcutConfirmationView::~CreateShortcutConfirmationView() = default;

CreateShortcutConfirmationView::CreateShortcutConfirmationView(
    std::unique_ptr<web_app::WebAppInstallInfo> web_app_info,
    std::unique_ptr<webapps::MlInstallOperationTracker> install_tracker,
    web_app::AppInstallationAcceptanceCallback callback)
    : web_app_info_(std::move(web_app_info)),
      install_tracker_(std::move(install_tracker)),
      callback_(std::move(callback)) {
  DCHECK(web_app_info_);
  const ChromeLayoutProvider* layout_provider = ChromeLayoutProvider::Get();

  // Define the table layout.
  constexpr int textfield_width = 320;
  auto* layout = SetLayoutManager(std::make_unique<views::TableLayout>());
  layout
      ->AddColumn(views::LayoutAlignment::kStretch,
                  views::LayoutAlignment::kCenter,
                  views::TableLayout::kFixedSize,
                  views::TableLayout::ColumnSize::kUsePreferred, 0, 0)
      .AddPaddingColumn(views::TableLayout::kFixedSize,
                        layout_provider->GetDistanceMetric(
                            views::DISTANCE_RELATED_CONTROL_HORIZONTAL))
      .AddColumn(views::LayoutAlignment::kStretch,
                 views::LayoutAlignment::kCenter,
                 views::TableLayout::kFixedSize,
                 views::TableLayout::ColumnSize::kFixed, textfield_width, 0)
      .AddRows(1, views::TableLayout::kFixedSize)
      .AddPaddingRow(
          views::TableLayout::kFixedSize,
          layout_provider->GetDistanceMetric(DISTANCE_CONTROL_LIST_VERTICAL))
      .AddRows(ShowRadioButtons() ? 3 : 1, views::TableLayout::kFixedSize);

  gfx::Size image_size(web_app::kWebAppIconSmall, web_app::kWebAppIconSmall);
  gfx::ImageSkia image(
      std::make_unique<WebAppInfoImageSource>(web_app::kWebAppIconSmall,
                                              web_app_info_->icon_bitmaps.any),
      image_size);

  // Builds the header row child views.
  auto builder =
      views::Builder<CreateShortcutConfirmationView>(this)
          .SetButtonLabel(
              ui::mojom::DialogButton::kOk,
              l10n_util::GetStringUTF16(IDS_CREATE_SHORTCUTS_BUTTON_LABEL))
          .SetModalType(ui::mojom::ModalType::kChild)
          .SetTitle(IDS_ADD_TO_OS_LAUNCH_SURFACE_BUBBLE_TITLE)
          .SetAcceptCallback(
              base::BindOnce(&CreateShortcutConfirmationView::OnAccept,
                             weak_ptr_factory_.GetWeakPtr()))
          .SetCloseCallback(
              base::BindOnce(&CreateShortcutConfirmationView::OnClose,
                             weak_ptr_factory_.GetWeakPtr()))
          .SetCancelCallback(
              base::BindOnce(&CreateShortcutConfirmationView::OnCancel,
                             weak_ptr_factory_.GetWeakPtr()))
          .set_margins(layout_provider->GetDialogInsetsForContentType(
              views::DialogContentType::kControl,
              views::DialogContentType::kText))
          .AddChildren(views::Builder<views::ImageView>()
                           .SetImageSize(image_size)
                           .SetImage(ui::ImageModel::FromImageSkia(image)),
                       views::Builder<views::Textfield>()
                           .CopyAddressTo(&title_tf_)
                           .SetText(web_app::NormalizeSuggestedAppTitle(
                               g_title_to_use_for_app != nullptr
                                   ? base::ASCIIToUTF16(g_title_to_use_for_app)
                                   : web_app_info_->title))
                           .SetAccessibleName(l10n_util::GetStringUTF16(
                               IDS_BOOKMARK_APP_AX_BUBBLE_NAME_LABEL))
                           .SetController(this));

  const auto display_mode = web_app_info_->user_display_mode;

  // Build the content child views.
  if (ShowRadioButtons()) {
    constexpr int kRadioGroupId = 0;
    builder.AddChildren(
        views::Builder<views::View>(),  // Skip the first column.
        views::Builder<views::RadioButton>()
            .CopyAddressTo(&open_as_tab_radio_)
            .SetText(
                l10n_util::GetStringUTF16(IDS_BOOKMARK_APP_BUBBLE_OPEN_AS_TAB))
            .SetGroup(kRadioGroupId)
            .SetChecked(display_mode ==
                        web_app::mojom::UserDisplayMode::kBrowser),
        views::Builder<views::View>(),  // Column skip.
        views::Builder<views::RadioButton>()
            .CopyAddressTo(&open_as_window_radio_)
            .SetText(l10n_util::GetStringUTF16(
                IDS_BOOKMARK_APP_BUBBLE_OPEN_AS_WINDOW))
            .SetGroup(kRadioGroupId)
            .SetChecked(
                display_mode != web_app::mojom::UserDisplayMode::kBrowser &&
                display_mode != web_app::mojom::UserDisplayMode::kTabbed),
        views::Builder<views::View>(),  // Column skip.
        views::Builder<views::RadioButton>()
            .CopyAddressTo(&open_as_tabbed_window_radio_)
            .SetText(l10n_util::GetStringUTF16(
                IDS_BOOKMARK_APP_BUBBLE_OPEN_AS_TABBED_WINDOW))
            .SetGroup(kRadioGroupId)
            .SetChecked(display_mode ==
                        web_app::mojom::UserDisplayMode::kTabbed));
  } else {
    builder.AddChildren(
        views::Builder<views::View>(),  // Column skip.
        views::Builder<views::Checkbox>()
            .CopyAddressTo(&open_as_window_checkbox_)
            .SetText(l10n_util::GetStringUTF16(
                IDS_BOOKMARK_APP_BUBBLE_OPEN_AS_WINDOW))
            .SetChecked(display_mode !=
                        web_app::mojom::UserDisplayMode::kBrowser));
  }

  std::move(builder).BuildChildren();

  if (g_auto_check_open_in_window_for_testing) {
    if (ShowRadioButtons()) {
      open_as_window_radio_->SetChecked(true);
    } else {
      open_as_window_checkbox_->SetChecked(true);
    }
  }
  title_tf_->SelectAll(true);
}

views::View* CreateShortcutConfirmationView::GetInitiallyFocusedView() {
  return title_tf_;
}

bool CreateShortcutConfirmationView::ShouldShowCloseButton() const {
  return false;
}

bool CreateShortcutConfirmationView::IsDialogButtonEnabled(
    ui::mojom::DialogButton button) const {
  return button == ui::mojom::DialogButton::kOk ? !GetTrimmedTitle().empty()
                                                : true;
}

void CreateShortcutConfirmationView::ContentsChanged(
    views::Textfield* sender,
    const std::u16string& new_contents) {
  DCHECK_EQ(title_tf_, sender);
  DialogModelChanged();
}

std::u16string CreateShortcutConfirmationView::GetTrimmedTitle() const {
  std::u16string title(title_tf_->GetText());
  base::TrimWhitespace(title, base::TRIM_ALL, &title);
  return title;
}

void CreateShortcutConfirmationView::OnAccept() {
  CHECK(web_app_info_);
  web_app_info_->title = GetTrimmedTitle();
  if (ShowRadioButtons()) {
    if (open_as_tabbed_window_radio_->GetChecked()) {
      web_app_info_->user_display_mode =
          web_app::mojom::UserDisplayMode::kTabbed;
    } else {
      web_app_info_->user_display_mode =
          open_as_window_radio_->GetChecked()
              ? web_app::mojom::UserDisplayMode::kStandalone
              : web_app::mojom::UserDisplayMode::kBrowser;
    }
  } else {
    web_app_info_->user_display_mode =
        open_as_window_checkbox_->GetChecked()
            ? web_app::mojom::UserDisplayMode::kStandalone
            : web_app::mojom::UserDisplayMode::kBrowser;
  }
  install_tracker_->ReportResult(webapps::MlInstallUserResponse::kAccepted);
  // Some tests repeatedly create this class, and it's not guaranteed this class
  // is destroyed for subsequent calls. So reset the tracker manually here.
  install_tracker_.reset();
  std::move(callback_).Run(true, std::move(web_app_info_));
}

void CreateShortcutConfirmationView::OnClose() {
  CHECK(install_tracker_);
  install_tracker_->ReportResult(webapps::MlInstallUserResponse::kIgnored);
  RunCloseCallbackIfExists();
}

void CreateShortcutConfirmationView::OnCancel() {
  CHECK(install_tracker_);
  install_tracker_->ReportResult(webapps::MlInstallUserResponse::kCancelled);
  RunCloseCallbackIfExists();
}

void CreateShortcutConfirmationView::RunCloseCallbackIfExists() {
  if (callback_) {
    CHECK(web_app_info_);
    std::move(callback_).Run(false, std::move(web_app_info_));
  }
}

BEGIN_METADATA(CreateShortcutConfirmationView)
ADD_READONLY_PROPERTY_METADATA(std::u16string, TrimmedTitle)
END_METADATA

namespace web_app {

void ShowCreateShortcutDialog(
    content::WebContents* web_contents,
    std::unique_ptr<web_app::WebAppInstallInfo> web_app_info,
    std::unique_ptr<webapps::MlInstallOperationTracker> install_tracker,
    AppInstallationAcceptanceCallback callback) {
  CHECK(web_app_info);
  CHECK(install_tracker);
  auto* dialog = new CreateShortcutConfirmationView(
      std::move(web_app_info), std::move(install_tracker), std::move(callback));
  constrained_window::ShowWebModalDialogViews(dialog, web_contents);

  g_dialog_for_testing = dialog;

  if (g_auto_accept_web_app_for_testing) {
    g_dialog_for_testing->Accept();
  }
}

void SetAutoAcceptWebAppDialogForTesting(bool auto_accept,  // IN-TEST
                                         bool auto_open_in_window) {
  g_auto_accept_web_app_for_testing = auto_accept;
  g_auto_check_open_in_window_for_testing = auto_open_in_window;
}

void SetOverrideTitleForTesting(const char* title_to_use) {
  g_title_to_use_for_app = title_to_use;
}

}  // namespace web_app
