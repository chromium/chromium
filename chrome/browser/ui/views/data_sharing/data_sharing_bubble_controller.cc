// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/data_sharing/data_sharing_bubble_controller.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/views/data_sharing/data_sharing_utils.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/top_container_view.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/browser/ui/webui/data_sharing/data_sharing_ui.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "net/base/url_util.h"
#include "ui/views/view_class_properties.h"

namespace {

class DataSharingBubbleDialogView : public WebUIBubbleDialogView {
  METADATA_HEADER(DataSharingBubbleDialogView, WebUIBubbleDialogView)
 public:
  explicit DataSharingBubbleDialogView(
      views::View* anchor_view,
      std::unique_ptr<WebUIContentsWrapper> contents_wrapper)
      : WebUIBubbleDialogView(anchor_view,
                              contents_wrapper->GetWeakPtr(),
                              std::nullopt,
                              views::BubbleBorder::Arrow::TOP_LEFT),
        contents_wrapper_(std::move(contents_wrapper)) {}

 private:
  std::unique_ptr<WebUIContentsWrapper> contents_wrapper_;
};

BEGIN_METADATA(DataSharingBubbleDialogView)
END_METADATA

}  // namespace

DataSharingBubbleController::~DataSharingBubbleController() = default;

void DataSharingBubbleController::Show(
    std::variant<tab_groups::LocalTabGroupID, data_sharing::GroupToken>
        request_info) {
  if (bubble_view_) {
    return;
  }

  auto url =
      data_sharing::GenerateWebUIUrl(request_info, GetBrowser().profile());
  if (!url) {
    return;
  }
  auto contents_wrapper =
      std::make_unique<WebUIContentsWrapperT<DataSharingUI>>(
          url.value(), GetBrowser().profile(),
          IDS_DATA_SHARING_BUBBLE_DIALOG_TITLE,
          /*esc_closes_ui=*/true,
          /*supports_draggable_regions=*/false);

  std::string flow_value;
  bool flow_has_value = net::GetValueForKeyInQuery(
      url.value(), data_sharing::kQueryParamFlow, &flow_value);
  CHECK(flow_has_value);
  views::View* anchor_view_for_share = nullptr;
  const auto* const browser_view =
      BrowserView::GetBrowserViewForBrowser(&GetBrowser());

  if (flow_value == data_sharing::kFlowShare) {
    if (const auto* const tab_strip = browser_view->tabstrip()) {
      if (auto* const group_header =
              tab_strip->group_header(std::get<0>(request_info))) {
        anchor_view_for_share = group_header;
      }
    }
    // The share bubble should anchor to the tab group header according to the
    // design.
    if (!anchor_view_for_share) {
      return;
    }
  }

  auto bubble_view = std::make_unique<DataSharingBubbleDialogView>(
      anchor_view_for_share, std::move(contents_wrapper));
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

DataSharingBubbleController::DataSharingBubbleController(Browser* browser)
    : BrowserUserData<DataSharingBubbleController>(*browser) {}

BROWSER_USER_DATA_KEY_IMPL(DataSharingBubbleController);
