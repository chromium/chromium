// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/data_sharing/data_sharing_bubble_controller.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/top_container_view.h"
#include "chrome/browser/ui/webui/data_sharing/data_sharing_ui.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "ui/views/view_class_properties.h"

namespace {

class DataSharingBubbleDialogView : public WebUIBubbleDialogView {
  METADATA_HEADER(DataSharingBubbleDialogView, WebUIBubbleDialogView)
 public:
  explicit DataSharingBubbleDialogView(
      views::View* anchor_view,
      std::unique_ptr<WebUIContentsWrapper> contents_wrapper)
      : WebUIBubbleDialogView(anchor_view, contents_wrapper->GetWeakPtr()),
        contents_wrapper_(std::move(contents_wrapper)) {}

 private:
  std::unique_ptr<WebUIContentsWrapper> contents_wrapper_;
};

BEGIN_METADATA(DataSharingBubbleDialogView)
END_METADATA

}  // namespace

DataSharingBubbleController::~DataSharingBubbleController() = default;

void DataSharingBubbleController::Show() {
  if (bubble_view_) {
    return;
  }

  auto contents_wrapper =
      std::make_unique<WebUIContentsWrapperT<DataSharingUI>>(
          GURL(chrome::kChromeUIUntrustedDataSharingURL),
          GetBrowser().profile(), IDS_DATA_SHARING_BUBBLE_DIALOG_TITLE,
          /*webui_resizes_host=*/true,
          /*esc_closes_ui=*/true,
          /*supports_draggable_regions=*/false);

  auto bubble_view = std::make_unique<DataSharingBubbleDialogView>(
      BrowserView::GetBrowserViewForBrowser(&GetBrowser())->top_container(),
      std::move(contents_wrapper));
  bubble_view->SetProperty(views::kElementIdentifierKey,
                           kDataSharingBubbleElementId);
  bubble_view_ = bubble_view->GetWeakPtr();
  views::BubbleDialogDelegateView::CreateBubble(std::move(bubble_view));
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
