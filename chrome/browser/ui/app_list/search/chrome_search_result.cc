// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/chrome_search_result.h"

#include <map>

#include "base/containers/adapters.h"
#include "base/logging.h"
#include "chrome/browser/ui/app_list/app_context_menu.h"
#include "chromeos/components/string_matching/tokenized_string.h"
#include "chromeos/components/string_matching/tokenized_string_match.h"
#include "ui/base/models/image_model.h"

namespace {

using chromeos::string_matching::TokenizedString;
using chromeos::string_matching::TokenizedStringMatch;

}  // namespace

ChromeSearchResult::ChromeSearchResult()
    : metadata_(std::make_unique<ash::SearchResultMetadata>()) {}

ChromeSearchResult::~ChromeSearchResult() = default;

void ChromeSearchResult::SetActions(const Actions& actions) {
  metadata_->actions = actions;
  SetSearchResultMetadata();
}

void ChromeSearchResult::SetDisplayScore(double display_score) {
  metadata_->display_score = display_score;
  SetSearchResultMetadata();
}

void ChromeSearchResult::SetIsInstalling(bool is_installing) {
  metadata_->is_installing = is_installing;
  SetSearchResultMetadata();
}

void ChromeSearchResult::SetTitle(const std::u16string& title) {
  metadata_->title = title;
  SetSearchResultMetadata();
}

void ChromeSearchResult::SetTitleTags(const Tags& tags) {
  metadata_->title_tags = tags;
  SetSearchResultMetadata();
}

void ChromeSearchResult::SetDetails(const std::u16string& details) {
  metadata_->details = details;
  SetSearchResultMetadata();
}

void ChromeSearchResult::SetDetailsTags(const Tags& tags) {
  metadata_->details_tags = tags;
  SetSearchResultMetadata();
}

void ChromeSearchResult::SetAccessibleName(const std::u16string& name) {
  metadata_->accessible_name = name;
  SetSearchResultMetadata();
}

void ChromeSearchResult::SetRating(float rating) {
  metadata_->rating = rating;
  SetSearchResultMetadata();
}

void ChromeSearchResult::SetFormattedPrice(
    const std::u16string& formatted_price) {
  metadata_->formatted_price = formatted_price;
  SetSearchResultMetadata();
}

void ChromeSearchResult::SetDisplayType(DisplayType display_type) {
  metadata_->display_type = display_type;
  SetSearchResultMetadata();
}

void ChromeSearchResult::SetResultType(ResultType result_type) {
  metadata_->result_type = result_type;
  SetSearchResultMetadata();
}

void ChromeSearchResult::SetMetricsType(MetricsType metrics_type) {
  metadata_->metrics_type = metrics_type;
  SetSearchResultMetadata();
}

void ChromeSearchResult::SetDisplayIndex(DisplayIndex display_index) {
  metadata_->display_index = display_index;
  SetSearchResultMetadata();
}

void ChromeSearchResult::SetOmniboxType(OmniboxType omnibox_type) {
  metadata_->omnibox_type = omnibox_type;
  SetSearchResultMetadata();
}

void ChromeSearchResult::SetPositionPriority(float position_priority) {
  metadata_->position_priority = position_priority;
  SetSearchResultMetadata();
}

void ChromeSearchResult::SetIsOmniboxSearch(bool is_omnibox_search) {
  metadata_->is_omnibox_search = is_omnibox_search;
  SetSearchResultMetadata();
}

void ChromeSearchResult::SetIsRecommendation(bool is_recommendation) {
  metadata_->is_recommendation = is_recommendation;
  SetSearchResultMetadata();
}

void ChromeSearchResult::SetQueryUrl(const GURL& url) {
  metadata_->query_url = url;
  auto* updater = model_updater();
  if (updater)
    updater->SetSearchResultMetadata(id(), CloneMetadata());
}

void ChromeSearchResult::SetEquivalentResutlId(
    const std::string& equivlanet_result_id) {
  metadata_->equivalent_result_id = equivlanet_result_id;
  auto* updater = model_updater();
  if (updater)
    updater->SetSearchResultMetadata(id(), CloneMetadata());
}

void ChromeSearchResult::SetIcon(const gfx::ImageSkia& icon) {
  icon.EnsureRepsForSupportedScales();
  metadata_->icon = icon;
  SetSearchResultMetadata();
}

void ChromeSearchResult::SetChipIcon(const gfx::ImageSkia& chip_icon) {
  chip_icon.EnsureRepsForSupportedScales();
  metadata_->chip_icon = chip_icon;
  SetSearchResultMetadata();
}

void ChromeSearchResult::SetBadgeIcon(const ui::ImageModel& badge_icon) {
  metadata_->badge_icon = badge_icon;
  SetSearchResultMetadata();
}

void ChromeSearchResult::SetUseBadgeIconBackground(
    bool use_badge_icon_background) {
  metadata_->use_badge_icon_background = use_badge_icon_background;
  SetSearchResultMetadata();
}

void ChromeSearchResult::SetNotifyVisibilityChange(
    bool notify_visibility_change) {
  metadata_->notify_visibility_change = notify_visibility_change;
}

void ChromeSearchResult::SetSearchResultMetadata() {
  AppListModelUpdater* updater = model_updater();
  if (updater)
    updater->SetSearchResultMetadata(id(), CloneMetadata());
}

void ChromeSearchResult::InvokeAction(int action_index) {}

void ChromeSearchResult::OnVisibilityChanged(bool visibility) {
  VLOG(1) << " Visibility change to " << visibility << " and ID is " << id();
}

void ChromeSearchResult::UpdateFromMatch(const TokenizedString& title,
                                         const TokenizedStringMatch& match) {
  const TokenizedStringMatch::Hits& hits = match.hits();

  Tags tags;
  tags.reserve(hits.size());
  for (const auto& hit : hits)
    tags.push_back(Tag(Tag::MATCH, hit.start(), hit.end()));

  SetTitle(title.text());
  SetTitleTags(tags);
  set_relevance(match.relevance());
}

void ChromeSearchResult::GetContextMenuModel(GetMenuModelCallback callback) {
  std::move(callback).Run(nullptr);
}

// static
std::string ChromeSearchResult::TagsDebugStringForTest(const std::string& text,
                                                       const Tags& tags) {
  std::string result = text;

  // Build a table of delimiters to insert.
  std::map<size_t, std::string> inserts;
  for (const auto& tag : tags) {
    if (tag.styles & Tag::URL)
      inserts[tag.range.start()].push_back('{');
    if (tag.styles & Tag::MATCH)
      inserts[tag.range.start()].push_back('[');
    if (tag.styles & Tag::DIM) {
      inserts[tag.range.start()].push_back('<');
      inserts[tag.range.end()].push_back('>');
    }
    if (tag.styles & Tag::MATCH)
      inserts[tag.range.end()].push_back(']');
    if (tag.styles & Tag::URL)
      inserts[tag.range.end()].push_back('}');
  }
  // Insert the delimiters (in reverse order, to preserve indices).
  for (const auto& insert : base::Reversed(inserts))
    result.insert(insert.first, insert.second);

  return result;
}

app_list::AppContextMenu* ChromeSearchResult::GetAppContextMenu() {
  return nullptr;
}
