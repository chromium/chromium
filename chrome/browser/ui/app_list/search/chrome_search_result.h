// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_CHROME_SEARCH_RESULT_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_CHROME_SEARCH_RESULT_H_

#include <memory>
#include <string>
#include <utility>

#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/public/interfaces/app_list.mojom.h"
#include "base/macros.h"
#include "base/unguessable_token.h"
#include "chrome/browser/ui/app_list/app_list_model_updater.h"

namespace app_list {
class AppContextMenu;
class TokenizedString;
class TokenizedStringMatch;
}  // namespace app_list

// ChromeSearchResult consists of an icon, title text and details text. Title
// and details text can have tagged ranges that are displayed differently from
// default style.
class ChromeSearchResult {
 public:
  using ResultType = ash::SearchResultType;
  using DisplayType = ash::SearchResultDisplayType;
  using Tag = ash::SearchResultTag;
  using Tags = ash::SearchResultTags;
  using Action = ash::SearchResultAction;
  using Actions = ash::SearchResultActions;

  ChromeSearchResult();
  virtual ~ChromeSearchResult();

  const base::string16& title() const { return metadata_->title; }
  const Tags& title_tags() const { return metadata_->title_tags; }
  const base::string16& details() const { return metadata_->details; }
  const Tags& details_tags() const { return metadata_->details_tags; }
  float rating() const { return metadata_->rating; }
  const base::string16& formatted_price() const {
    return metadata_->formatted_price;
  }
  const std::string& id() const { return metadata_->id; }
  DisplayType display_type() const { return metadata_->display_type; }
  ResultType result_type() const { return metadata_->result_type; }
  const Actions& actions() const { return metadata_->actions; }
  double display_score() const { return metadata_->display_score; }
  bool is_installing() const { return metadata_->is_installing; }
  const base::UnguessableToken& answer_card_contents_token() const {
    return metadata_->answer_card_contents_token.value();
  }
  const gfx::ImageSkia& icon() const { return metadata_->icon; }
  const gfx::ImageSkia& chip_icon() const { return metadata_->chip_icon; }
  const gfx::ImageSkia& badge_icon() const { return metadata_->badge_icon; }

  const std::string& comparable_id() const { return comparable_id_; }

  // The following methods set Chrome side data here, and call model updater
  // interface to update Ash.
  void SetTitle(const base::string16& title);
  void SetTitleTags(const Tags& tags);
  void SetDetails(const base::string16& details);
  void SetDetailsTags(const Tags& tags);
  void SetAccessibleName(const base::string16& name);
  void SetRating(float rating);
  void SetFormattedPrice(const base::string16& formatted_price);
  void SetDisplayType(DisplayType display_type);
  void SetResultType(ResultType result_type);
  void SetDisplayScore(double display_score);
  void SetActions(const Actions& actions);
  void SetIsOmniboxSearch(bool is_omnibox_search);
  void SetAnswerCardContentsToken(const base::UnguessableToken& token);
  void SetAnswerCardSize(const gfx::Size& size);
  void SetIsInstalling(bool is_installing);
  void SetIcon(const gfx::ImageSkia& icon);
  void SetChipIcon(const gfx::ImageSkia& icon);
  void SetBadgeIcon(const gfx::ImageSkia& badge_icon);

  // The following methods call model updater to update Ash.
  void SetPercentDownloaded(int percent_downloaded);
  void NotifyItemInstalled();

  void SetMetadata(ash::mojom::SearchResultMetadataPtr metadata) {
    metadata_ = std::move(metadata);
  }
  ash::mojom::SearchResultMetadataPtr CloneMetadata() const {
    return metadata_.Clone();
  }

  void set_model_updater(AppListModelUpdater* model_updater) {
    model_updater_ = model_updater;
  }
  AppListModelUpdater* model_updater() const { return model_updater_; }

  double relevance() const { return relevance_; }
  void set_relevance(double relevance) { relevance_ = relevance; }

  // Invokes a custom action on the result. It does nothing by default.
  virtual void InvokeAction(int action_index, int event_flags);

  // Opens the result. Clients should use AppListViewDelegate::OpenSearchResult.
  virtual void Open(int event_flags) = 0;

  // Updates the result's relevance score, and sets its title and title tags,
  // based on a string match result.
  void UpdateFromMatch(const app_list::TokenizedString& title,
                       const app_list::TokenizedStringMatch& match);

  // Returns the context menu model for this item, or NULL if there is currently
  // no menu for the item (e.g. during install). |callback| takes the ownership
  // of the returned menu model.
  using GetMenuModelCallback =
      base::OnceCallback<void(std::unique_ptr<ui::MenuModel>)>;
  virtual void GetContextMenuModel(GetMenuModelCallback callback);

  // Invoked when a context menu item of this search result is selected.
  void ContextMenuItemSelected(int command_id, int event_flags);

  static std::string TagsDebugStringForTest(const std::string& text,
                                            const Tags& tags);

 protected:
  // These id setters should be called in derived class constructors only.
  void set_id(const std::string& id) { metadata_->id = id; }
  void set_comparable_id(const std::string& comparable_id) {
    comparable_id_ = comparable_id;
  }

  // Get the context menu of a certain search result. This could be different
  // for different kinds of items.
  virtual app_list::AppContextMenu* GetAppContextMenu();

 private:
  // ID that can be compared across results from different providers to remove
  // duplicates. May be empty, in which case |id_| will be used for comparison.
  std::string comparable_id_;

  // The relevance of this result to the search, which is determined by the
  // search query. It's used for sorting when we publish the results to the
  // SearchModel in Ash. We'll update metadata_->display_score based on the
  // sorted order, group multiplier and group boost.
  double relevance_ = 0;

  ash::mojom::SearchResultMetadataPtr metadata_;

  AppListModelUpdater* model_updater_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(ChromeSearchResult);
};

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_CHROME_SEARCH_RESULT_H_
