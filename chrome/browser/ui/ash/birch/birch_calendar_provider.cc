// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/birch/birch_calendar_provider.h"

#include <string>
#include <vector>

#include "ash/birch/birch_item.h"
#include "ash/birch/birch_model.h"
#include "ash/shell.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/ui/ash/birch/birch_calendar_fetcher.h"
#include "google_apis/calendar/calendar_api_requests.h"
#include "google_apis/common/api_error_codes.h"
#include "google_apis/common/auth_service.h"
#include "google_apis/common/request_sender.h"
#include "google_apis/gaia/gaia_constants.h"

namespace ash {

BirchCalendarProvider::BirchCalendarProvider(Profile* profile)
    : profile_(profile) {}

BirchCalendarProvider::~BirchCalendarProvider() {}

void BirchCalendarProvider::Initialize() {
  fetcher_ = std::make_unique<BirchCalendarFetcher>(profile_);
}

void BirchCalendarProvider::Shutdown() {
  fetcher_->Shutdown();
}

void BirchCalendarProvider::RequestBirchDataFetch() {
  VLOG(1) << "BirchCalendarProvider::RequestBirchDataFetch";

  // Get all events from 2 hours ago until 1 day in the future.
  base::Time now = base::Time::Now();
  base::Time start_time = now - base::Hours(2);
  base::Time end_time = now + base::Days(1);

  fetcher_->GetCalendarEvents(
      start_time, end_time,
      base::BindOnce(&BirchCalendarProvider::OnEventsFetched,
                     weak_factory_.GetWeakPtr()));
}

void BirchCalendarProvider::OnEventsFetched(
    google_apis::ApiErrorCode error,
    std::unique_ptr<google_apis::calendar::EventList> events) {
  VLOG(1) << "BirchCalendarProvider::OnEventsFetched error " << error
          << " size " << (events ? events->items().size() : -1);
  auto* birch_model = Shell::Get()->birch_model();

  if (error != google_apis::HTTP_SUCCESS) {
    LOG(ERROR) << "Failed to fetch calendar events, error "
               << static_cast<int>(error);
    birch_model->SetCalendarItems({});
    return;
  }

  // There might not be any events.
  if (!events) {
    birch_model->SetCalendarItems({});
    return;
  }

  std::vector<BirchCalendarItem> calendar_items;
  std::vector<BirchAttachmentItem> attachment_items;
  for (const auto& item : events->items()) {
    if (!item) {
      continue;
    }

    // Convert the data from google_apis format to birch format.
    BirchCalendarItem birch_item(
        base::UTF8ToUTF16(item->summary()), item->start_time().date_time(),
        item->end_time().date_time(), GURL(item->html_link()),
        item->conference_data_uri());
    calendar_items.push_back(std::move(birch_item));

    // Attachments are stored as separate items.
    for (const auto& attachment : item->attachments()) {
      BirchAttachmentItem birch_attachment(
          base::UTF8ToUTF16(attachment.title()), attachment.file_url(),
          attachment.icon_link(), item->start_time().date_time(),
          item->end_time().date_time());
      attachment_items.push_back(std::move(birch_attachment));
    }
  }
  birch_model->SetCalendarItems(std::move(calendar_items));
  birch_model->SetAttachmentItems(std::move(attachment_items));
}

void BirchCalendarProvider::SetFetcherForTest(
    std::unique_ptr<BirchCalendarFetcher> fetcher) {
  fetcher_ = std::move(fetcher);
}

}  // namespace ash
