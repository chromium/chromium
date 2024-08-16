// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_info/core/page_info_history_data_source.h"

#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/i18n/number_formatting.h"
#include "base/i18n/time_formatting.h"
#include "base/strings/string_util.h"
#include "components/history/core/browser/history_service.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace page_info {

PageInfoHistoryDataSource::PageInfoHistoryDataSource(
    history::HistoryService* history_service,
    const GURL& site_url)
    : history_service_(history_service), site_url_(site_url) {}

PageInfoHistoryDataSource::~PageInfoHistoryDataSource() = default;

// static
std::u16string PageInfoHistoryDataSource::FormatLastVisitedTimestamp(
    base::Time last_visit,
    base::Time now) {
  if (last_visit.is_null())
    return std::u16string();

  constexpr base::TimeDelta kDay = base::Days(1);

  const base::Time midnight_today = now.LocalMidnight();
  const base::Time mightnight_last_visited = last_visit.LocalMidnight();
  base::TimeDelta delta = midnight_today - mightnight_last_visited;
  // Adjust delta for DST, add or remove one hour as need to make the delta
  // divisible by 24 (in hours).
  const base::TimeDelta remainder =
      delta % kDay < base::Milliseconds(0) ? -(delta % kDay) : delta % kDay;
  if (remainder != base::Milliseconds(0)) {
    DCHECK(remainder == base::Hours(23) || remainder == base::Hours(1));
    delta += remainder == base::Hours(1) ? -base::Hours(1) : base::Hours(1);
  }

  if (delta == base::Milliseconds(0))
    return l10n_util::GetStringUTF16(IDS_PAGE_INFO_HISTORY_LAST_VISIT_TODAY);

  if (delta == kDay)
    return l10n_util::GetStringUTF16(
        IDS_PAGE_INFO_HISTORY_LAST_VISIT_YESTERDAY);

  if (delta > kDay && delta <= kDay * 7)
    return l10n_util::GetStringFUTF16Int(IDS_PAGE_INFO_HISTORY_LAST_VISIT_DAYS,
                                         delta.InDays());

  return l10n_util::GetStringFUTF16(IDS_PAGE_INFO_HISTORY_LAST_VISIT_DATE,
                                    base::TimeFormatShortDate(last_visit));
}

void PageInfoHistoryDataSource::GetLastVisitedTimestamp(
    base::OnceCallback<void(std::optional<base::Time>)> callback) {
  // TODO(crbug.com/40808038): Use the data source in Android implementation.
  base::Time now = base::Time::Now();
  history_service_->GetLastVisitToHost(
      site_url_.host(), base::Time() /* before_time */, now /* end_time */,
      base::BindOnce(&PageInfoHistoryDataSource::
                         OnLastVisitBeforeRecentNavigationsComplete,
                     weak_factory_.GetWeakPtr(), site_url_.host(), now,
                     std::move(callback)),
      &query_task_tracker_);
}

void PageInfoHistoryDataSource::OnLastVisitBeforeRecentNavigationsComplete(
    const std::string& host_name,
    base::Time query_start_time,
    base::OnceCallback<void(std::optional<base::Time>)> callback,
    history::HistoryLastVisitResult result) {
  if (!result.success || result.last_visit.is_null()) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  base::Time end_time =
      result.last_visit < (query_start_time - base::Minutes(1))
          ? result.last_visit
          : query_start_time - base::Minutes(1);
  history_service_->GetLastVisitToHost(
      host_name, base::Time() /* before_time */, end_time /* end_time */,
      base::BindOnce(&PageInfoHistoryDataSource::
                         OnLastVisitBeforeRecentNavigationsComplete2,
                     weak_factory_.GetWeakPtr(), std::move(callback)),
      &query_task_tracker_);
}

void PageInfoHistoryDataSource::OnLastVisitBeforeRecentNavigationsComplete2(
    base::OnceCallback<void(std::optional<base::Time>)> callback,
    history::HistoryLastVisitResult result) {
  // Checks that the result is still valid.
  CHECK(result.success);
  base::Time last_visit = result.last_visit;
  std::move(callback).Run(last_visit.is_null() ? std::nullopt
                                               : std::optional(last_visit));
}

}  // namespace page_info
