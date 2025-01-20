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

// Interface to PageActionModel, used for either the concrete implementation or
// a mock for testing.
class PageActionModelInterface {
 public:
  PageActionModelInterface() = default;
  virtual ~PageActionModelInterface() = default;

  virtual void AddObserver(PageActionModelObserver* observer) = 0;
  virtual void RemoveObserver(PageActionModelObserver* observer) = 0;

  virtual void SetActionItemProperties(
      base::PassKey<PageActionController>,
      const actions::ActionItem* action_item) = 0;
  virtual void SetShowRequested(base::PassKey<PageActionController>,
                                bool requested) = 0;
  virtual void SetOverrideText(
      base::PassKey<PageActionController>,
      const std::optional<std::u16string>& override_text) = 0;

  virtual bool GetVisible() const = 0;
  virtual const ui::ImageModel& GetImage() const = 0;
  virtual const std::u16string GetText() const = 0;
  virtual const std::u16string GetTooltipText() const = 0;
};

// PageActionModel represents the page action's state, scoped to a single tab.
class PageActionModel : public PageActionModelInterface {
 public:
  PageActionModel();
  PageActionModel(const PageActionModel&) = delete;
  PageActionModel& operator=(const PageActionModel&) = delete;
  ~PageActionModel() override;

  void AddObserver(PageActionModelObserver* observer) override;
  void RemoveObserver(PageActionModelObserver* observer) override;

  // Applies any relevant ActionItem properties to the model, including
  // visibility, text and icon properties.
  void SetActionItemProperties(base::PassKey<PageActionController>,
                               const actions::ActionItem* action_item) override;
  void SetShowRequested(base::PassKey<PageActionController>,
                        bool requested) override;
  void SetOverrideText(
      base::PassKey<PageActionController>,
      const std::optional<std::u16string>& override_text) override;

  // The model distills all visibility properties into a single result.
  bool GetVisible() const override;

  const ui::ImageModel& GetImage() const override;
  const std::u16string GetText() const override;
  const std::u16string GetTooltipText() const override;

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
