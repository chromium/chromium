// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PROFILES_BATCH_UPLOAD_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PROFILES_BATCH_UPLOAD_DIALOG_VIEW_H_

#include "base/scoped_observation.h"
#include "chrome/browser/profiles/batch_upload/batch_upload_delegate.h"
#include "chrome/browser/profiles/batch_upload/batch_upload_service.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/sync/service/local_data_description.h"
#include "content/public/browser/web_contents_delegate.h"
#include "ui/views/window/dialog_delegate.h"

class Browser;

class BatchUploadDialogViewBrowserTest;

namespace views {
class WebView;
}  // namespace views

// Dialog closing reasons.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class BatchUploadDialogCloseReason {
  kSaveClicked = 0,
  kCancelClicked = 1,
  kDismissed = 2,
  kWindowClosed = 3,
  kSiginPending = 4,
  kSignout = 5,

  kMaxValue = kSignout,
};

// Native dialog view that holds the web ui component for the Batch Upload ui.
// It needs to adapt the height size based on the web ui content that is
// displayed, which is dynamic.
class BatchUploadDialogView : public views::DialogDelegateView,
                              public content::WebContentsDelegate,
                              public signin::IdentityManager::Observer {
  METADATA_HEADER(BatchUploadDialogView, views::DialogDelegateView)

 public:
  BatchUploadDialogView(const BatchUploadDialogView& other) = delete;
  BatchUploadDialogView& operator=(const BatchUploadDialogView& other) = delete;
  ~BatchUploadDialogView() override;

  // Creates the dialog view and registers as a modal view.
  // The created dialog view is owned by the views system.
  static BatchUploadDialogView* CreateBatchUploadDialogView(
      Browser& browser,
      std::vector<syncer::LocalDataDescription> local_data_description_list,
      BatchUploadService::EntryPoint entry_point,
      BatchUploadSelectedDataTypeItemsCallback complete_callback);

  views::WebView* GetWebViewForTesting();

 private:
  friend class BatchUploadDialogViewBrowserTest;

  FRIEND_TEST_ALL_PREFIXES(BatchUploadDialogViewBrowserTest,
                           OpenBatchUploadDialogViewWithCancelAction);
  FRIEND_TEST_ALL_PREFIXES(BatchUploadDialogViewBrowserTest,
                           OpenBatchUploadDialogViewWithDestroyed);
  FRIEND_TEST_ALL_PREFIXES(BatchUploadDialogViewBrowserTest,
                           OpenBatchUploadDialogViewWithSaveActionAllItems);
  FRIEND_TEST_ALL_PREFIXES(BatchUploadDialogViewBrowserTest,
                           OpenBatchUploadDialogViewWithSaveActionSomeItems);

  explicit BatchUploadDialogView(
      Browser& browser,
      std::vector<syncer::LocalDataDescription> local_data_description_list,
      BatchUploadService::EntryPoint entry_point,
      BatchUploadSelectedDataTypeItemsCallback complete_callback);

  // Callback to properly resize the view based on the loaded web ui content.
  // Also shows the widget.
  void SetHeightAndShowWidget(int height);

  // Callback to control whether the web content can receive inputs or not.
  void AllowWebViewInput(bool allow);

  // Callback to receive the selected items from the web ui view.
  // Empty list means the dialog was closed without a move item request.
  void OnDialogSelectionMade(
      const std::map<syncer::DataType,
                     std::vector<syncer::LocalDataItemModel::DataId>>&
          selected_map);

  // Callback used as a clearing method whenever the view is being closed. Used
  // to clear any data until the view is actually closed, given that closing the
  // view fully is asynchronous.
  void OnClose();

  // Closes the dialog and sets the closing reason to be recorded in the
  // destructor.
  void CloseWithReason(BatchUploadDialogCloseReason reason);

  // content::WebContentsDelegate:
  bool HandleKeyboardEvent(content::WebContents* source,
                           const input::NativeWebKeyboardEvent& event) override;

  // signin::IdentityManager::Observer:
  void OnPrimaryAccountChanged(
      const signin::PrimaryAccountChangeEvent& event_details) override;
  void OnErrorStateOfRefreshTokenUpdatedForAccount(
      const CoreAccountInfo& account_info,
      const GoogleServiceAuthError& error,
      signin_metrics::SourceForRefreshTokenOperation token_operation_source)
      override;

  // Account info for which the data is showing.
  AccountInfo primary_account_info_;
  BatchUploadSelectedDataTypeItemsCallback complete_callback_;
  BatchUploadService::EntryPoint entry_point_;

  raw_ptr<views::WebView> web_view_;

  // Count of items per data type. To be used for metrics purposes.
  std::map<syncer::DataType, int> data_item_count_map_;

  // When this value is set, ignore any input into `web_view_`s web contents.
  std::optional<content::WebContents::ScopedIgnoreInputEvents>
      scoped_ignore_events_;

  // Reason for closing the dialog. This value used to record a histogram when
  // the dialog is closed. Expected to be filled in `CloseWithReason()`.
  // Defaulted to `kWindowClosed` as this value cannot be deduced and only
  // happens if the dialog is closed without any user explicit action on the
  // dialog.
  BatchUploadDialogCloseReason close_reason_ =
      BatchUploadDialogCloseReason::kWindowClosed;

  base::ScopedObservation<signin::IdentityManager,
                          signin::IdentityManager::Observer>
      scoped_identity_manager_observation_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_PROFILES_BATCH_UPLOAD_DIALOG_VIEW_H_
