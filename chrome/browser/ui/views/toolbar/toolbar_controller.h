// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_TOOLBAR_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_TOOLBAR_CONTROLLER_H_

#include <vector>

#include "base/gtest_prod_util.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "ui/views/view.h"

// Manages toolbar elements' visibility using flex rules.
class ToolbarController {
 public:
  ToolbarController(std::vector<ui::ElementIdentifier> element_ids,
                    int element_flex_order_start,
                    views::View* toolbar_container_view);
  ToolbarController(const ToolbarController&) = delete;
  ToolbarController& operator=(const ToolbarController&) = delete;
  ~ToolbarController();

 private:
  FRIEND_TEST_ALL_PREFIXES(ToolbarControllerTest, FlexOrderCorrect);

  // Searches for a toolbar element from `toolbar_container_view_` with `id`.
  views::View* FindToolbarElementWithId(ui::ElementIdentifier id) {
    return const_cast<views::View*>(
        std::as_const(*this).FindToolbarElementWithId(id));
  }
  const views::View* FindToolbarElementWithId(ui::ElementIdentifier id) const;

  // The toolbar elements managed by this controller.
  // Order matters as each will be assigned with a flex order that increments by
  // 1 starting from `element_flex_order_start_`. So the last element drops out
  // first once overflow starts.
  const std::vector<ui::ElementIdentifier> element_ids_;

  // The starting flex order assigned to the first element in `elements_ids_`.
  const int element_flex_order_start_;

  // Reference to ToolbarView::container_view_. Must outlive `this`.
  const raw_ptr<views::View> toolbar_container_view_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_TOOLBAR_CONTROLLER_H_
