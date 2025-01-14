// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_PAGE_ACTION_MODEL_H_
#define CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_PAGE_ACTION_MODEL_H_

#include <string>

#include "base/observer_list.h"
#include "base/types/pass_key.h"
#include "ui/base/models/image_model.h"

namespace actions {
class ActionItem;
}  // namespace actions

namespace page_actions {

class PageActionController;
class PageActionModelObserver;

// PageActionModel represents the page action's state, scoped to a single tab.
class PageActionModel {
 public:
  PageActionModel();
  PageActionModel(const PageActionModel&) = delete;
  PageActionModel& operator=(const PageActionModel&) = delete;
  ~PageActionModel();

  void AddObserver(PageActionModelObserver* observer);
  void RemoveObserver(PageActionModelObserver* observer);

  // Applies any relevant ActionItem properties to the model, including
  // visibility, text and icon properties.
  void SetActionItemProperties(base::PassKey<PageActionController>,
                               const actions::ActionItem* action_item);

  void SetShowRequested(base::PassKey<PageActionController>, bool requested);
  void SetOverrideText(base::PassKey<PageActionController>,
                       const std::optional<std::u16string>& override_text);

  // The model distills all visibility properties into a single result.
  bool GetVisible() const;

  const ui::ImageModel& GetImage() const;
  const std::u16string GetText() const;
  const std::u16string GetTooltipText() const;

 private:
  // Notifies observers of a model change.
  void NotifyChange();

  // Represents whether a feature requested to show this page action.
  bool show_requested_ = false;

  // Properties taken from ActionItem.
  bool action_item_enabled_ = false;
  bool action_item_visible_ = false;
  std::u16string text_;
  // When set, it will always take precedence over `text_`.
  std::optional<std::u16string> override_text_;
  std::u16string tooltip_;
  ui::ImageModel action_item_image_;

  base::ObserverList<PageActionModelObserver> observer_list_;
};

}  // namespace page_actions

#endif  // CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_PAGE_ACTION_MODEL_H_
