// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/chrome_search_result.h"

#include <map>

#include "ash/public/cpp/app_list/tokenized_string.h"
#include "ash/public/cpp/app_list/tokenized_string_match.h"
#include "base/containers/adapters.h"
#include "chrome/browser/ui/app_list/app_context_menu.h"

ChromeSearchResult::ChromeSearchResult()
    : metadata_(ash::mojom::SearchResultMetadata::New()) {}

ChromeSearchResult::~ChromeSearchResult() = default;

void ChromeSearchResult::SetActions(const Actions& actions) {
  metadata_->actions = actions;
  AppListModelUpdater* updater = model_updater();
  if (updater)
    updater->SetSearchResultMetadata(id(), CloneMetadata());
}

void ChromeSearchResult::SetDisplayScore(double display_score) {
  metadata_->display_score = display_score;
  AppListModelUpdater* updater = model_updater();
  if (updater)
    updater->SetSearchResultMetadata(id(), CloneMetadata());
}

void ChromeSearchResult::SetIsInstalling(bool is_installing) {
  metadata_->is_installing = is_installing;
  AppListModelUpdater* updater = model_updater();
  if (updater)
    updater->SetSearchResultMetadata(id(), CloneMetadata());
}

void ChromeSearchResult::SetTitle(const base::string16& title) {
  metadata_->title = title;
  AppListModelUpdater* updater = model_updater();
  if (updater)
    updater->SetSearchResultMetadata(id(), CloneMetadata());
}

void ChromeSearchResult::SetTitleTags(const Tags& tags) {
  metadata_->title_tags = tags;
  AppListModelUpdater* updater = model_updater();
  if (updater)
    updater->SetSearchResultMetadata(id(), CloneMetadata());
}

void ChromeSearchResult::SetDetails(const base::string16& details) {
  metadata_->details = details;
  AppListModelUpdater* updater = model_updater();
  if (updater)
    updater->SetSearchResultMetadata(id(), CloneMetadata());
}

void ChromeSearchResult::SetDetailsTags(const Tags& tags) {
  metadata_->details_tags = tags;
  AppListModelUpdater* updater = model_updater();
  if (updater)
    updater->SetSearchResultMetadata(id(), CloneMetadata());
}

void ChromeSearchResult::SetAccessibleName(const base::string16& name) {
  metadata_->accessible_name = name;
  AppListModelUpdater* updater = model_updater();
  if (updater)
    updater->SetSearchResultMetadata(id(), CloneMetadata());
}

void ChromeSearchResult::SetRating(float rating) {
  metadata_->rating = rating;
  AppListModelUpdater* updater = model_updater();
  if (updater)
    updater->SetSearchResultMetadata(id(), CloneMetadata());
}

void ChromeSearchResult::SetFormattedPrice(
    const base::string16& formatted_price) {
  metadata_->formatted_price = formatted_price;
  AppListModelUpdater* updater = model_updater();
  if (updater)
    updater->SetSearchResultMetadata(id(), CloneMetadata());
}

void ChromeSearchResult::SetDisplayType(DisplayType display_type) {
  metadata_->display_type = display_type;
  AppListModelUpdater* updater = model_updater();
  if (updater)
    updater->SetSearchResultMetadata(id(), CloneMetadata());
}

void ChromeSearchResult::SetResultType(ResultType result_type) {
  metadata_->result_type = result_type;
  AppListModelUpdater* updater = model_updater();
  if (updater)
    updater->SetSearchResultMetadata(id(), CloneMetadata());
}

void ChromeSearchResult::SetIsOmniboxSearch(bool is_omnibox_search) {
  metadata_->is_omnibox_search = is_omnibox_search;
  AppListModelUpdater* updater = model_updater();
  if (updater)
    updater->SetSearchResultMetadata(id(), CloneMetadata());
}

void ChromeSearchResult::SetAnswerCardContentsToken(
    const base::UnguessableToken& token) {
  metadata_->answer_card_contents_token = token;
  AppListModelUpdater* updater = model_updater();
  if (updater)
    updater->SetSearchResultMetadata(id(), CloneMetadata());
}

void ChromeSearchResult::SetAnswerCardSize(const gfx::Size& size) {
  metadata_->answer_card_size = size;
  AppListModelUpdater* updater = model_updater();
  if (updater)
    updater->SetSearchResultMetadata(id(), CloneMetadata());
}

void ChromeSearchResult::SetPercentDownloaded(int percent_downloaded) {
  AppListModelUpdater* updater = model_updater();
  if (updater)
    updater->SetSearchResultPercentDownloaded(id(), percent_downloaded);
}

void ChromeSearchResult::SetIcon(const gfx::ImageSkia& icon) {
  icon.EnsureRepsForSupportedScales();
  metadata_->icon = icon;
  AppListModelUpdater* updater = model_updater();
  if (updater)
    updater->SetSearchResultMetadata(id(), CloneMetadata());
}

void ChromeSearchResult::SetChipIcon(const gfx::ImageSkia& chip_icon) {
  chip_icon.EnsureRepsForSupportedScales();
  metadata_->chip_icon = chip_icon;
  AppListModelUpdater* updater = model_updater();
  if (updater)
    updater->SetSearchResultMetadata(id(), CloneMetadata());
}

void ChromeSearchResult::SetBadgeIcon(const gfx::ImageSkia& badge_icon) {
  badge_icon.EnsureRepsForSupportedScales();
  metadata_->badge_icon = badge_icon;
  AppListModelUpdater* updater = model_updater();
  if (updater)
    updater->SetSearchResultMetadata(id(), CloneMetadata());
}

void ChromeSearchResult::NotifyItemInstalled() {
  AppListModelUpdater* updater = model_updater();
  if (updater)
    updater->NotifySearchResultItemInstalled(id());
}

void ChromeSearchResult::InvokeAction(int action_index, int event_flags) {}

void ChromeSearchResult::UpdateFromMatch(
    const app_list::TokenizedString& title,
    const app_list::TokenizedStringMatch& match) {
  const app_list::TokenizedStringMatch::Hits& hits = match.hits();

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

void ChromeSearchResult::ContextMenuItemSelected(int command_id,
                                                 int event_flags) {
  if (GetAppContextMenu())
    GetAppContextMenu()->ExecuteCommand(command_id, event_flags);
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
