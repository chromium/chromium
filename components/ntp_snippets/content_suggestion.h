// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_SNIPPETS_CONTENT_SUGGESTION_H_
#define COMPONENTS_NTP_SNIPPETS_CONTENT_SUGGESTION_H_

#include <cstdint>
#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/strings/string16.h"
#include "base/time/time.h"
#include "components/ntp_snippets/category.h"
#include "url/gurl.h"

namespace ntp_snippets {

// ReadingListSuggestionExtra contains additional data which is only available
// for Reading List suggestions.
struct ReadingListSuggestionExtra {
  // URL of the page whose favicon should be displayed for this suggestion.
  GURL favicon_page_url;
};

// Contains additional data for notification-worthy suggestions.
struct NotificationExtra {
  // Deadline for showing notification. If the deadline is past, the
  // notification is no longer fresh and no notification should be sent. If the
  // deadline passes while a notification is up, it should be canceled.
  base::Time deadline;
};

// A content suggestion for the new tab page, which can be an article or an
// offline page, for example.
class ContentSuggestion {
 public:
  class ID {
   public:
    ID(Category category, const std::string& id_within_category)
        : category_(category), id_within_category_(id_within_category) {}

    Category category() const { return category_; }

    const std::string& id_within_category() const {
      return id_within_category_;
    }

    bool operator==(const ID& rhs) const;
    bool operator!=(const ID& rhs) const;

   private:
    Category category_;
    std::string id_within_category_;

    // Allow copy and assignment.
  };

  // Creates a new ContentSuggestion. The caller must ensure that the |id|
  // passed in here is unique application-wide.
  ContentSuggestion(const ID& id, const GURL& url);
  ContentSuggestion(Category category,
                    const std::string& id_within_category,
                    const GURL& url);
  ContentSuggestion(ContentSuggestion&&);
  ContentSuggestion& operator=(ContentSuggestion&&);

  ~ContentSuggestion();

  // An ID for identifying the suggestion. The ID is unique application-wide.
  const ID& id() const { return id_; }

  // The URL where the content referenced by the suggestion can be accessed.
  // This may be an AMP URL.
  const GURL& url() const { return url_; }

  // The URL of the page that links to a favicon that represents the suggestion.
  // Path is trimmed for the URL because the current favicon server backend
  // prefers it this way.
  GURL url_with_favicon() const {
    return url_with_favicon_.is_valid() ? GetFaviconDomain(url_with_favicon_)
                                        : GetFaviconDomain(url_);
  }
  void set_url_with_favicon(const GURL& url_with_favicon) {
    url_with_favicon_ = url_with_favicon;
  }

  // A URL for an image that represents the content of the suggestion.
  // Empty when an image is not available.
  GURL salient_image_url() const { return salient_image_url_; }
  void set_salient_image_url(const GURL& salient_image_url) {
    salient_image_url_ = salient_image_url;
  }

  static GURL GetFaviconDomain(const GURL& favicon_url);

  // Title of the suggestion.
  const base::string16& title() const { return title_; }
  void set_title(const base::string16& title) { title_ = title; }

  // Summary or relevant textual extract from the content.
  const base::string16& snippet_text() const { return snippet_text_; }
  void set_snippet_text(const base::string16& snippet_text) {
    snippet_text_ = snippet_text;
  }

  // The time when the content represented by this suggestion was published.
  const base::Time& publish_date() const { return publish_date_; }
  void set_publish_date(const base::Time& publish_date) {
    publish_date_ = publish_date;
  }

  // The name of the source/publisher of this suggestion.
  const base::string16& publisher_name() const { return publisher_name_; }
  void set_publisher_name(const base::string16& publisher_name) {
    publisher_name_ = publisher_name;
  }

  bool is_video_suggestion() const { return is_video_suggestion_; }
  void set_is_video_suggestion(bool is_video_suggestion) {
    is_video_suggestion_ = is_video_suggestion;
  }

  // TODO(pke): Remove the score from the ContentSuggestion class. The UI only
  // uses it to track user clicks (histogram data). Instead, the providers
  // should be informed about clicks and do appropriate logging themselves.
  // IMPORTANT: The score may simply be 0 for suggestions from providers which
  // cannot provide score values.
  float score() const { return score_; }
  void set_score(float score) { score_ = score; }

  // Extra information for reading list suggestions. Only available for
  // KnownCategories::READING_LIST suggestions.
  ReadingListSuggestionExtra* reading_list_suggestion_extra() const {
    return reading_list_suggestion_extra_.get();
  }
  void set_reading_list_suggestion_extra(
      std::unique_ptr<ReadingListSuggestionExtra>
          reading_list_suggestion_extra);

  // Extra information for notifications. When absent, no notification should be
  // sent for this suggestion. When present, a notification should be sent,
  // unless other factors disallow it (examples: the extra parameters say to;
  // notifications are disabled; Chrome is in the foreground).
  NotificationExtra* notification_extra() const {
    return notification_extra_.get();
  }
  void set_notification_extra(
      std::unique_ptr<NotificationExtra> notification_extra);

  const base::Time& fetch_date() const { return fetch_date_; }
  void set_fetch_date(const base::Time& fetch_date) {
    fetch_date_ = fetch_date;
  }

  const base::Optional<uint32_t>& optional_image_dominant_color() const {
    return image_dominant_color_;
  }
  void set_optional_image_dominant_color(
      const base::Optional<uint32_t>& optional_color_int) {
    image_dominant_color_ = optional_color_int;
  }

 private:
  ID id_;
  GURL url_;
  GURL url_with_favicon_;
  GURL salient_image_url_;
  base::string16 title_;
  base::string16 snippet_text_;
  base::Time publish_date_;
  base::string16 publisher_name_;
  float score_;
  std::unique_ptr<ReadingListSuggestionExtra> reading_list_suggestion_extra_;
  std::unique_ptr<NotificationExtra> notification_extra_;

  // The time when the remote suggestion was fetched from the server. This field
  // is only populated when the ContentSuggestion is created from a
  // RemoteSuggestion.
  base::Time fetch_date_;

  bool is_video_suggestion_;

  // Encoded as an Android @ColorInt.
  base::Optional<uint32_t> image_dominant_color_;

  DISALLOW_COPY_AND_ASSIGN(ContentSuggestion);
};

std::ostream& operator<<(std::ostream& os, const ContentSuggestion::ID& id);

}  // namespace ntp_snippets

#endif  // COMPONENTS_NTP_SNIPPETS_CONTENT_SUGGESTION_H_
