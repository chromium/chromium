// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VISITED_URL_RANKING_PUBLIC_URL_VISIT_UTIL_H_
#define COMPONENTS_VISITED_URL_RANKING_PUBLIC_URL_VISIT_UTIL_H_

#include <array>
#include <string_view>

#include "base/containers/fixed_flat_set.h"
#include "components/segmentation_platform/public/input_context.h"
#include "components/visited_url_ranking/public/fetch_result.h"
#include "components/visited_url_ranking/public/url_visit_schema.h"
#include "url/gurl.h"

namespace url_deduplication {
class URLDeduplicationHelper;
}  // namespace url_deduplication

namespace visited_url_ranking {

// TODO(crbug.com/330580421): Remove/replace the category blocklist array
// specified in `modules_util.h` with the one below.
inline constexpr auto kBlocklistedCategories =
    base::MakeFixedFlatSet<std::string_view>(
        {"/g/11b76fyj2r", "/m/09lkz", "/m/012mj", "/m/01rbb", "/m/02px0wr",
         "/m/028hh", "/m/034qg", "/m/034dj", "/m/0jxxt", "/m/015fwp",
         "/m/04shl0", "/m/01h6rj", "/m/05qt0", "/m/06gqm", "/m/09l0j_",
         "/m/01pxgq", "/m/0chbx", "/m/02c66t"});

// For Android, this set should be consistent with sDefaultAppBlocklist in
// TabResumptionModuleUtils.java.
inline constexpr auto kDefaultAppBlocklist =
    base::MakeFixedFlatSet<std::string_view>(
        {"assistant.google.com", "calendar.google.com", "docs.google.com",
         "drive.google.com", "mail.google.com", "music.youtube.com",
         "m.youtube.com", "photos.google.com", "www.youtube.com"});

std::unique_ptr<url_deduplication::URLDeduplicationHelper>
CreateDefaultURLDeduplicationHelper();

// Generates an identifier for the given URL leveraged for merging and
// deduplication of similar URLs.
URLMergeKey ComputeURLMergeKey(
    const GURL& url,
    const std::u16string& title,
    url_deduplication::URLDeduplicationHelper* deduplication_helper);

// Generates an input context from a given `URLVisitAggregate` object given a
// schema definition.
scoped_refptr<segmentation_platform::InputContext> AsInputContext(
    const std::array<FieldSchema, kNumInputs>& fields_schema,
    const URLVisitAggregate& url_visit_aggregate);

// Returns tab data if it exists for a `URLVisitAggregate`.
const URLVisitAggregate::TabData* GetTabDataIfExists(
    const URLVisitAggregate& url_visit_aggregate);

// Returns a tab if it exists for a `URLVisitAggregate`.
const URLVisitAggregate::Tab* GetTabIfExists(
    const URLVisitAggregate& url_visit_aggregate);

const URLVisitAggregate::HistoryData* GetHistoryDataIfExists(
    const URLVisitAggregate& url_visit_aggregate);

// Returns a history entry if it exists for a `URLVisitAggregate`.
const history::AnnotatedVisit* GetHistoryEntryVisitIfExists(
    const URLVisitAggregate& url_visit_aggregate);

// Returns the highest priority decorator for a `URLVisitAggregate`.
const Decoration& GetMostRelevantDecoration(
    const URLVisitAggregate& url_visit_aggregate);

// Returns the decoration string for the given `type`.
std::u16string GetStringForDecoration(DecorationType type,
                                      bool visited_recently = false);

// Returns the decoration string for a recent visit at given `last_visit_time`.
std::u16string GetStringForRecencyDecorationWithTime(
    base::Time last_visit_time,
    base::TimeDelta recently_visited_minutes_threshold = base::Minutes(1));

}  // namespace visited_url_ranking

#endif  // COMPONENTS_VISITED_URL_RANKING_PUBLIC_URL_VISIT_UTIL_H_
