// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/download_shelf/download_shelf_ui.h"

#include "base/location.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/download/download_commands.h"
#include "chrome/browser/ui/webui/download_shelf/download_shelf_page_handler.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/download_shelf_resources.h"
#include "chrome/grit/download_shelf_resources_map.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui_data_source.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"

namespace {

bool isDownloading(DownloadItem* download) {
  return !download->IsPaused() && download->PercentComplete() != 100;
}

}  // namespace

DownloadShelfUI::DownloadShelfUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui, true),
      progress_timer_(std::make_unique<base::RetainingOneShotTimer>(
          FROM_HERE,
          base::Milliseconds(30),
          base::BindRepeating(&DownloadShelfUI::NotifyDownloadProgress,
                              base::Unretained(this)))),
      download_manager_(Profile::FromWebUI(web_ui)->GetDownloadManager()),
      webui_load_timer_(web_ui->GetWebContents(),
                        "Download.Shelf.WebUI.LoadDocumentTime",
                        "Download.Shelf.WebUI.LoadCompletedTime") {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUIDownloadShelfHost);
  static constexpr webui::LocalizedString kStrings[] = {
      {"close", IDS_ACCNAME_CLOSE},
      {"discardButtonText", IDS_DISCARD_DOWNLOAD},
      {"downloadStatusOpeningText", IDS_DOWNLOAD_STATUS_OPENING},
      {"showAll", IDS_SHOW_ALL_DOWNLOADS}};
  source->AddLocalizedStrings(kStrings);

  webui::SetupWebUIDataSource(
      source,
      base::make_span(kDownloadShelfResources, kDownloadShelfResourcesSize),
      IDR_DOWNLOAD_SHELF_DOWNLOAD_SHELF_HTML);
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::TrustedTypes,
      "trusted-types static-types;");
  content::WebUIDataSource::Add(web_ui->GetWebContents()->GetBrowserContext(),
                                source);
}

DownloadShelfUI::~DownloadShelfUI() {
  // The destructor can take place before DownloadItem calls to
  // OnDownloadDestroyed.
  for (const auto& download_entry : items_)
    download_entry.second->download()->RemoveObserver(this);
}

WEB_UI_CONTROLLER_TYPE_IMPL(DownloadShelfUI)

void DownloadShelfUI::BindInterface(
    mojo::PendingReceiver<download_shelf::mojom::PageHandlerFactory> receiver) {
  page_factory_receiver_.reset();
  page_factory_receiver_.Bind(std::move(receiver));
}

void DownloadShelfUI::CreatePageHandler(
    mojo::PendingRemote<download_shelf::mojom::Page> page,
    mojo::PendingReceiver<download_shelf::mojom::PageHandler> receiver) {
  page_handler_ = std::make_unique<DownloadShelfPageHandler>(
      std::move(receiver), std::move(page), this);
}

void DownloadShelfUI::DoClose() {
  if (embedder())
    embedder()->DoClose();
}

void DownloadShelfUI::DoShowAll() {
  if (embedder())
    embedder()->DoShowAll();
}

void DownloadShelfUI::DiscardDownload(uint32_t download_id) {
  DownloadUIModel* download_ui_model = FindDownloadById(download_id);
  // WebUI's view is updated asynchronously via Mojo IPC, so the
  // corresponding C++ DownloadUIModel might already be gone due
  // to races with other UI surfaces.
  if (!download_ui_model)
    return;

  DownloadCommands(download_ui_model->GetWeakPtr())
      .ExecuteCommand(DownloadCommands::DISCARD);
}

void DownloadShelfUI::KeepDownload(uint32_t download_id) {
  DownloadUIModel* download_ui_model = FindDownloadById(download_id);
  // WebUI's view is updated asynchronously via Mojo IPC, so the
  // corresponding C++ DownloadUIModel might already be gone due
  // to races with other UI surfaces.
  if (!download_ui_model)
    return;

  DownloadCommands(download_ui_model->GetWeakPtr())
      .ExecuteCommand(DownloadCommands::KEEP);
}

void DownloadShelfUI::ShowContextMenu(
    uint32_t download_id,
    int32_t client_x,
    int32_t client_y,
    base::OnceClosure on_menu_will_show_callback) {
  DownloadUIModel* download_ui_model = FindDownloadById(download_id);
  // WebUI's view is updated asynchronously via Mojo IPC, so the
  // corresponding C++ DownloadUIModel might already be gone due
  // to races with other UI surfaces.
  if (!download_ui_model)
    return;

  if (embedder()) {
    embedder()->ShowDownloadContextMenu(download_ui_model,
                                        gfx::Point(client_x, client_y),
                                        std::move(on_menu_will_show_callback));
  }
}

void DownloadShelfUI::OpenDownload(uint32_t download_id) {
  DownloadUIModel* download_ui_model = FindDownloadById(download_id);
  // DownloadUIModel can be updated/removed from somewhere else, e.g extension
  // API or chrome://downloads, checking if download_ui_model exists makes it
  // safer for edges cases such as a download item is removed during a mojo
  // IPC call.
  if (!download_ui_model)
    return;
  download_ui_model->OpenDownload();
}

void DownloadShelfUI::DoShowDownload(
    DownloadUIModel::DownloadUIModelPtr download_model,
    base::Time show_download_start_time) {
  DownloadUIModel* download = AddDownload(std::move(download_model));
  show_download_time_map_.insert_or_assign(download->download()->GetId(),
                                           show_download_start_time);
  // Observe any changes on the download item in order to propagate such changes
  // to the UI.
  download->download()->AddObserver(this);
  progress_timer_->Reset();

  if (page_handler_)
    page_handler_->DoShowDownload(download);
}

std::vector<DownloadUIModel*> DownloadShelfUI::GetDownloads() {
  std::vector<DownloadUIModel*> downloads;
  for (const auto& download_entry : items_) {
    DownloadUIModel* download_model = download_entry.second.get();
    if (download_model->ShouldShowInShelf())
      downloads.push_back(download_model);
  }

  return downloads;
}

base::Time DownloadShelfUI::GetShowDownloadTime(uint32_t download_id) {
  return show_download_time_map_[download_id];
}

void DownloadShelfUI::RemoveDownload(uint32_t download_id) {
  DownloadUIModel* download_ui_model = items_.at(download_id).get();
  download_ui_model->download()->RemoveObserver(this);
  items_.erase(download_id);

  if (show_download_time_map_.count(download_id)) {
    show_download_time_map_.erase(download_id);
  }
}

void DownloadShelfUI::OnDownloadOpened(DownloadItem* download) {
  if (page_handler_)
    page_handler_->OnDownloadOpened(download->GetId());
}

void DownloadShelfUI::OnDownloadUpdated(DownloadItem* download) {
  if (page_handler_) {
    DownloadUIModel* download_model = FindDownloadById(download->GetId());
    DCHECK(download_model);

    if (!download_model->ShouldShowInShelf()) {
      page_handler_->OnDownloadErased(download->GetId());
      return;
    }

    page_handler_->OnDownloadUpdated(download_model);
  }

  if (isDownloading(download) && !progress_timer_->IsRunning())
    progress_timer_->Reset();
}

void DownloadShelfUI::OnDownloadRemoved(DownloadItem* download) {
  if (page_handler_)
    page_handler_->OnDownloadErased(download->GetId());
}

void DownloadShelfUI::OnDownloadDestroyed(DownloadItem* download) {
  download->RemoveObserver(this);
  items_.erase(download->GetId());
}

DownloadUIModel* DownloadShelfUI::AddDownload(
    DownloadUIModel::DownloadUIModelPtr download) {
  DownloadUIModel* pointer = download.get();
  items_.insert_or_assign(download->download()->GetId(), std::move(download));
  return pointer;
}

DownloadUIModel* DownloadShelfUI::FindDownloadById(uint32_t download_id) const {
  return items_.count(download_id) ? items_.at(download_id).get() : nullptr;
}

void DownloadShelfUI::NotifyDownloadProgress() {
  bool download_in_progress = false;
  for (const auto& download_entry : items_) {
    DownloadUIModel* download_model = download_entry.second.get();
    if (isDownloading(download_model->download())) {
      download_in_progress = true;
      // TODO(romanarora): Optimize by introducing a new OnDownloadProgress()
      // method.
      if (page_handler_)
        page_handler_->OnDownloadUpdated(download_model);
    }
  }

  if (download_in_progress)
    progress_timer_->Reset();
}

void DownloadShelfUI::SetPageHandlerForTesting(
    std::unique_ptr<DownloadShelfHandler> page_handler) {
  page_handler_ = std::move(page_handler);
}

void DownloadShelfUI::SetProgressTimerForTesting(
    std::unique_ptr<base::RetainingOneShotTimer> timer) {
  progress_timer_ = std::move(timer);
  progress_timer_->Start(
      FROM_HERE, base::Milliseconds(30),
      base::BindRepeating(&DownloadShelfUI::NotifyDownloadProgress,
                          base::Unretained(this)));
}
