// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_CHROME_SEARCH_RESULT_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_CHROME_SEARCH_RESULT_H_

#include <memory>
#include <string>
#include <utility>

#include "ash/public/cpp/app_list/app_list_metrics.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "base/macros.h"
#include "chrome/browser/ui/app_list/app_list_model_updater.h"
#include "ui/base/models/simple_menu_model.h"

namespace chromeos {
namespace string_matching {
class TokenizedString;
class TokenizedStringMatch;
}  // namespace string_matching
}  // namespace chromeos

namespace app_list {
class AppContextMenu;
}  // namespace app_list

namespace ui {
class ImageModel;
}

// ChromeSearchResult consists of an icon, title text and details text. Title
// and details text can have tagged ranges that are displayed differently from
// default style.
class ChromeSearchResult {
 public:
  using ResultType = ash::AppListSearchResultType;
  using DisplayType = ash::SearchResultDisplayType;
  using MetricsType = ash::SearchResultType;
  using Tag = ash::SearchResultTag;
  using Tags = ash::SearchResultTags;
  using Action = ash::SearchResultAction;
  using Actions = ash::SearchResultActions;
  using DisplayIndex = ash::SearchResultDisplayIndex;
  using OmniboxType = ash::SearchResultOmniboxType;

  ChromeSearchResult();
  virtual ~ChromeSearchResult();

  const std::u16string& title() const { return metadata_->title; }
  const Tags& title_tags() const { return metadata_->title_tags; }
  const std::u16string& details() const { return metadata_->details; }
  const Tags& details_tags() const { return metadata_->details_tags; }
  const std::u16string& accessible_name() const {
    return metadata_->accessible_name;
  }
  float rating() const { return metadata_->rating; }
  const std::u16string& formatted_price() const {
    return metadata_->formatted_price;
  }
  const std::string& id() const { return metadata_->id; }
  DisplayType display_type() const { return metadata_->display_type; }
  ash::AppListSearchResultType result_type() const {
    return metadata_->result_type;
  }
  MetricsType metrics_type() const { return metadata_->metrics_type; }
  DisplayIndex display_index() const { return metadata_->display_index; }
  OmniboxType omnibox_type() const { return metadata_->omnibox_type; }
  float position_priority() const { return metadata_->position_priority; }
  const Actions& actions() const { return metadata_->actions; }
  double display_score() const { return metadata_->display_score; }
  bool is_installing() const { return metadata_->is_installing; }
  bool is_recommendation() const { return metadata_->is_recommendation; }
  const base::Optional<GURL>& query_url() const { return metadata_->query_url; }
  const base::Optional<std::string>& equivalent_result_id() const {
    return metadata_->equivalent_result_id;
  }
  const gfx::ImageSkia& icon() const { return metadata_->icon; }
  const gfx::ImageSkia& chip_icon() const { return metadata_->chip_icon; }
  const ui::ImageModel& badge_icon() const { return metadata_->badge_icon; }

  bool notify_visibility_change() const {
    return metadata_->notify_visibility_change;
  }

  // The following methods set Chrome side data here, and call model updater
  // interface to update Ash.
  void SetTitle(const std::u16string& title);
  void SetTitleTags(const Tags& tags);
  void SetDetails(const std::u16string& details);
  void SetDetailsTags(const Tags& tags);
  void SetAccessibleName(const std::u16string& name);
  void SetRating(float rating);
  void SetFormattedPrice(const std::u16string& formatted_price);
  void SetDisplayType(DisplayType display_type);
  void SetResultType(ResultType result_type);
  void SetMetricsType(MetricsType metrics_type);
  void SetDisplayIndex(DisplayIndex display_index);
  void SetOmniboxType(OmniboxType omnibox_type);
  void SetPositionPriority(float position_priority);
  void SetDisplayScore(double display_score);
  void SetActions(const Actions& actions);
  void SetIsOmniboxSearch(bool is_omnibox_search);
  void SetIsRecommendation(bool is_recommendation);
  void SetIsInstalling(bool is_installing);
  void SetQueryUrl(const GURL& url);
  void SetEquivalentResutlId(const std::string& equivlanet_result_id);
  void SetIcon(const gfx::ImageSkia& icon);
  void SetChipIcon(const gfx::ImageSkia& icon);
  void SetBadgeIcon(const ui::ImageModel& badge_icon);
  void SetUseBadgeIconBackground(bool use_badge_icon_background);
  void SetNotifyVisibilityChange(bool notify_visibility_change);

  void SetSearchResultMetadata();

  void SetMetadata(std::unique_ptr<ash::SearchResultMetadata> metadata) {
    metadata_ = std::move(metadata);
  }
  std::unique_ptr<ash::SearchResultMetadata> CloneMetadata() const {
    return std::make_unique<ash::SearchResultMetadata>(*metadata_);
  }

  void set_model_updater(AppListModelUpdater* model_updater) {
    model_updater_ = model_updater;
  }
  AppListModelUpdater* model_updater() const { return model_updater_; }

  double relevance() const { return relevance_; }
  void set_relevance(double relevance) { relevance_ = relevance; }

  bool dismiss_view_on_open() const { return dismiss_view_on_open_; }
  void set_dismiss_view_on_open(bool dismiss_view_on_open) {
    dismiss_view_on_open_ = dismiss_view_on_open;
  }

  // Invokes a custom action on the result. It does nothing by default.
  virtual void InvokeAction(int action_index);

  // Opens the result. Clients should use AppListViewDelegate::OpenSearchResult.
  virtual void Open(int event_flags) = 0;

  // Called if set visible/hidden.
  virtual void OnVisibilityChanged(bool visibility);

  // Updates the result's relevance score, and sets its title and title tags,
  // based on a string match result.
  void UpdateFromMatch(
      const chromeos::string_matching::TokenizedString& title,
      const chromeos::string_matching::TokenizedStringMatch& match);

  // Returns the context menu model for this item, or NULL if there is currently
  // no menu for the item (e.g. during install). |callback| takes the ownership
  // of the returned menu model.
  using GetMenuModelCallback =
      base::OnceCallback<void(std::unique_ptr<ui::SimpleMenuModel>)>;
  virtual void GetContextMenuModel(GetMenuModelCallback callback);

  static std::string TagsDebugStringForTest(const std::string& text,
                                            const Tags& tags);

  // Subtype of a search result. -1 means no sub type. Derived classes
  // can set this in their metadata to return useful values for rankers etc.
  // Note set_result_subtype() does not call into ModelUpdater so changing the
  // subtype after construction is not reflected in ash.
  int result_subtype() const { return metadata_->result_subtype; }

 protected:
  // These id setters should be called in derived class constructors only.
  void set_id(const std::string& id) { metadata_->id = id; }
  void set_result_subtype(int result_subtype) {
    metadata_->result_subtype = result_subtype;
  }

  // Get the context menu of a certain search result. This could be different
  // for different kinds of items.
  virtual app_list::AppContextMenu* GetAppContextMenu();

 private:
  // The relevance of this result to the search, which is determined by the
  // search query. It's used for sorting when we publish the results to the
  // SearchModel in Ash. We'll update metadata_->display_score based on the
  // sorted order, group multiplier and group boost.
  double relevance_ = 0;

  // More often than not, calling Open() on a ChromeSearchResult will cause the
  // app list view to be closed as a side effect. Because opening apps can take
  // some time, the app list view is eagerly dismissed by default after invoking
  // Open() for added polish. Some ChromeSearchResults may not appreciate this
  // behavior so it can be disabled as needed.
  bool dismiss_view_on_open_ = true;

  std::unique_ptr<ash::SearchResultMetadata> metadata_;

  AppListModelUpdater* model_updater_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(ChromeSearchResult);
};

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_CHROME_SEARCH_RESULT_H_
