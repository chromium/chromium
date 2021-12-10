// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/chrome_search_result.h"

#include <map>

#include "base/containers/adapters.h"
#include "base/logging.h"
#include "chrome/browser/ui/app_list/app_context_menu.h"
#include "ui/base/models/image_model.h"

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

void ChromeSearchResult::SetCategory(Category category) {
  metadata_->category = category;
  SetSearchResultMetadata();
}

void ChromeSearchResult::SetBestMatch(bool best_match) {
  metadata_->best_match = best_match;
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

void ChromeSearchResult::SetIcon(const IconInfo& icon) {
  icon.icon.EnsureRepsForSupportedScales();
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

void ChromeSearchResult::InvokeAction(ash::SearchResultActionType action) {}

void ChromeSearchResult::OnVisibilityChanged(bool visibility) {
  VLOG(1) << " Visibility change to " << visibility << " and ID is " << id();
}

void ChromeSearchResult::GetContextMenuModel(GetMenuModelCallback callback) {
  std::move(callback).Run(nullptr);
}

app_list::AppContextMenu* ChromeSearchResult::GetAppContextMenu() {
  return nullptr;
}

::std::ostream& operator<<(::std::ostream& os,
                           const ChromeSearchResult& result) {
  return os << result.id() << " " << result.scoring();
}
