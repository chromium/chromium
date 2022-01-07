// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/omnibox_answer_result.h"

#include "ash/public/cpp/app_list/app_list_config.h"
#include "ash/public/cpp/app_list/vector_icons/vector_icons.h"
#include "chrome/browser/bitmap_fetcher/bitmap_fetcher.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/app_list_controller_delegate.h"
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
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/paint_vector_icon.h"
#include "url/gurl.h"
#include "url/url_canon.h"

namespace app_list {
namespace {

constexpr char kOmniboxAnswerSchema[] = "omnibox_answer://";

ChromeSearchResult::IconInfo CreateAnswerIconInfo(
    const gfx::VectorIcon& vector_icon) {
  const int dimension =
      ash::SharedAppListConfig::instance().search_list_answer_icon_dimension();
  const auto icon = gfx::ImageSkiaOperations::CreateImageWithCircleBackground(
      dimension / 2, gfx::kGoogleBlue600,
      gfx::CreateVectorIcon(vector_icon, SK_ColorWHITE));
  return ChromeSearchResult::IconInfo(icon, dimension);
}

absl::optional<std::u16string> GetAdditionalText(
    const SuggestionAnswer::ImageLine& line) {
  if (line.additional_text()) {
    const auto additional_text = line.additional_text()->text();
    if (!additional_text.empty())
      return additional_text;
  }
  return absl::nullopt;
}

std::u16string ImageLineToString16(const SuggestionAnswer::ImageLine& line) {
  std::vector<std::u16string> text;
  for (const auto& text_field : line.text_fields()) {
    text.push_back(text_field.text());
  }
  const auto& additional_text = GetAdditionalText(line);
  if (additional_text) {
    text.push_back(additional_text.value());
  }
  // TODO(crbug.com/1130372): Use placeholders or a l10n-friendly way to
  // construct this string instead of concatenation. This currently only happens
  // for stock ticker symbols.
  return base::JoinString(text, u" ");
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
  SetDisplayType(DisplayType::kList);
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
  UpdateTitleAndDetails();
}

OmniboxAnswerResult::~OmniboxAnswerResult() = default;

void OmniboxAnswerResult::Open(int event_flags) {
  list_controller_->OpenURL(profile_, match_.destination_url, match_.transition,
                            ui::DispositionFromEventFlags(event_flags));
}

void OmniboxAnswerResult::UpdateIcon() {
  if (IsCalculatorResult()) {
    SetIcon(CreateAnswerIconInfo(omnibox::kCalculatorIcon));
  } else if (match_.answer->type() == SuggestionAnswer::ANSWER_TYPE_WEATHER &&
             !match_.answer->image_url().is_empty()) {
    // Weather icons are downloaded. Check this first so that the local
    // default answer icon can be used as a fallback if the URL is missing.
    FetchImage(match_.answer->image_url());
  } else {
    SetIcon(CreateAnswerIconInfo(
        AutocompleteMatch::AnswerTypeToAnswerIcon(match_.answer->type())));
  }
  return;
}

void OmniboxAnswerResult::UpdateTitleAndDetails() {
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
    SetTitle(
        additional_text
            ? base::JoinString({match_.contents, additional_text.value()}, u" ")
            : match_.contents);
    SetDetails(ImageLineToString16(match_.answer->second_line()));
  }
}

void OmniboxAnswerResult::FetchImage(const GURL& url) {
  if (!bitmap_fetcher_) {
    bitmap_fetcher_ =
        std::make_unique<BitmapFetcher>(url, this, kOmniboxTrafficAnnotation);
  }
  bitmap_fetcher_->Init(/*referrer=*/std::string(),
                        net::ReferrerPolicy::NEVER_CLEAR,
                        network::mojom::CredentialsMode::kOmit);
  bitmap_fetcher_->Start(profile_->GetURLLoaderFactory().get());
}

void OmniboxAnswerResult::OnFetchComplete(const GURL& url,
                                          const SkBitmap* bitmap) {
  if (!bitmap)
    return;

  IconInfo icon_info(gfx::ImageSkia::CreateFrom1xBitmap(*bitmap));
  CHECK(match_.answer->type() == SuggestionAnswer::ANSWER_TYPE_WEATHER);
  icon_info.dimension =
      ash::SharedAppListConfig::instance().search_list_answer_icon_dimension();
  SetIcon(icon_info);
}

bool OmniboxAnswerResult::IsCalculatorResult() const {
  return match_.type == AutocompleteMatchType::CALCULATOR;
}

}  // namespace app_list
