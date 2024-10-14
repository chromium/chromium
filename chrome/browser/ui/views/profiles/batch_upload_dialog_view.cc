// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/batch_upload_dialog_view.h"

#include "base/functional/callback.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/profiles/batch_upload_ui_delegate.h"
#include "chrome/browser/ui/webui/signin/batch_upload_ui.h"
#include "chrome/common/webui_url_constants.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/input/native_web_keyboard_event.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/widget.h"

namespace {

constexpr int kBatchUploadDialogFixedWidth = 512;
constexpr int kBatchUploadDialogMaxHeight = 628;

BatchUploadUI* GetBatchUploadUI(views::WebView* web_view) {
  return web_view->GetWebContents()
      ->GetWebUI()
      ->GetController()
      ->GetAs<BatchUploadUI>();
}

// Account is expected not to be in error.
AccountInfo GetBatchUploadPrimaryAccountInfo(
    signin::IdentityManager& identity_manager) {
  AccountInfo primary_account = identity_manager.FindExtendedAccountInfo(
      identity_manager.GetPrimaryAccountInfo(signin::ConsentLevel::kSignin));
  CHECK(!primary_account.email.empty());
  CHECK(!identity_manager.HasAccountWithRefreshTokenInPersistentErrorState(
      primary_account.account_id));
  return primary_account;
}

}  // namespace

BatchUploadDialogView::~BatchUploadDialogView() {
  // Makes sure that everything is cleaned up if it was not done before.
  OnClose();
}

// static
BatchUploadDialogView* BatchUploadDialogView::CreateBatchUploadDialogView(
    Browser& browser,
    std::vector<BatchUploadDataContainer> data_containers_list,
    SelectedDataTypeItemsCallback complete_callback) {
  std::unique_ptr<BatchUploadDialogView> dialog_view =
      base::WrapUnique(new BatchUploadDialogView(
          browser.profile(), std::move(data_containers_list),
          std::move(complete_callback)));
  BatchUploadDialogView* dialog_view_ptr = dialog_view.get();

  gfx::NativeWindow window = browser.tab_strip_model()
                                 ->GetActiveWebContents()
                                 ->GetTopLevelNativeWindow();

  constrained_window::CreateBrowserModalDialogViews(std::move(dialog_view),
                                                    window);
  return dialog_view_ptr;
}

BatchUploadDialogView::BatchUploadDialogView(
    Profile* profile,
    std::vector<BatchUploadDataContainer> data_containers_list,
    SelectedDataTypeItemsCallback complete_callback)
    : complete_callback_(std::move(complete_callback)) {
  SetModalType(ui::mojom::ModalType::kWindow);
  // No native buttons.
  SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));
  // No close (x) top right button.
  SetShowCloseButton(false);
  set_margins(gfx::Insets());

  // Setting a close callback to make sure every time the view is being closed,
  // that all necessary data are cleared. The view and underlying child views
  // may be destroyed asynchronosly.
  SetCloseCallback(
      base::BindOnce(&BatchUploadDialogView::OnClose, base::Unretained(this)));

  // Create the web view in the native bubble.
  std::unique_ptr<views::WebView> web_view =
      std::make_unique<views::WebView>(profile);
  web_view->LoadInitialURL(GURL(chrome::kChromeUIBatchUploadURL));
  web_view_ = web_view.get();
  web_view_->GetWebContents()->SetDelegate(this);
  SetInitiallyFocusedView(web_view_);
  // Set initial height to max height in order not to have an empty window.
  web_view_->SetPreferredSize(
      gfx::Size(kBatchUploadDialogFixedWidth, kBatchUploadDialogMaxHeight));

  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  CHECK(identity_manager);
  primary_account_info_ = GetBatchUploadPrimaryAccountInfo(*identity_manager);

  BatchUploadUI* web_ui = GetBatchUploadUI(web_view_);
  CHECK(web_ui);
  // Initializes the UI that will initialize the handler when ready.
  web_ui->Initialize(
      primary_account_info_, std::move(data_containers_list),
      base::BindRepeating(&BatchUploadDialogView::SetHeightAndShowWidget,
                          base::Unretained(this)),
      base::BindOnce(&BatchUploadDialogView::OnDialogSelectionMade,
                     base::Unretained(this)));

  AddChildView(std::move(web_view));

  SetLayoutManager(std::make_unique<views::FillLayout>());

  // Observe Identity Manager for sign in state changes.
  scoped_identity_manager_observation_.Observe(identity_manager);
}

void BatchUploadDialogView::OnClose() {
  // `complete_callback_` will destroy data owned by the service/controller
  // passed to the UI and handler. We need to make sure those are cleared if the
  // UI is still alive, before clearing the service/controller.
  BatchUploadUI* web_ui = GetBatchUploadUI(web_view_);
  if (web_ui) {
    web_ui->Clear();
  }

  // If the view was closed without a user action, run the callback as if it was
  // cancelled (empty result).
  if (complete_callback_) {
    std::move(complete_callback_).Run({});
  }
}

void BatchUploadDialogView::OnDialogSelectionMade(
    const base::flat_map<BatchUploadDataType,
                         std::vector<BatchUploadDataItemModel::DataId>>&
        selected_map) {
  std::move(complete_callback_).Run(selected_map);
  GetWidget()->Close();
}

void BatchUploadDialogView::SetHeightAndShowWidget(int height) {
  views::Widget* widget = GetWidget();
  // This might happen if an update was requested after a close event happens
  // externally such as signing out or signin pending that triggers the closing
  // of the widget. The widget does not get destroyed right away but is in
  // closing mode.
  if (widget->IsClosed()) {
    return;
  }

  // Beyond `kBatchUploadDialogMaxHeight`, the dialog will show a scrollbar.
  web_view_->SetPreferredSize(
      gfx::Size(kBatchUploadDialogFixedWidth,
                std::min(height, kBatchUploadDialogMaxHeight)));
  widget->SetSize(widget->non_client_view()->GetPreferredSize());

  // `SetHeightAndShowWidget()` may be called multiple times, only show the view
  // and set the static values once.
  if (!widget->IsVisible()) {
    // Enforce the web view round corners to match the native view. Since we set
    // the view margin to 0 in the constructor, it leads to the webview
    // overlapping on the native view in the corners.
    web_view_->holder()->SetCornerRadii(
        gfx::RoundedCornersF(GetCornerRadius()));

    widget->Show();
  }
}

void BatchUploadDialogView::OnPrimaryAccountChanged(
    const signin::PrimaryAccountChangeEvent& event_details) {
  switch (event_details.GetEventTypeFor(signin::ConsentLevel::kSignin)) {
    case signin::PrimaryAccountChangeEvent::Type::kCleared:
      GetWidget()->Close();
      return;
    case signin::PrimaryAccountChangeEvent::Type::kNone:
    case signin::PrimaryAccountChangeEvent::Type::kSet:
      break;
  }
}

void BatchUploadDialogView::OnErrorStateOfRefreshTokenUpdatedForAccount(
    const CoreAccountInfo& account_info,
    const GoogleServiceAuthError& error,
    signin_metrics::SourceForRefreshTokenOperation token_operation_source) {
  if (account_info == primary_account_info_ && error.IsPersistentError()) {
    GetWidget()->Close();
  }
}

bool BatchUploadDialogView::HandleKeyboardEvent(
    content::WebContents* source,
    const input::NativeWebKeyboardEvent& event) {
  // TODO(crbug.com/373297250): Look into using
  // `views::UnhandledKeyboardEventHandler` to properly handle all
  // shortcut/unhandled keyboard events.
  if (event.dom_key == ui::DomKey::ESCAPE) {
    GetWidget()->Close();
    return true;
  }

  return false;
}

views::WebView* BatchUploadDialogView::GetWebViewForTesting() {
  return web_view_;
}

// BatchUploadUIDelegate -------------------------------------------------------

void BatchUploadUIDelegate::ShowBatchUploadDialogInternal(
    Browser& browser,
    std::vector<BatchUploadDataContainer> data_containers_list,
    SelectedDataTypeItemsCallback complete_callback) {
  BatchUploadDialogView::CreateBatchUploadDialogView(
      browser, std::move(data_containers_list), std::move(complete_callback));
}

BEGIN_METADATA(BatchUploadDialogView)
END_METADATA
