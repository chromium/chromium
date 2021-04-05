// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/download_shelf/download_shelf_page_handler.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/download/download_item_model.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/download_shelf/download_shelf_ui.h"
#include "components/download/public/common/download_item.h"
#include "content/public/browser/browser_context.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace {

download_shelf::mojom::DownloadItemPtr GetDownloadItemFromUIModel(
    DownloadUIModel* download_model) {
  download::DownloadItem* download = download_model->download();
  auto download_item = download_shelf::mojom::DownloadItem::New();
  download_item->allow_download_feedback =
      download_model->ShouldAllowDownloadFeedback();
  download_item->danger_type = download_model->GetDangerType();
  download_item->file_name_to_report_user =
      download_model->GetFileNameToReportUser();
  download_item->id = download->GetId();
  download_item->is_dangerous = download_model->IsDangerous();
  download_item->is_paused = download->IsPaused();
  download_item->is_malicious = download_model->IsMalicious();
  download_item->mixed_content_status = download_model->GetMixedContentStatus();
  download_item->original_url = download_model->GetOriginalURL();
  download_item->received_bytes = download->GetReceivedBytes();
  download_item->should_open_when_complete =
      download_model->GetOpenWhenComplete();
  download_item->should_promote_origin = download_model->ShouldPromoteOrigin();
  download_item->state = download->GetState();
  download_item->status_text =
      base::UTF16ToUTF8(download_model->GetStatusText());
  download_item->target_file_path = download_model->GetTargetFilePath();
  download_item->tooltip_text =
      base::UTF16ToUTF8(download_model->GetTooltipText());
  download_item->total_bytes = download->GetTotalBytes();
  download_item->warning_confirm_button_text =
      base::UTF16ToUTF8(download_model->GetWarningConfirmButtonText());

  return download_item;
}

}  // namespace

namespace download {
class DownloadItem;
}

DownloadShelfPageHandler::DownloadShelfPageHandler(
    mojo::PendingReceiver<download_shelf::mojom::PageHandler> receiver,
    mojo::PendingRemote<download_shelf::mojom::Page> page,
    content::WebUI* web_ui,
    DownloadShelfUI* download_shelf_ui)
    : receiver_(this, std::move(receiver)),
      page_(std::move(page)),
      download_shelf_ui_(download_shelf_ui) {}

DownloadShelfPageHandler::~DownloadShelfPageHandler() = default;

void DownloadShelfPageHandler::ShowContextMenu(uint32_t download_id,
                                               int32_t client_x,
                                               int32_t client_y) {
  download_shelf_ui_->ShowContextMenu(download_id, client_x, client_y);
}

void DownloadShelfPageHandler::DoShowDownload(DownloadUIModel* download_model) {
  page_->OnNewDownload(GetDownloadItemFromUIModel(download_model));
}
