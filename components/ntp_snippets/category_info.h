// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_SNIPPETS_CATEGORY_INFO_H_
#define COMPONENTS_NTP_SNIPPETS_CATEGORY_INFO_H_

#include <string>

namespace ntp_snippets {

// On Android builds, a Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.ntp.snippets
enum class ContentSuggestionsCardLayout {
  // Uses all fields.
  FULL_CARD,

  // No snippet_text and no thumbnail image.
  MINIMAL_CARD
};

// On Android builds, a Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.suggestions
enum class ContentSuggestionsAdditionalAction {
  // No additional action available.
  NONE,

  // More suggestions can be fetched using the Fetch methods with this category.
  FETCH,

  // Open a new surface dedicated to the content related to this category. The
  // UI has to choose which surface to open.
  VIEW_ALL
};

// Contains static meta information about a Category.
class CategoryInfo {
 public:
  CategoryInfo(const std::u16string& title,
               ContentSuggestionsCardLayout card_layout,
               ContentSuggestionsAdditionalAction additional_action,
               bool show_if_empty,
               const std::u16string& no_suggestions_message);
  CategoryInfo() = delete;
  CategoryInfo(CategoryInfo&&);
  CategoryInfo(const CategoryInfo&);
  CategoryInfo& operator=(CategoryInfo&&);
  CategoryInfo& operator=(const CategoryInfo&);
  ~CategoryInfo();

  // Localized title of the category.
  const std::u16string& title() const { return title_; }

  // Layout of the cards to be used to display suggestions in this category.
  ContentSuggestionsCardLayout card_layout() const { return card_layout_; }

  // Supported action for the category.
  ContentSuggestionsAdditionalAction additional_action() const {
    return additional_action_;
  }

  // Whether this category should be shown if it offers no suggestions.
  bool show_if_empty() const { return show_if_empty_; }

  // The message to show if there are no suggestions in this category. Note that
  // this matters even if |show_if_empty()| is false: The message still shows
  // up when the user dismisses all suggestions in the category.
  const std::u16string& no_suggestions_message() const {
    return no_suggestions_message_;
  }

 private:
  std::u16string title_;
  ContentSuggestionsCardLayout card_layout_;

  ContentSuggestionsAdditionalAction additional_action_;

  // Whether to show the category if a fetch returns no suggestions.
  bool show_if_empty_;
  std::u16string no_suggestions_message_;
};

}  // namespace ntp_snippets

#endif  // COMPONENTS_NTP_SNIPPETS_CATEGORY_INFO_H_
