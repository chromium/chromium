// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/omnibox_answer_result.h"

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/app_list/vector_icons/vector_icons.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "chrome/browser/bitmap_fetcher/bitmap_fetcher.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/app_list_controller_delegate.h"
#include "chrome/browser/ui/app_list/search/common/icon_constants.h"
#include "chrome/browser/ui/app_list/search/omnibox_util.h"
#include "chrome/browser/ui/app_list/search/search_tags_util.h"
#include "chrome/grit/generated_resources.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/omnibox/browser/autocomplete_controller.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_match_type.h"
#include "components/omnibox/browser/vector_icons.h"
#include "extensions/common/image_util.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/paint_vector_icon.h"
#include "url/gurl.h"
#include "url/url_canon.h"

namespace app_list {
namespace {

using Tag = ash::SearchResultTag;
using TextItem = ash::SearchResultTextItem;
using TextType = ash::SearchResultTextItemType;

constexpr char kOmniboxAnswerSchema[] = "omnibox_answer://";

ChromeSearchResult::IconInfo CreateAnswerIconInfo(
    const gfx::VectorIcon& vector_icon) {
  const int dimension = GetAnswerCardIconDimension();
  const auto icon = gfx::ImageSkiaOperations::CreateImageWithCircleBackground(
      dimension / 2, gfx::kGoogleBlue600,
      gfx::CreateVectorIcon(vector_icon, SK_ColorWHITE));
  return ChromeSearchResult::IconInfo(icon, dimension);
}

TextItem CreateTextItem(const std::u16string& text) {
  TextItem text_item(TextType::kString);
  text_item.SetText(text);
  text_item.SetTextTags({});
  return text_item;
}

std::u16string GetAdditionalText(const SuggestionAnswer::ImageLine& line) {
  if (line.additional_text()) {
    const auto& additional_text = line.additional_text()->text();
    if (!additional_text.empty())
      return additional_text;
  }
  return std::u16string();
}

ash::SearchResultTags TextFieldToTags(
    const SuggestionAnswer::TextField& text_field) {
  ash::SearchResultTags tags;
  const auto length = text_field.text().length();
  switch (text_field.style()) {
    case SuggestionAnswer::TextStyle::POSITIVE:
      tags.push_back(Tag(Tag::GREEN, 0, length));
      break;
    case SuggestionAnswer::TextStyle::NEGATIVE:
      tags.push_back(Tag(Tag::RED, 0, length));
      break;
    default:
      break;
  }
  return tags;
}

std::vector<TextItem> ImageLineToTextVector(
    const SuggestionAnswer::ImageLine& line) {
  std::vector<TextItem> text_vector;
  for (const auto& text_field : line.text_fields()) {
    if (!text_vector.empty())
      text_vector.push_back(CreateTextItem(u" "));

    TextItem text_item(TextType::kString);
    text_item.SetText(text_field.text());
    text_item.SetTextTags(TextFieldToTags(text_field));
    text_vector.push_back(text_item);
  }
  return text_vector;
}

std::vector<TextItem> AddBoldTags(std::vector<TextItem> text_vector) {
  std::vector<TextItem> bolded_vector;
  for (const auto& old_text : text_vector) {
    auto new_text = old_text;
    if (old_text.GetType() == TextType::kString) {
      auto tags = old_text.GetTextTags();
      tags.push_back(Tag(Tag::MATCH, 0, old_text.GetText().length()));
      new_text.SetTextTags(tags);
    }
    bolded_vector.push_back(new_text);
  }
  return bolded_vector;
}

// TODO(crbug.com/1250154): Remove non-a11y references to this once the
// productivity launcher is enabled.
std::u16string TextVectorToString(const std::vector<TextItem>& text_vector) {
  std::vector<std::u16string> text;
  for (const auto& text_item : text_vector) {
    if (text_item.GetType() == TextType::kString) {
      text.push_back(text_item.GetText());
    }
  }
  return base::StrCat(text);
}

std::u16string ComputeAccessibleName(
    const std::vector<std::vector<TextItem>>& text_vectors) {
  std::vector<std::u16string> text;
  for (const auto& text_vector : text_vectors) {
    if (!text_vector.empty()) {
      text.push_back(TextVectorToString(text_vector));
    }
  }
  return base::JoinString(text, u", ");
}

}  // namespace

OmniboxAnswerResult::OmniboxAnswerResult(
    Profile* profile,
    AppListControllerDelegate* list_controller,
    AutocompleteController* autocomplete_controller,
    const AutocompleteMatch& match)
    : profile_(profile),
      list_controller_(list_controller),
      autocomplete_controller_(autocomplete_controller),
      match_(match) {
  if (match_.search_terms_args && autocomplete_controller_) {
    match_.search_terms_args->request_source = TemplateURLRef::CROS_APP_LIST;
    autocomplete_controller_->SetMatchDestinationURL(&match_);
  }
  SetDisplayType(ash::features::IsProductivityLauncherEnabled()
                     ? DisplayType::kAnswerCard
                     : DisplayType::kList);
  SetResultType(ResultType::kOmnibox);
  SetCategory(Category::kSearchAndAssistant);
  set_id(kOmniboxAnswerSchema + match_.stripped_destination_url.spec());

  SetMetricsType(IsCalculatorResult() ? ash::OMNIBOX_CALCULATOR
                                      : ash::OMNIBOX_ANSWER);

  // Derive relevance from omnibox relevance and normalize it to [0, 1].
  set_relevance(match_.relevance / kMaxOmniboxScore);

  if (AutocompleteMatch::IsSearchType(match_.type))
    SetIsOmniboxSearch(true);

  UpdateIcon();

  if (ash::features::IsProductivityLauncherEnabled()) {
    UpdateTitleAndDetails();
  } else {
    UpdateClassicTitleAndDetails();
  }
}

OmniboxAnswerResult::~OmniboxAnswerResult() = default;

void OmniboxAnswerResult::Open(int event_flags) {
  list_controller_->OpenURL(profile_, match_.destination_url, match_.transition,
                            ui::DispositionFromEventFlags(event_flags));
}

void OmniboxAnswerResult::UpdateIcon() {
  if (IsCalculatorResult()) {
    SetIcon(CreateAnswerIconInfo(omnibox::kCalculatorIcon));
  } else if (IsWeatherResult() && !match_.answer->image_url().is_empty()) {
    // Weather icons are downloaded. Check this first so that the local
    // default answer icon can be used as a fallback if the URL is missing.
    FetchImage(match_.answer->image_url());
  } else {
    SetIcon(CreateAnswerIconInfo(
        AutocompleteMatch::AnswerTypeToAnswerIcon(match_.answer->type())));
  }
}

void OmniboxAnswerResult::UpdateTitleAndDetails() {
  // TODO(crbug.com/1250154): Simplify this and split into separate methods.
  if (IsCalculatorResult()) {
    std::vector<TextItem> contents_vector = {CreateTextItem(match_.contents)};
    if (match_.description.empty()) {
      SetTitleTextVector(contents_vector);
    } else {
      SetTitleTextVector({CreateTextItem(match_.description)});
      SetDetailsTextVector(contents_vector);
    }
  } else if (IsWeatherResult()) {
    const auto& second_line = match_.answer->second_line();

    SetBigTitleTextVector(ImageLineToTextVector(second_line));
    // TODO(crbug.com/1250154): Put additional weather text into the title
    // field instead of match contents, once the information becomes available
    // from the Suggest server.
    SetTitleTextVector({CreateTextItem(match_.contents)});
    SetDetailsTextVector({CreateTextItem(GetAdditionalText(second_line))});
  } else {
    const auto& second_line = match_.answer->second_line();
    auto title_vector = ImageLineToTextVector(second_line);
    const auto& additional_title = GetAdditionalText(second_line);
    if (!additional_title.empty()) {
      title_vector.push_back(CreateTextItem(u" "));
      title_vector.push_back(CreateTextItem(additional_title));
    }
    SetTitleTextVector(title_vector);

    const auto& first_line = match_.answer->first_line();
    std::vector<TextItem> details_vector = {CreateTextItem(match_.contents)};
    const auto& additional_details = GetAdditionalText(first_line);
    if (!additional_details.empty()) {
      details_vector.push_back(CreateTextItem(u" "));
      details_vector.push_back(CreateTextItem(additional_details));
    }
    SetDetailsTextVector(details_vector);
  }

  // Bold the title field.
  SetTitleTextVector(AddBoldTags(title_text_vector()));

  std::u16string accessible_name = ComputeAccessibleName(
      {big_title_text_vector(), title_text_vector(), details_text_vector()});
  SetAccessibleName(accessible_name);

  // TODO(crbug.com/1250154): Remove these once the migration to TextVectors
  // is completed.
  SetTitle(TextVectorToString(title_text_vector()));
  SetDetails(TextVectorToString(details_text_vector()));
}

void OmniboxAnswerResult::UpdateClassicTitleAndDetails() {
  if (IsCalculatorResult()) {
    SetTitle(match_.contents);
    ChromeSearchResult::Tags title_tags;
    ACMatchClassificationsToTags(match_.contents, match_.contents_class,
                                 &title_tags);
    SetTitleTags(title_tags);

    SetDetails(match_.description);
    ChromeSearchResult::Tags details_tags;
    ACMatchClassificationsToTags(match_.description, match_.description_class,
                                 &details_tags);
    SetDetailsTags(details_tags);
  } else {
    const auto& additional_text =
        GetAdditionalText(match_.answer->first_line());
    // TODO(crbug.com/1130372): Use placeholders or a l10n-friendly way to
    // construct this string instead of concatenation. This currently only
    // happens for stock ticker symbols.
    SetTitle(!additional_text.empty()
                 ? base::JoinString({match_.contents, additional_text}, u" ")
                 : match_.contents);
    SetDetails(TextVectorToString(
        ImageLineToTextVector(match_.answer->second_line())));
  }
}

void OmniboxAnswerResult::FetchImage(const GURL& url) {
  if (!bitmap_fetcher_) {
    bitmap_fetcher_ =
        std::make_unique<BitmapFetcher>(url, this, kOmniboxTrafficAnnotation);
  }
  bitmap_fetcher_->Init(net::ReferrerPolicy::NEVER_CLEAR,
                        network::mojom::CredentialsMode::kOmit);
  bitmap_fetcher_->Start(profile_->GetURLLoaderFactory().get());
}

void OmniboxAnswerResult::OnFetchComplete(const GURL& url,
                                          const SkBitmap* bitmap) {
  if (!bitmap)
    return;

  DCHECK(IsWeatherResult());
  IconInfo icon_info(gfx::ImageSkia::CreateFrom1xBitmap(*bitmap),
                     GetAnswerCardIconDimension());
  SetIcon(icon_info);
}

bool OmniboxAnswerResult::IsCalculatorResult() const {
  return match_.type == AutocompleteMatchType::CALCULATOR;
}

bool OmniboxAnswerResult::IsWeatherResult() const {
  return match_.answer.has_value() &&
         match_.answer->type() == SuggestionAnswer::ANSWER_TYPE_WEATHER;
}

}  // namespace app_list
