// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/omnibox_answer_result.h"

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/app_list/vector_icons/vector_icons.h"
#include "ash/public/cpp/style/color_provider.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "chrome/browser/bitmap_fetcher/bitmap_fetcher.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/app_list_controller_delegate.h"
#include "chrome/browser/ui/app_list/search/common/icon_constants.h"
#include "chrome/browser/ui/app_list/search/common/search_result_util.h"
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
  const bool dark_mode = ash::features::IsProductivityLauncherEnabled() ||
                         (ash::features::IsDarkLightModeEnabled() &&
                          ash::ColorProvider::Get()->IsDarkModeEnabled());
  const auto icon =
      dark_mode ? gfx::ImageSkiaOperations::CreateImageWithCircleBackground(
                      dimension / 2, gfx::kGoogleBlue300,
                      gfx::CreateVectorIcon(vector_icon, gfx::kGoogleGrey900))
                : gfx::ImageSkiaOperations::CreateImageWithCircleBackground(
                      dimension / 2, gfx::kGoogleBlue600,
                      gfx::CreateVectorIcon(vector_icon, SK_ColorWHITE));
  return ChromeSearchResult::IconInfo(icon, dimension);
}

TextItem TextFieldToTextItem(const SuggestionAnswer::TextField& text_field) {
  TextItem text_item(TextType::kString);
  text_item.SetText(text_field.text());

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
  text_item.SetTextTags(tags);

  return text_item;
}

// Converts an ImageLine to TextVector, ignoring any additional text.
std::vector<TextItem> ImageLineToTextVector(
    const SuggestionAnswer::ImageLine& line) {
  std::vector<TextItem> text_vector;
  for (const auto& text_field : line.text_fields()) {
    if (!text_vector.empty()) {
      text_vector.push_back(CreateStringTextItem(u" "));
    }
    text_vector.push_back(TextFieldToTextItem(text_field));
  }
  return text_vector;
}

// Converts the line's additional text into a TextItem and appends it to the
// supplied TextVector.
void AppendAdditionalText(const SuggestionAnswer::ImageLine& line,
                          std::vector<TextItem>& text_vector) {
  if (!line.additional_text() || line.additional_text()->text().empty())
    return;

  if (!text_vector.empty()) {
    text_vector.push_back(CreateStringTextItem(u" "));
  }

  text_vector.push_back(TextFieldToTextItem(*line.additional_text()));
}

// Converts AutocompleteMatch fields to a TextItem.
std::vector<TextItem> MatchFieldsToTextVector(
    const std::u16string& text,
    const ACMatchClassifications& classifications) {
  TextItem text_item(TextType::kString);
  text_item.SetText(text);

  ash::SearchResultTags tags;
  // Classifications include URL tags, which we need to convert, and MATCH tags,
  // which we should ignore since the query highlighter will perform all the
  // matching instead.
  ACMatchClassificationsToTags(text, classifications, &tags,
                               /*ignore_match=*/true);
  text_item.SetTextTags(tags);

  return {text_item};
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

std::u16string ComputeAccessibleName(
    const std::vector<std::vector<TextItem>>& text_vectors) {
  std::vector<std::u16string> text;
  for (const auto& text_vector : text_vectors) {
    if (!text_vector.empty()) {
      text.push_back(StringFromTextVector(text_vector));
    }
  }
  return base::JoinString(text, u", ");
}

}  // namespace

OmniboxAnswerResult::OmniboxAnswerResult(
    Profile* profile,
    AppListControllerDelegate* list_controller,
    AutocompleteController* autocomplete_controller,
    const AutocompleteMatch& match,
    const std::u16string& query)
    : profile_(profile),
      list_controller_(list_controller),
      autocomplete_controller_(autocomplete_controller),
      match_(match),
      query_(query) {
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
  if (IsCalculatorResult()) {
    // Calculator results come in two forms:
    // 1) Answer in |match.contents|, empty description,
    // 2) Query in |match.contents|, answer in |match.description|.
    std::vector<TextItem> contents_vector =
        MatchFieldsToTextVector(match_.contents, match_.contents_class);
    if (match_.description.empty()) {
      SetTitleTextVector(contents_vector);
      SetDetailsTextVector({CreateStringTextItem(query_)});
    } else {
      SetTitleTextVector(MatchFieldsToTextVector(match_.description,
                                                 match_.description_class));
      SetDetailsTextVector(contents_vector);
    }
  } else if (IsWeatherResult()) {
    const auto& second_line = match_.answer->second_line();

    SetBigTitleTextVector(ImageLineToTextVector(second_line));
    // TODO(crbug.com/1250154): Put additional weather text into the title
    // field instead of match contents, once the information becomes available
    // from the Suggest server.
    SetTitleTextVector(
        MatchFieldsToTextVector(match_.contents, match_.contents_class));

    std::vector<TextItem> details_vector;
    AppendAdditionalText(second_line, details_vector);
    SetDetailsTextVector(details_vector);
  } else {
    std::vector<TextItem> first_vector =
        MatchFieldsToTextVector(match_.contents, match_.contents_class);
    AppendAdditionalText(match_.answer->first_line(), first_vector);

    const auto& second_line = match_.answer->second_line();
    auto second_vector = ImageLineToTextVector(second_line);
    AppendAdditionalText(second_line, second_vector);

    if (IsDictionaryResult()) {
      SetTitleTextVector(first_vector);
      SetDetailsTextVector(second_vector);
    } else {
      SetTitleTextVector(second_vector);
      SetDetailsTextVector(first_vector);
    }
  }

  // Bold the title field.
  SetTitleTextVector(AddBoldTags(title_text_vector()));

  std::u16string accessible_name = ComputeAccessibleName(
      {big_title_text_vector(), title_text_vector(), details_text_vector()});
  SetAccessibleName(accessible_name);
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
    const auto* additional = match_.answer->first_line().additional_text();
    const std::u16string title =
        additional && !additional->text().empty()
            ? base::JoinString({match_.contents, additional->text()}, u" ")
            : match_.contents;
    SetTitle(title);

    auto details_vector = ImageLineToTextVector(match_.answer->second_line());
    AppendAdditionalText(match_.answer->second_line(), details_vector);
    SetDetails(StringFromTextVector(details_vector));
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

bool OmniboxAnswerResult::IsDictionaryResult() const {
  return match_.answer.has_value() &&
         match_.answer->type() == SuggestionAnswer::ANSWER_TYPE_DICTIONARY;
}

bool OmniboxAnswerResult::IsWeatherResult() const {
  return match_.answer.has_value() &&
         match_.answer->type() == SuggestionAnswer::ANSWER_TYPE_WEATHER;
}

}  // namespace app_list
