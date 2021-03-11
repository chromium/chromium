// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_SNIPPETS_REMOTE_REMOTE_SUGGESTION_BUILDER_H_
#define COMPONENTS_NTP_SNIPPETS_REMOTE_REMOTE_SUGGESTION_BUILDER_H_

#include <string>
#include <vector>

#include "base/optional.h"
#include "components/ntp_snippets/remote/json_to_categories.h"
#include "components/ntp_snippets/remote/remote_suggestion.h"

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
  base::Optional<std::vector<std::string>> ids_;
  base::Optional<std::string> title_;
  base::Optional<std::string> snippet_;
  base::Optional<std::string> salient_image_url_;
  base::Optional<base::Time> publish_date_;
  base::Optional<base::Time> expiry_date_;
  base::Optional<double> score_;
  base::Optional<bool> is_dismissed_;
  base::Optional<int> remote_category_id_;
  base::Optional<std::string> url_;
  base::Optional<std::string> publisher_name_;
  base::Optional<std::string> amp_url_;
  base::Optional<base::Time> fetch_date_;
  base::Optional<int> rank_;
  base::Optional<bool> should_notify_;
  base::Optional<base::Time> notification_deadline_;
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
  base::Optional<Category> category_;
  base::Optional<std::u16string> title_;
  base::Optional<ContentSuggestionsCardLayout> card_layout_;
  base::Optional<ContentSuggestionsAdditionalAction> additional_action_;
  base::Optional<bool> show_if_empty_;
  base::Optional<std::u16string> no_suggestions_message_;
  base::Optional<std::vector<RemoteSuggestionBuilder>> suggestion_builders_;
};

}  // namespace test

}  // namespace ntp_snippets

#endif  // COMPONENTS_NTP_SNIPPETS_REMOTE_REMOTE_SUGGESTION_BUILDER_H_
