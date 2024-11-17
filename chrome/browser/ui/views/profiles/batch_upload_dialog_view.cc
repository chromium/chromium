// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/batch_upload_dialog_view.h"

#include "base/functional/callback.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
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

constexpr char kDataTypeInformationHistogramBase[] =
    "Sync.BatchUpload.DataType";

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

// Records the availability of all data types.
void RecordAvailableDataTypes(
    const std::map<syncer::DataType, int>& data_item_count) {
  for (const auto& [type, count] : data_item_count) {
    CHECK_NE(count, 0);
    base::UmaHistogramEnumeration(
        base::StrCat({kDataTypeInformationHistogramBase, "Available"}),
        DataTypeHistogramValue(type));
  }
}

// Records whether at least one element was selected per data type. Will not
// record for a specific data type if it is not in the map or has no items
// selected. Also records the percentage of selected items vs available items.
// Returns whether any data was selected.
bool RecordSelectedDataTypes(
    const std::map<syncer::DataType,
                   std::vector<syncer::LocalDataItemModel::DataId>>&
        selected_types,
    const std::map<syncer::DataType, int>& data_item_count_map) {
  bool has_selected_data = false;
  for (const auto& [type, selected_items] : selected_types) {
    int selected_count = selected_items.size();
    if (selected_count != 0) {
      has_selected_data = true;
      base::UmaHistogramEnumeration(
          base::StrCat({kDataTypeInformationHistogramBase, "Selected"}),
          DataTypeHistogramValue(type));

      int available_count = data_item_count_map.at(type);
      CHECK_LE(selected_count, available_count);
      base::UmaHistogramPercentage(
          base::StrCat(
              {kDataTypeInformationHistogramBase, "SelectedItemPercentage"}),
          selected_count * 100 / available_count);
    }
  }
  return has_selected_data;
}

}  // namespace

BatchUploadDialogView::~BatchUploadDialogView() {
  // Makes sure that everything is cleaned up if it was not done before.
  OnClose();

  // Records at view destruction to make sure it is recorded once only per
  // dialog.
  base::UmaHistogramEnumeration("Sync.BatchUpload.DialogCloseReason",
                                close_reason_);
}

// static
BatchUploadDialogView* BatchUploadDialogView::CreateBatchUploadDialogView(
    Browser& browser,
    std::vector<syncer::LocalDataDescription> local_data_description_list,
    BatchUploadService::EntryPoint entry_point,
    BatchUploadSelectedDataTypeItemsCallback complete_callback) {
  std::unique_ptr<BatchUploadDialogView> dialog_view = base::WrapUnique(
      new BatchUploadDialogView(browser, std::move(local_data_description_list),
                                entry_point, std::move(complete_callback)));
  BatchUploadDialogView* dialog_view_ptr = dialog_view.get();

  gfx::NativeWindow window = browser.tab_strip_model()
                                 ->GetActiveWebContents()
                                 ->GetTopLevelNativeWindow();

  constrained_window::CreateBrowserModalDialogViews(std::move(dialog_view),
                                                    window);
  return dialog_view_ptr;
}

BatchUploadDialogView::BatchUploadDialogView(
    Browser& browser,
    std::vector<syncer::LocalDataDescription> local_data_description_list,
    BatchUploadService::EntryPoint entry_point,
    BatchUploadSelectedDataTypeItemsCallback complete_callback)
    : complete_callback_(std::move(complete_callback)),
      entry_point_(entry_point) {
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
      std::make_unique<views::WebView>(browser.profile());
  web_view->LoadInitialURL(GURL(chrome::kChromeUIBatchUploadURL));
  web_view_ = web_view.get();
  web_view_->GetWebContents()->SetDelegate(this);
  SetInitiallyFocusedView(web_view_);
  // Set initial height to max height in order not to have an empty window.
  web_view_->SetPreferredSize(
      gfx::Size(kBatchUploadDialogFixedWidth, kBatchUploadDialogMaxHeight));

  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(browser.profile());
  CHECK(identity_manager);
  primary_account_info_ = GetBatchUploadPrimaryAccountInfo(*identity_manager);

  for (const syncer::LocalDataDescription& local_data_description :
       local_data_description_list) {
    const int item_count = local_data_description.local_data_models.size();
    CHECK_NE(item_count, 0);
    data_item_count_map_.insert_or_assign(local_data_description.type,
                                          item_count);
  }

  BatchUploadUI* web_ui = GetBatchUploadUI(web_view_);
  CHECK(web_ui);
  // Initializes the UI that will initialize the handler when ready.
  web_ui->Initialize(
      primary_account_info_, &browser, std::move(local_data_description_list),
      base::BindRepeating(&BatchUploadDialogView::SetHeightAndShowWidget,
                          base::Unretained(this)),
      base::BindRepeating(&BatchUploadDialogView::AllowWebViewInput,
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
    const std::map<syncer::DataType,
                   std::vector<syncer::LocalDataItemModel::DataId>>&
        selected_map) {
  bool has_selected_data =
      RecordSelectedDataTypes(selected_map, data_item_count_map_);

  std::move(complete_callback_).Run(selected_map);
  CloseWithReason(has_selected_data
                      ? BatchUploadDialogCloseReason::kSaveClicked
                      : BatchUploadDialogCloseReason::kCancelClicked);
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

    RecordAvailableDataTypes(data_item_count_map_);
    base::UmaHistogramEnumeration("Sync.BatchUpload.Opened", entry_point_);
  }
}

void BatchUploadDialogView::AllowWebViewInput(bool allow) {
  if (allow) {
    scoped_ignore_events_.reset();
    return;
  }

  scoped_ignore_events_ =
      web_view_->GetWebContents()->IgnoreInputEvents(std::nullopt);
}

void BatchUploadDialogView::OnPrimaryAccountChanged(
    const signin::PrimaryAccountChangeEvent& event_details) {
  switch (event_details.GetEventTypeFor(signin::ConsentLevel::kSignin)) {
    case signin::PrimaryAccountChangeEvent::Type::kCleared:
      CloseWithReason(BatchUploadDialogCloseReason::kSignout);
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
    CloseWithReason(BatchUploadDialogCloseReason::kSiginPending);
  }
}

bool BatchUploadDialogView::HandleKeyboardEvent(
    content::WebContents* source,
    const input::NativeWebKeyboardEvent& event) {
  // TODO(crbug.com/373297250): Look into using
  // `views::UnhandledKeyboardEventHandler` to properly handle all
  // shortcut/unhandled keyboard events.
  if (event.dom_key == ui::DomKey::ESCAPE) {
    CloseWithReason(BatchUploadDialogCloseReason::kDismissed);
    return true;
  }

  return false;
}

void BatchUploadDialogView::CloseWithReason(
    BatchUploadDialogCloseReason reason) {
  close_reason_ = reason;
  GetWidget()->Close();
}

views::WebView* BatchUploadDialogView::GetWebViewForTesting() {
  return web_view_;
}

// BatchUploadUIDelegate -------------------------------------------------------

void BatchUploadUIDelegate::ShowBatchUploadDialogInternal(
    Browser& browser,
    std::vector<syncer::LocalDataDescription> local_data_description_list,
    BatchUploadService::EntryPoint entry_point,
    BatchUploadSelectedDataTypeItemsCallback complete_callback) {
  BatchUploadDialogView::CreateBatchUploadDialogView(
      browser, std::move(local_data_description_list), entry_point,
      std::move(complete_callback));
}

BEGIN_METADATA(BatchUploadDialogView)
END_METADATA
