// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chrome_finds_internals/chrome_finds_internals_ui.h"

#include <algorithm>

#include "base/json/json_writer.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/stringprintf.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/values.h"
#include "chrome/browser/finds/core/finds_features.h"
#include "chrome/browser/finds/core/finds_service.h"
#include "chrome/browser/finds/finds_service_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/chrome_finds_internals_resources.h"
#include "chrome/grit/chrome_finds_internals_resources_map.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_types.h"
// Use raw_ptr from base/memory/raw_ptr.h which is already included.
#include "content/public/browser/web_ui_data_source.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/webui/webui_util.h"

namespace chrome_finds_internals {

namespace {
constexpr int kMaxHistoryEntries = 500;
}  // namespace

class PageHandlerImpl : public mojom::PageHandler {
 public:
  PageHandlerImpl(mojo::PendingReceiver<mojom::PageHandler> receiver,
                  mojo::PendingRemote<mojom::Page> page,
                  Profile* profile)
      : receiver_(this, std::move(receiver)),
        page_(std::move(page)),
        profile_(profile) {
    history_service_ = HistoryServiceFactory::GetForProfile(
        profile_, ServiceAccessType::EXPLICIT_ACCESS);
    finds_service_ = finds::FindsServiceFactory::GetForProfile(profile_);
  }

  ~PageHandlerImpl() override = default;

  PageHandlerImpl(const PageHandlerImpl&) = delete;
  PageHandlerImpl& operator=(const PageHandlerImpl&) = delete;

  // mojom::PageHandler:
  void GetFindsServiceModelResponse() override {
    page_->LogMessageAdded("Running FindsService Model...");

    if (!finds_service_) {
      page_->LogMessageAdded("Error: FindsService not available.");
      return;
    }

    finds_service_->ExecuteModelAndScheduleNotification(
        base::BindOnce(&PageHandlerImpl::OnModelResponseComplete,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  void TriggerFindsTestNotification() override {
    page_->LogMessageAdded("Triggering test notification via FindsService...");

    if (!finds_service_) {
      page_->LogMessageAdded("Error: FindsService not available.");
      return;
    }

    bool success = finds_service_->ScheduleNotificationForInternalsPage();
    page_->LogMessageAdded(
        success ? "Test notification scheduled successfully. If the "
                  "notification is not showing up, ensure you have the "
                  "finds notification channel enabled and the bypass "
                  "cooldowns feature param on."
                : "Failed to schedule test notification.");
  }

  void GetHistoryJson(int32_t history_count,
                      GetHistoryJsonCallback callback) override {
    if (history_count <= 0) {
      std::move(callback).Run("[]");
      return;
    }

    if (!history_service_) {
      page_->LogMessageAdded("Error: HistoryService not available.");
      std::move(callback).Run("{}");
      return;
    }

    page_->LogMessageAdded(
        base::StringPrintf("Querying history (count: %d)...", history_count));

    history::QueryOptions options;
    options.max_count = std::min(history_count, kMaxHistoryEntries);
    history_service_->QueryHistory(
        std::u16string(), options,
        base::BindOnce(&PageHandlerImpl::OnHistoryQueryComplete,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)),
        &history_task_tracker_);
  }

 private:
  void OnModelResponseComplete(finds::FindsService::Result result) {
    page_->LogMessageAdded(base::StringPrintf("Result status: %d",
                                              static_cast<int>(result.status)));
    if (result.status != finds::FindsService::Result::Status::kSuccess) {
      page_->LogMessageAdded(result.message);
      return;
    }
    if (!result.message.empty()) {
      page_->LogMessageAdded(result.message);
    }
  }

  void OnHistoryQueryComplete(GetHistoryJsonCallback callback,
                              history::QueryResults results) {
    page_->LogMessageAdded(base::StringPrintf(
        "History query complete. Found %zu results.", results.size()));

    base::ListValue history_list;
    for (const auto& result : results) {
      base::DictValue entry;
      entry.Set("title", result.title());
      entry.Set("url", result.url().spec());
      entry.Set("visit_time",
                static_cast<double>(
                    result.visit_time().InMillisecondsSinceUnixEpoch()));
      history_list.Append(std::move(entry));
    }

    std::string json;
    base::JSONWriter::WriteWithOptions(
        history_list,
        base::JSONWriter::OPTIONS_PRETTY_PRINT |
            base::JSONWriter::OPTIONS_OMIT_DOUBLE_TYPE_PRESERVATION,
        &json);
    std::move(callback).Run(json);
  }

  mojo::Receiver<mojom::PageHandler> receiver_;
  mojo::Remote<mojom::Page> page_;
  raw_ptr<Profile> profile_;
  raw_ptr<history::HistoryService> history_service_;
  raw_ptr<finds::FindsService> finds_service_;
  base::CancelableTaskTracker history_task_tracker_;

  base::WeakPtrFactory<PageHandlerImpl> weak_ptr_factory_{this};
};

bool ChromeFindsInternalsUIConfig::IsWebUIEnabled(
    content::BrowserContext* browser_context) {
  return base::FeatureList::IsEnabled(finds::features::kChromeFindsInternals);
}

ChromeFindsInternalsUI::ChromeFindsInternalsUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui) {
  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      profile, chrome::kChromeUIChromeFindsInternalsHost);
  webui::SetupWebUIDataSource(source, kChromeFindsInternalsResources,
                              IDR_CHROME_FINDS_INTERNALS_INDEX_HTML);
}

ChromeFindsInternalsUI::~ChromeFindsInternalsUI() = default;

void ChromeFindsInternalsUI::BindInterface(
    mojo::PendingReceiver<mojom::PageHandlerFactory> receiver) {
  factory_receiver_.reset();
  factory_receiver_.Bind(std::move(receiver));
}

void ChromeFindsInternalsUI::CreatePageHandler(
    mojo::PendingRemote<mojom::Page> page,
    mojo::PendingReceiver<mojom::PageHandler> receiver) {
  Profile* profile = Profile::FromWebUI(web_ui());
  page_handler_ = std::make_unique<PageHandlerImpl>(std::move(receiver),
                                                    std::move(page), profile);
}

WEB_UI_CONTROLLER_TYPE_IMPL(ChromeFindsInternalsUI)

}  // namespace chrome_finds_internals
