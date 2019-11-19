// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/content_suggestion.h"

#include <utility>

namespace ntp_snippets {

bool ContentSuggestion::ID::operator==(const ID& rhs) const {
  return category_ == rhs.category_ &&
         id_within_category_ == rhs.id_within_category_;
}

bool ContentSuggestion::ID::operator!=(const ID& rhs) const {
  return !(*this == rhs);
}

ContentSuggestion::ContentSuggestion(const ID& id, const GURL& url)
    : id_(id), url_(url), score_(0), is_video_suggestion_(false) {}

ContentSuggestion::ContentSuggestion(Category category,
                                     const std::string& id_within_category,
                                     const GURL& url)
    : id_(category, id_within_category),
      url_(url),
      score_(0),
      is_video_suggestion_(false) {}

ContentSuggestion::ContentSuggestion(ContentSuggestion&&) = default;

ContentSuggestion& ContentSuggestion::operator=(ContentSuggestion&&) = default;

ContentSuggestion::~ContentSuggestion() = default;

std::ostream& operator<<(std::ostream& os, const ContentSuggestion::ID& id) {
  os << id.category() << "|" << id.id_within_category();
  return os;
}

// static
GURL ContentSuggestion::GetFaviconDomain(const GURL& favicon_url) {
  return favicon_url.GetWithEmptyPath();
}

void ContentSuggestion::set_reading_list_suggestion_extra(
    std::unique_ptr<ReadingListSuggestionExtra> reading_list_suggestion_extra) {
  DCHECK(id_.category().IsKnownCategory(KnownCategories::READING_LIST));
  reading_list_suggestion_extra_ = std::move(reading_list_suggestion_extra);
}

void ContentSuggestion::set_notification_extra(
    std::unique_ptr<NotificationExtra> notification_extra) {
  notification_extra_ = std::move(notification_extra);
}

}  // namespace ntp_snippets
