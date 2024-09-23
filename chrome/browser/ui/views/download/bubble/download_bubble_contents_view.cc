// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/download/bubble/download_bubble_contents_view.h"

#include <utility>

#include "chrome/browser/browser_process.h"
#include "chrome/browser/download/bubble/download_bubble_prefs.h"
#include "chrome/browser/download/bubble/download_bubble_ui_controller.h"
#include "chrome/browser/download/chrome_download_manager_delegate.h"
#include "chrome/browser/download/download_core_service.h"
#include "chrome/browser/download/download_core_service_factory.h"
#include "chrome/browser/download/download_item_warning_data.h"
#include "chrome/browser/safe_browsing/download_protection/download_protection_service.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/download/download_bubble_info.h"
#include "chrome/browser/ui/views/download/bubble/download_bubble_partial_view.h"
#include "chrome/browser/ui/views/download/bubble/download_bubble_primary_view.h"
#include "chrome/browser/ui/views/download/bubble/download_bubble_row_list_view.h"
#include "chrome/browser/ui/views/download/bubble/download_bubble_row_view.h"
#include "chrome/browser/ui/views/download/bubble/download_bubble_security_view.h"
#include "chrome/browser/ui/views/download/bubble/download_dialog_view.h"
#include "chrome/browser/ui/views/download/bubble/download_toolbar_button_view.h"
#include "components/offline_items_collection/core/offline_item.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "content/public/browser/download_item_utils.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"

using offline_items_collection::ContentId;

namespace {

void MaybeSendDownloadReport(content::BrowserContext* browser_context,
                             download::DownloadItem* download) {
  if (safe_browsing::SafeBrowsingService* service =
          g_browser_process->safe_browsing_service()) {
    service->SendDownloadReport(download,
                                safe_browsing::ClientSafeBrowsingReportRequest::
                                    DANGEROUS_DOWNLOAD_RECOVERY,
                                /*did_proceed=*/true,
                                /*show_download_in_folder=*/std::nullopt);
  }
}

}  // namespace

DownloadBubbleContentsView::DownloadBubbleContentsView(
    base::WeakPtr<Browser> browser,
    base::WeakPtr<DownloadBubbleUIController> bubble_controller,
    base::WeakPtr<DownloadBubbleNavigationHandler> navigation_handler,
    bool primary_view_is_partial_view,
    std::unique_ptr<DownloadBubbleContentsViewInfo> info,
    views::BubbleDialogDelegate* bubble_delegate)
    : info_(std::move(info)),
      bubble_controller_(bubble_controller),
      navigation_handler_(navigation_handler),
      bubble_delegate_(bubble_delegate) {
  SetProperty(views::kElementIdentifierKey, kToolbarDownloadBubbleElementId);
  CHECK(!info_->row_list_view_info().rows().empty());
  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical);

  std::unique_ptr<DownloadBubblePrimaryView> primary_view;
  if (primary_view_is_partial_view) {
    primary_view = std::make_unique<DownloadBubblePartialView>(
        browser, bubble_controller, navigation_handler,
        info_->row_list_view_info(),
        base::BindOnce(&DownloadBubbleNavigationHandler::OnDialogInteracted,
                       navigation_handler));
  } else {
    primary_view = std::make_unique<DownloadDialogView>(
        browser, bubble_controller, navigation_handler,
        info_->row_list_view_info());
  }

  primary_view_ = AddChildView(std::move(primary_view));
  security_view_ = AddChildView(std::make_unique<DownloadBubbleSecurityView>(
      /*delegate=*/this, info_->security_view_info(), navigation_handler,
      bubble_delegate));

  // Starts on the primary page.
  ShowPrimaryPage();

  bubble_delegate->SetInitiallyFocusedView(
      primary_view_->GetInitiallyFocusedView());
}

DownloadBubbleContentsView::~DownloadBubbleContentsView() {
  if (VisiblePage() == Page::kSecurity) {
    security_view_->MaybeLogDismiss();
  }
  security_view_->Reset();
  // In order to ensure that `info_` is valid for the entire lifetime of the
  // child views, we delete the child views here rather than in `~View()`.
  primary_view_ = nullptr;
  security_view_ = nullptr;
  RemoveAllChildViews();
}

DownloadBubbleRowView* DownloadBubbleContentsView::GetPrimaryViewRowForTesting(
    size_t index) {
  return primary_view_->GetRowForTesting(index);  // IN-TEST
}

DownloadBubbleRowView* DownloadBubbleContentsView::ShowPrimaryPage(
    std::optional<offline_items_collection::ContentId> id) {
  CHECK(!id || *id != ContentId());
  security_view_->SetVisible(false);
  security_view_->Reset();
  info_->ResetSecurityView();
  // Reset fixed width, which could be previously set by the security
  // view.
  bubble_delegate_->set_fixed_width(0);
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
  security_view_->SetVisible(true);
}

DownloadBubbleContentsView::Page DownloadBubbleContentsView::VisiblePage()
    const {
  return page_;
}

void DownloadBubbleContentsView::InitializeSecurityView(const ContentId& id) {
  info_->InitializeSecurityView(id);
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
    // Calling this before because ProcessDownloadButtonPress may cause
    // the model item to be deleted during its call.
    if (navigation_handler_) {
      navigation_handler_->OnSecurityDialogButtonPress(*model, command);
    }
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
    DownloadItemWarningData::DeepScanTrigger trigger,
    base::optional_ref<const std::string> password) {
  if (DownloadUIModel* model = GetDownloadModel(id); model) {
    LogDeepScanEvent(model->GetDownloadItem(),
                     safe_browsing::DeepScanEvent::kPromptAccepted);
    DownloadItemWarningData::AddWarningActionEvent(
        model->GetDownloadItem(),
        DownloadItemWarningData::WarningSurface::BUBBLE_SUBPAGE,
        DownloadItemWarningData::WarningAction::ACCEPT_DEEP_SCAN);
    safe_browsing::DownloadProtectionService::UploadForConsumerDeepScanning(
        model->GetDownloadItem(), trigger, password);
  }
}

void DownloadBubbleContentsView::ProcessLocalDecryptionPress(
    const offline_items_collection::ContentId& id,
    base::optional_ref<const std::string> password) {
  if (DownloadUIModel* model = GetDownloadModel(id); model) {
    LogLocalDecryptionEvent(safe_browsing::DeepScanEvent::kPromptAccepted);
    safe_browsing::DownloadProtectionService::CheckDownloadWithLocalDecryption(
        model->GetDownloadItem(), password);
  }
}

void DownloadBubbleContentsView::ProcessLocalPasswordInProgressClick(
    const offline_items_collection::ContentId& id,
    DownloadCommands::Command command) {
  DownloadUIModel* model = GetDownloadModel(id);
  if (!model) {
    return;
  }

  download::DownloadItem* item = model->GetDownloadItem();
  safe_browsing::SafeBrowsingService* sb_service =
      g_browser_process->safe_browsing_service();
  if (!sb_service) {
    return;
  }
  safe_browsing::DownloadProtectionService* protection_service =
      sb_service->download_protection_service();
  if (!protection_service) {
    return;
  }

  protection_service->CancelChecksForDownload(item);

  content::BrowserContext* browser_context =
      content::DownloadItemUtils::GetBrowserContext(item);
  DownloadCoreService* download_core_service =
      DownloadCoreServiceFactory::GetForBrowserContext(browser_context);

  DCHECK(download_core_service);
  ChromeDownloadManagerDelegate* delegate =
      download_core_service->GetDownloadManagerDelegate();
  DCHECK(delegate);

  if (command == DownloadCommands::CANCEL) {
    LogLocalDecryptionEvent(safe_browsing::DeepScanEvent::kScanCanceled);
    delegate->CheckClientDownloadDone(
        item->GetId(),
        safe_browsing::DownloadCheckResult::PROMPT_FOR_LOCAL_PASSWORD_SCANNING);
  } else if (command == DownloadCommands::BYPASS_DEEP_SCANNING) {
    LogLocalDecryptionEvent(safe_browsing::DeepScanEvent::kPromptBypassed);
    MaybeSendDownloadReport(browser_context, item);
    delegate->CheckClientDownloadDone(
        item->GetId(), safe_browsing::DownloadCheckResult::UNKNOWN);
  } else {
    NOTREACHED_IN_MIGRATION()
        << "Unexpected command: " << static_cast<int>(command);
  }
}

bool DownloadBubbleContentsView::IsEncryptedArchive(const ContentId& id) {
  if (DownloadUIModel* model = GetDownloadModel(id); model) {
    return DownloadItemWarningData::IsTopLevelEncryptedArchive(
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
  return info_->GetDownloadModel(id);
}

BEGIN_METADATA(DownloadBubbleContentsView)
END_METADATA
