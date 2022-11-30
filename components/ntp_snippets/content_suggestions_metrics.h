// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_SNIPPETS_CONTENT_SUGGESTIONS_METRICS_H_
#define COMPONENTS_NTP_SNIPPETS_CONTENT_SUGGESTIONS_METRICS_H_

#include <utility>
#include <vector>

#include "base/time/time.h"
#include "components/ntp_snippets/category.h"
#include "ui/base/window_open_disposition.h"

namespace ntp_snippets {
namespace metrics {

// |is_category_visible| contains true iff the corresponding category can be
// seen by the user on this page (even if it is empty). It does not depend on
// whether the user actually saw the category.
void OnPageShown(const std::vector<Category>& categories,
                 const std::vector<int>& suggestions_per_category,
                 const std::vector<bool>& is_category_visible);

// Should only be called once per NTP for each suggestion.
void OnSuggestionShown(int global_position,
                       Category category,
                       int position_in_category,
                       base::Time publish_date,
                       float score,
                       base::Time fetch_date);

// TODO(crbug.com/682160): Take struct, so that one could not mix up the
// order of arguments.
void OnSuggestionOpened(int global_position,
                        Category category,
                        int category_index,
                        int position_in_category,
                        base::Time publish_date,
                        float score,
                        WindowOpenDisposition disposition,
                        bool is_prefetched,
                        bool is_offline);

void OnSuggestionMenuOpened(int global_position,
                            Category category,
                            int position_in_category,
                            base::Time publish_date,
                            float score);

void OnSuggestionDismissed(int global_position,
                           Category category,
                           int position_in_category,
                           bool visited);

void OnSuggestionTargetVisited(Category category, base::TimeDelta visit_time);

void OnCategoryMovedUp(int new_index);

// Should only be called once per NTP for each "more" button.
void OnMoreButtonShown(Category category, int position);

void OnMoreButtonClicked(Category category, int position);

void OnCategoryDismissed(Category category);

void RecordRemoteSuggestionsProviderState(bool enabled);

void RecordContentSuggestionDismissed();

void RecordCategoryDismissed();

void RecordFetchAction();

}  // namespace metrics
}  // namespace ntp_snippets

#endif  // COMPONENTS_NTP_SNIPPETS_CONTENT_SUGGESTIONS_METRICS_H_
