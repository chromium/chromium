// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_DATA_SHARING_DATA_SHARING_BUBBLE_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_DATA_SHARING_DATA_SHARING_BUBBLE_CONTROLLER_H_

#include "chrome/browser/ui/browser_user_data.h"
#include "chrome/browser/ui/views/bubble/webui_bubble_manager.h"
#include "chrome/browser/ui/views/data_sharing/data_sharing_utils.h"
#include "chrome/browser/ui/webui/data_sharing/data_sharing_ui.h"
#include "components/collaboration/public/collaboration_controller_delegate.h"
#include "components/data_sharing/public/group_data.h"
#include "components/saved_tab_groups/public/types.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/widget/widget_observer.h"

// Controller responsible for hosting the data sharing bubble per browser.
class DataSharingBubbleController
    : public BrowserUserData<DataSharingBubbleController>,
      public views::WidgetObserver,
      public DataSharingUI::Delegate {
 public:
  using OnCloseCallback = base::OnceCallback<void(
      std::optional<data_sharing::mojom::GroupAction> action,
      std::optional<data_sharing::mojom::GroupActionProgress> progress)>;

  DataSharingBubbleController(const DataSharingBubbleController&) = delete;
  DataSharingBubbleController& operator=(const DataSharingBubbleController&) =
      delete;
  ~DataSharingBubbleController() override;

  // `request_info` contains the values we want to pass into the loaded WebUI in
  // this bubble.
  void Show(data_sharing::RequestInfo request_info);
  // Closes the instance of the data sharing bubble.
  void Close();

  // Set a callback to invoke when the widget is closed.
  void SetOnCloseCallback(OnCloseCallback callback);

  // Set a callback to invoke when there's an error.
  void SetShowErrorDialogCallback(base::OnceCallback<void()> callback);

  void SetOnShareLinkRequestedCallback(
      collaboration::CollaborationControllerDelegate::
          ResultWithGroupTokenCallback callback);

  void OnUrlReadyToShare(GURL url);

  // views::WidgetObserver
  void OnWidgetClosing(views::Widget* widget) override;

  // DataSharingUI::Delegate
  void ApiInitComplete() override;
  void ShowErrorDialog(int status_code) override;
  void OnShareLinkRequested(
      const std::string& group_id,
      const std::string& access_token,
      base::OnceCallback<void(const std::optional<GURL>&)> callback) override;
  void OnGroupAction(
      data_sharing::mojom::GroupAction action,
      data_sharing::mojom::GroupActionProgress progress) override;

  base::WeakPtr<WebUIBubbleDialogView> BubbleViewForTesting() {
    return bubble_view_;
  }

  std::optional<data_sharing::mojom::GroupAction> group_action_for_testing()
      const {
    return group_action_;
  }

  std::optional<data_sharing::mojom::GroupActionProgress>
  group_action_progress_for_testing() const {
    return group_action_progress_;
  }

 private:
  friend class BrowserUserData<DataSharingBubbleController>;

  explicit DataSharingBubbleController(Browser* browser);

  base::ScopedObservation<views::Widget, views::WidgetObserver>
      bubble_widget_observation_{this};

  // Callback to invoke when the widget closes.
  OnCloseCallback on_close_callback_;

  // Callback to invoke when there's an error.
  base::OnceCallback<void()> on_error_callback_;

  // Callback passed from CollaborationService to invoke when user clicks on the
  // copy link button for the first time from share flow.
  collaboration::CollaborationControllerDelegate::ResultWithGroupTokenCallback
      on_share_link_requested_callback_;

  // Callback passed from mojom interface to invoke when share link is ready or
  // failed to share.
  base::OnceCallback<void(const std::optional<GURL>&)> share_link_callback_;

  // The latest group action received from Data Sharing SDK.
  std::optional<data_sharing::mojom::GroupAction> group_action_;

  // Progress of the latest group action received from Data Sharing SDK.
  std::optional<data_sharing::mojom::GroupActionProgress>
      group_action_progress_;

  base::WeakPtr<WebUIBubbleDialogView> bubble_view_;

  BROWSER_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_UI_VIEWS_DATA_SHARING_DATA_SHARING_BUBBLE_CONTROLLER_H_
