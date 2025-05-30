// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/data_sharing/data_sharing_bubble_controller.h"

#include "base/memory/raw_ptr.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/views/data_sharing/data_sharing_utils.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/top_container_view.h"
#include "chrome/browser/ui/views/tabs/tab_group_header.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/saved_tab_groups/public/types.h"
#include "components/tab_groups/tab_group_id.h"
#include "net/base/url_util.h"
#include "ui/views/view_class_properties.h"

namespace {

class DataSharingBubbleDialogView : public WebUIBubbleDialogView {
  METADATA_HEADER(DataSharingBubbleDialogView, WebUIBubbleDialogView)
 public:
  explicit DataSharingBubbleDialogView(
      Browser* browser,
      views::View* anchor_view,
      std::unique_ptr<WebUIContentsWrapper> contents_wrapper)
      : WebUIBubbleDialogView(anchor_view,
                              contents_wrapper->GetWeakPtr(),
                              std::nullopt,
                              views::BubbleBorder::Arrow::TOP_LEFT),
        contents_wrapper_(std::move(contents_wrapper)),
        browser_(browser) {}

  // WebUIContentsWrapper::Host override. Handle opening WebUI href links into
  // browser.
  content::WebContents* AddNewContents(
      content::WebContents* source,
      std::unique_ptr<content::WebContents> new_contents,
      const GURL& target_url,
      WindowOpenDisposition disposition,
      const blink::mojom::WindowFeatures& window_features,
      bool user_gesture,
      bool* was_blocked) override;

 private:
  std::unique_ptr<WebUIContentsWrapper> contents_wrapper_;
  raw_ptr<Browser> browser_;
};

content::WebContents* DataSharingBubbleDialogView::AddNewContents(
    content::WebContents* source,
    std::unique_ptr<content::WebContents> new_contents,
    const GURL& target_url,
    WindowOpenDisposition disposition,
    const blink::mojom::WindowFeatures& window_features,
    bool user_gesture,
    bool* was_blocked) {
  NavigateParams params(browser_, std::move(new_contents));
  params.tabstrip_index = browser_->tab_strip_model()->count();
  // Open link in a new window for better visibility because the bubble lays on
  // top of the current window.
  params.disposition = WindowOpenDisposition::NEW_WINDOW;
  Navigate(&params);
  return params.navigated_or_inserted_contents;
}

BEGIN_METADATA(DataSharingBubbleDialogView)
END_METADATA

views::View* GetAnchorViewForShare(const BrowserView* browser_view,
                                   tab_groups::LocalTabGroupID group_id) {
  if (!browser_view->tabstrip()) {
    return nullptr;
  }

  TabGroupHeader* const group_header =
      browser_view->tabstrip()->group_header(group_id);
  if (!group_header) {
    return nullptr;
  }

  return group_header;
}

}  // namespace

DataSharingBubbleController::~DataSharingBubbleController() = default;

void DataSharingBubbleController::Show(data_sharing::RequestInfo request_info) {
  if (bubble_view_) {
    return;
  }

  auto url =
      data_sharing::GenerateWebUIUrl(request_info, GetBrowser().profile());
  if (!url) {
    return;
  }

  std::string flow_value;
  CHECK(net::GetValueForKeyInQuery(url.value(), data_sharing::kQueryParamFlow,
                                   &flow_value));

  const BrowserView* const browser_view =
      BrowserView::GetBrowserViewForBrowser(&GetBrowser());

  views::View* anchor_view_for_share = nullptr;
  if (flow_value == data_sharing::kFlowShare) {
    anchor_view_for_share = GetAnchorViewForShare(
        browser_view, std::get<tab_groups::TabGroupId>(request_info.id));
    if (!anchor_view_for_share) {
      // The share bubble has nothing to anchor from; return early.
      return;
    }
  }

  auto contents_wrapper =
      std::make_unique<WebUIContentsWrapperT<DataSharingUI>>(
          url.value(), GetBrowser().profile(),
          IDS_DATA_SHARING_BUBBLE_DIALOG_TITLE,
          /*esc_closes_ui=*/true,
          /*supports_draggable_regions=*/false);
  contents_wrapper->GetWebUIController()->SetDelegate(this);

  auto bubble_view = std::make_unique<DataSharingBubbleDialogView>(
      &GetBrowser(), anchor_view_for_share, std::move(contents_wrapper));
  bubble_view->SetProperty(views::kElementIdentifierKey,
                           kDataSharingBubbleElementId);
  bubble_view_ = bubble_view->GetWeakPtr();

  if (flow_value == data_sharing::kFlowShare) {
    // Sharing flow uses a normal bubble.
    views::BubbleDialogDelegateView::CreateBubble(std::move(bubble_view));
  } else {
    // Manage and Join flow use modals. In this case the `anchor_view_for_share`
    // doesn't take effect.
    bubble_view->SetModalType(ui::mojom::ModalType::kWindow);
    constrained_window::CreateBrowserModalDialogViews(
        std::move(bubble_view), browser_view->GetNativeWindow());
  }

  views::Widget* widget = bubble_view_->GetWidget();
  CHECK(widget);
  bubble_widget_observation_.Observe(widget);
}

void DataSharingBubbleController::Close() {
  if (!bubble_view_) {
    return;
  }
  CHECK(bubble_view_->GetWidget());
  bubble_view_->GetWidget()->CloseWithReason(
      views::Widget::ClosedReason::kUnspecified);
  bubble_view_ = nullptr;
}

void DataSharingBubbleController::SetOnCloseCallback(OnCloseCallback callback) {
  on_close_callback_ = std::move(callback);
}

void DataSharingBubbleController::SetShowErrorDialogCallback(
    base::OnceCallback<void()> callback) {
  on_error_callback_ = std::move(callback);
}

void DataSharingBubbleController::SetOnShareLinkRequestedCallback(
    collaboration::CollaborationControllerDelegate::ResultWithGroupTokenCallback
        callback) {
  on_share_link_requested_callback_ = std::move(callback);
}

void DataSharingBubbleController::OnUrlReadyToShare(GURL url) {
  if (share_link_callback_) {
    std::move(share_link_callback_).Run(url);
  }
}

void DataSharingBubbleController::OnWidgetClosing(views::Widget* widget) {
  bubble_widget_observation_.Reset();
  if (on_share_link_requested_callback_) {
    std::move(on_share_link_requested_callback_)
        .Run(collaboration::CollaborationControllerDelegate::Outcome::kCancel,
             std::nullopt);
  }

  if (on_close_callback_) {
    std::move(on_close_callback_).Run(group_action_, group_action_progress_);
  }

  // Reset progress on dialog close.
  group_action_ = std::nullopt;
  group_action_progress_ = std::nullopt;
}

void DataSharingBubbleController::ApiInitComplete() {
  // No-op for this class.
}

void DataSharingBubbleController::ShowErrorDialog(int status_code) {
  if (share_link_callback_) {
    // On error case, if the share link callback is present, return an empty url
    // which will close the share dialog.
    std::move(share_link_callback_).Run(std::nullopt);
  }

  if (on_error_callback_) {
    std::move(on_error_callback_).Run();
  }
}

void DataSharingBubbleController::OnShareLinkRequested(
    const std::string& group_id,
    const std::string& access_token,
    base::OnceCallback<void(const std::optional<GURL>&)> callback) {
  if (on_share_link_requested_callback_) {
    share_link_callback_ = std::move(callback);
    std::move(on_share_link_requested_callback_)
        .Run(collaboration::CollaborationControllerDelegate::Outcome::kSuccess,
             data_sharing::GroupToken(data_sharing::GroupId(group_id),
                                      access_token));
  }
}

void DataSharingBubbleController::OnGroupAction(
    data_sharing::mojom::GroupAction action,
    data_sharing::mojom::GroupActionProgress progress) {
  group_action_ = action;
  group_action_progress_ = progress;
}

DataSharingBubbleController::DataSharingBubbleController(Browser* browser)
    : BrowserUserData<DataSharingBubbleController>(*browser) {}

BROWSER_USER_DATA_KEY_IMPL(DataSharingBubbleController);
