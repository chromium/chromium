// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/download/bubble/download_bubble_contents_view.h"

#include <utility>

#include "chrome/browser/download/bubble/download_bubble_prefs.h"
#include "chrome/browser/download/bubble/download_bubble_ui_controller.h"
#include "chrome/browser/download/download_item_warning_data.h"
#include "chrome/browser/safe_browsing/download_protection/download_protection_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/download/bubble/download_bubble_partial_view.h"
#include "chrome/browser/ui/views/download/bubble/download_bubble_primary_view.h"
#include "chrome/browser/ui/views/download/bubble/download_bubble_row_list_view.h"
#include "chrome/browser/ui/views/download/bubble/download_bubble_row_view.h"
#include "chrome/browser/ui/views/download/bubble/download_bubble_security_view.h"
#include "chrome/browser/ui/views/download/bubble/download_dialog_view.h"
#include "chrome/browser/ui/views/download/bubble/download_toolbar_button_view.h"
#include "components/offline_items_collection/core/offline_item.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/view.h"

using offline_items_collection::ContentId;

DownloadBubbleContentsView::DownloadBubbleContentsView(
    base::WeakPtr<Browser> browser,
    base::WeakPtr<DownloadBubbleUIController> bubble_controller,
    base::WeakPtr<DownloadBubbleNavigationHandler> navigation_handler,
    bool primary_view_is_partial_view,
    std::vector<DownloadUIModel::DownloadUIModelPtr> primary_view_models,
    views::BubbleDialogDelegate* bubble_delegate)
    : bubble_controller_(bubble_controller) {
  CHECK(!primary_view_models.empty());
  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical);

  std::unique_ptr<DownloadBubblePrimaryView> primary_view;
  if (primary_view_is_partial_view) {
    primary_view = std::make_unique<DownloadBubblePartialView>(
        browser, bubble_controller, navigation_handler,
        std::move(primary_view_models),
        base::BindOnce(&DownloadBubbleNavigationHandler::OnDialogInteracted,
                       navigation_handler));
  } else {
    primary_view = std::make_unique<DownloadDialogView>(
        browser, bubble_controller, navigation_handler,
        std::move(primary_view_models));
  }

  primary_view_ = AddChildView(std::move(primary_view));
  security_view_ = AddChildView(std::make_unique<DownloadBubbleSecurityView>(
      /*delegate=*/this, navigation_handler, bubble_delegate,
      download::IsDownloadBubbleV2Enabled(browser->profile())));

  // Starts on the primary page.
  ShowPrimaryPage();
}

DownloadBubbleContentsView::~DownloadBubbleContentsView() {
  security_view_->Reset();
}

DownloadBubbleRowView* DownloadBubbleContentsView::GetPrimaryViewRowForTesting(
    size_t index) {
  return primary_view_->GetRowForTesting(index);  // IN-TEST
}

DownloadBubbleRowView* DownloadBubbleContentsView::ShowPrimaryPage(
    absl::optional<offline_items_collection::ContentId> id) {
  CHECK(!id || *id != ContentId());
  security_view_->SetVisible(false);
  security_view_->Reset();
  page_ = Page::kPrimary;
  primary_view_->SetVisible(true);
  if (!id) {
    return nullptr;
  }
  if (DownloadBubbleRowView* row = primary_view_->GetRow(*id); row) {
    row->ScrollViewToVisible();
    return row;
  }
  return nullptr;
}

void DownloadBubbleContentsView::ShowSecurityPage(const ContentId& id) {
  CHECK(id != ContentId());
  primary_view_->SetVisible(false);
  page_ = Page::kSecurity;
  InitializeSecurityView(id);
  security_view_->UpdateAccessibilityTextAndFocus();
  security_view_->SetVisible(true);
}

DownloadBubbleContentsView::Page DownloadBubbleContentsView::VisiblePage()
    const {
  return page_;
}

void DownloadBubbleContentsView::InitializeSecurityView(const ContentId& id) {
  CHECK(id != ContentId());
  if (security_view_->content_id() == id) {
    return;
  }
  if (DownloadUIModel* model = GetDownloadModel(id); model) {
    security_view_->InitializeForDownload(*model);
    return;
  }
  NOTREACHED();
}

void DownloadBubbleContentsView::ProcessSecuritySubpageButtonPress(
    const offline_items_collection::ContentId& id,
    DownloadCommands::Command command) {
  CHECK(security_view_->IsInitialized());
  if (!bubble_controller_) {
    // If the bubble controller has gone away, close the dialog.
    return;
  }
  if (DownloadUIModel* model = GetDownloadModel(id); model) {
    bubble_controller_->ProcessDownloadButtonPress(model->GetWeakPtr(), command,
                                                   /*is_main_view=*/false);
  }
}

void DownloadBubbleContentsView::AddSecuritySubpageWarningActionEvent(
    const offline_items_collection::ContentId& id,
    DownloadItemWarningData::WarningAction action) {
  CHECK(security_view_->IsInitialized());
  if (DownloadUIModel* model = GetDownloadModel(id); model) {
    DownloadItemWarningData::AddWarningActionEvent(
        model->GetDownloadItem(),
        DownloadItemWarningData::WarningSurface::BUBBLE_SUBPAGE, action);
  }
}

void DownloadBubbleContentsView::ProcessDeepScanPress(
    const ContentId& id,
    base::optional_ref<const std::string> password) {
  if (DownloadUIModel* model = GetDownloadModel(id); model) {
    safe_browsing::DownloadProtectionService::UploadForConsumerDeepScanning(
        model->GetDownloadItem(), password);
  }
}

bool DownloadBubbleContentsView::IsEncryptedArchive(const ContentId& id) {
  if (DownloadUIModel* model = GetDownloadModel(id); model) {
    return DownloadItemWarningData::IsEncryptedArchive(
        model->GetDownloadItem());
  }

  return false;
}

bool DownloadBubbleContentsView::HasPreviousIncorrectPassword(
    const ContentId& id) {
  if (DownloadUIModel* model = GetDownloadModel(id); model) {
    return DownloadItemWarningData::HasIncorrectPassword(
        model->GetDownloadItem());
  }

  return false;
}

DownloadUIModel* DownloadBubbleContentsView::GetDownloadModel(
    const ContentId& id) {
  if (DownloadBubbleRowView* row = primary_view_->GetRow(id); row) {
    return row->model();
  }
  return nullptr;
}

BEGIN_METADATA(DownloadBubbleContentsView, views::View)
END_METADATA
