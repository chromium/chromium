// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_SNIPPETS_REMOTE_REMOTE_SUGGESTION_BUILDER_H_
#define COMPONENTS_NTP_SNIPPETS_REMOTE_REMOTE_SUGGESTION_BUILDER_H_

#include <string>
#include <vector>

#include "base/time/time.h"
#include "components/ntp_snippets/remote/json_to_categories.h"
#include "components/ntp_snippets/remote/remote_suggestion.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ntp_snippets {

namespace test {

class RemoteSuggestionBuilder {
 public:
  RemoteSuggestionBuilder();
  RemoteSuggestionBuilder(const RemoteSuggestionBuilder& other);
  ~RemoteSuggestionBuilder();

  RemoteSuggestionBuilder& AddId(const std::string& id);
  RemoteSuggestionBuilder& SetTitle(const std::string& title);
  RemoteSuggestionBuilder& SetSnippet(const std::string& snippet);
  RemoteSuggestionBuilder& SetImageUrl(const std::string& image_url);
  RemoteSuggestionBuilder& SetPublishDate(const base::Time& publish_date);
  RemoteSuggestionBuilder& SetExpiryDate(const base::Time& expiry_date);
  RemoteSuggestionBuilder& SetScore(double score);
  RemoteSuggestionBuilder& SetIsDismissed(bool is_dismissed);
  RemoteSuggestionBuilder& SetRemoteCategoryId(int remote_category_id);
  RemoteSuggestionBuilder& SetUrl(const std::string& url);
  RemoteSuggestionBuilder& SetPublisher(const std::string& publisher);
  RemoteSuggestionBuilder& SetAmpUrl(const std::string& amp_url);
  RemoteSuggestionBuilder& SetFetchDate(const base::Time& fetch_date);
  RemoteSuggestionBuilder& SetRank(int rank);
  RemoteSuggestionBuilder& SetShouldNotify(bool should_notify);
  RemoteSuggestionBuilder& SetNotificationDeadline(
      const base::Time& notification_deadline);

  std::unique_ptr<RemoteSuggestion> Build() const;

 private:
  absl::optional<std::vector<std::string>> ids_;
  absl::optional<std::string> title_;
  absl::optional<std::string> snippet_;
  absl::optional<std::string> salient_image_url_;
  absl::optional<base::Time> publish_date_;
  absl::optional<base::Time> expiry_date_;
  absl::optional<double> score_;
  absl::optional<bool> is_dismissed_;
  absl::optional<int> remote_category_id_;
  absl::optional<std::string> url_;
  absl::optional<std::string> publisher_name_;
  absl::optional<std::string> amp_url_;
  absl::optional<base::Time> fetch_date_;
  absl::optional<int> rank_;
  absl::optional<bool> should_notify_;
  absl::optional<base::Time> notification_deadline_;
};

class FetchedCategoryBuilder {
 public:
  FetchedCategoryBuilder();
  FetchedCategoryBuilder(const FetchedCategoryBuilder& other);
  ~FetchedCategoryBuilder();

  FetchedCategoryBuilder& SetCategory(Category category);
  FetchedCategoryBuilder& SetTitle(const std::string& title);
  FetchedCategoryBuilder& SetCardLayout(
      ContentSuggestionsCardLayout card_layout);
  FetchedCategoryBuilder& SetAdditionalAction(
      ContentSuggestionsAdditionalAction additional_action);
  FetchedCategoryBuilder& SetShowIfEmpty(bool show_if_empty);
  FetchedCategoryBuilder& SetNoSuggestionsMessage(
      const std::string& no_suggestions_message);
  FetchedCategoryBuilder& AddSuggestionViaBuilder(
      const RemoteSuggestionBuilder& builder);

  FetchedCategory Build() const;

 private:
  absl::optional<Category> category_;
  absl::optional<std::u16string> title_;
  absl::optional<ContentSuggestionsCardLayout> card_layout_;
  absl::optional<ContentSuggestionsAdditionalAction> additional_action_;
  absl::optional<bool> show_if_empty_;
  absl::optional<std::u16string> no_suggestions_message_;
  absl::optional<std::vector<RemoteSuggestionBuilder>> suggestion_builders_;
};

}  // namespace test

}  // namespace ntp_snippets

#endif  // COMPONENTS_NTP_SNIPPETS_REMOTE_REMOTE_SUGGESTION_BUILDER_H_
