// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/remote/json_to_categories.h"

#include "base/strings/utf_string_conversions.h"
#include "components/strings/grit/components_strings.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/l10n/l10n_util.h"

namespace ntp_snippets {

namespace {

// Creates suggestions from dictionary values in |list| and adds them to
// |suggestions|. Returns true on success, false if anything went wrong.
bool AddSuggestionsFromListValue(int remote_category_id,
                                 const base::Value& list,
                                 RemoteSuggestion::PtrVector* suggestions,
                                 const base::Time& fetch_time) {
  DCHECK(list.is_list());

  for (const base::Value& value : list.GetListDeprecated()) {
    const base::DictionaryValue* dict = nullptr;
    if (!value.GetAsDictionary(&dict)) {
      return false;
    }

    std::unique_ptr<RemoteSuggestion> suggestion;

    suggestion = RemoteSuggestion::CreateFromContentSuggestionsDictionary(
        *dict, remote_category_id, fetch_time);

    if (!suggestion) {
      return false;
    }

    suggestions->push_back(std::move(suggestion));
  }
  return true;
}

}  // namespace

FetchedCategory::FetchedCategory(Category c, CategoryInfo&& info)
    : category(c), info(info) {}

FetchedCategory::FetchedCategory(FetchedCategory&&) = default;

FetchedCategory::~FetchedCategory() = default;

FetchedCategory& FetchedCategory::operator=(FetchedCategory&&) = default;

CategoryInfo BuildArticleCategoryInfo(
    const absl::optional<std::u16string>& title) {
  return CategoryInfo(
      title.has_value() ? title.value()
                        : l10n_util::GetStringUTF16(
                              IDS_NTP_ARTICLE_SUGGESTIONS_SECTION_HEADER),
      ContentSuggestionsCardLayout::FULL_CARD,
      ContentSuggestionsAdditionalAction::FETCH,
      /*show_if_empty=*/true,
      l10n_util::GetStringUTF16(IDS_NTP_ARTICLE_SUGGESTIONS_SECTION_EMPTY));
}

CategoryInfo BuildRemoteCategoryInfo(const std::u16string& title,
                                     bool allow_fetching_more_results) {
  ContentSuggestionsAdditionalAction action =
      ContentSuggestionsAdditionalAction::NONE;
  if (allow_fetching_more_results) {
    action = ContentSuggestionsAdditionalAction::FETCH;
  }
  return CategoryInfo(
      title, ContentSuggestionsCardLayout::FULL_CARD, action,
      /*show_if_empty=*/false,
      // TODO(tschumann): The message for no-articles is likely wrong
      // and needs to be added to the stubby protocol if we want to
      // support it.
      l10n_util::GetStringUTF16(IDS_NTP_ARTICLE_SUGGESTIONS_SECTION_EMPTY));
}

bool JsonToCategories(const base::Value& parsed,
                      FetchedCategoriesVector* categories,
                      const base::Time& fetch_time) {
  const base::DictionaryValue* top_dict = nullptr;
  if (!parsed.GetAsDictionary(&top_dict)) {
    return false;
  }

  const base::ListValue* categories_value = nullptr;
  if (!top_dict->GetList("categories", &categories_value)) {
    return false;
  }

  for (const base::Value& v : categories_value->GetListDeprecated()) {
    if (!v.is_dict())
      return false;

    const std::string* utf8_title = v.FindStringKey("localizedTitle");
    int remote_category_id = v.FindIntKey("id").value_or(-1);

    if (!utf8_title || remote_category_id <= 0) {
      return false;
    }

    RemoteSuggestion::PtrVector suggestions;
    const base::Value* suggestions_list = v.FindListKey("suggestions");
    // Absence of a list of suggestions is treated as an empty list, which
    // is permissible.
    if (suggestions_list &&
        !AddSuggestionsFromListValue(remote_category_id, *suggestions_list,
                                     &suggestions, fetch_time)) {
      return false;
    }
    Category category = Category::FromRemoteCategory(remote_category_id);
    if (category.IsKnownCategory(KnownCategories::ARTICLES)) {
      categories->push_back(FetchedCategory(
          category, BuildArticleCategoryInfo(base::UTF8ToUTF16(*utf8_title))));
    } else {
      // TODO(tschumann): Right now, the backend does not yet populate this
      // field. Make it mandatory once the backends provide it.
      bool allow_fetching_more_results =
          v.FindBoolKey("allowFetchingMoreResults").value_or(false);
      categories->push_back(FetchedCategory(
          category, BuildRemoteCategoryInfo(base::UTF8ToUTF16(*utf8_title),
                                            allow_fetching_more_results)));
    }
    categories->back().suggestions = std::move(suggestions);
  }

  return true;
}

}  // namespace ntp_snippets
