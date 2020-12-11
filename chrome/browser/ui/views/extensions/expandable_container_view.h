// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXPANDABLE_CONTAINER_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXPANDABLE_CONTAINER_VIEW_H_

#include <vector>

#include "base/strings/string16.h"
#include "ui/views/view.h"

namespace views {
class Link;
}

// A view that displays a list of details, along with a link that expands and
// collapses those details.
class ExpandableContainerView : public views::View {
 public:
  ExpandableContainerView(const std::vector<base::string16>& details,
                          int available_width);
  ~ExpandableContainerView() override;

  // views::View:
  void ChildPreferredSizeChanged(views::View* child) override;

  ExpandableContainerView(const ExpandableContainerView&) = delete;
  ExpandableContainerView& operator=(const ExpandableContainerView&) = delete;

  // Accessors for testing.
  View* details_view() { return details_view_; }
  void ToggleDetailLevelForTest() { ToggleDetailLevel(); }

 private:
  // Helper class representing the list of details, that can hide itself.
  class DetailsView : public views::View {
   public:
    explicit DetailsView(const std::vector<base::string16>& details);
    ~DetailsView() override;

    // Expands or collapses this view.
    void ToggleExpanded();

    bool expanded() { return expanded_; }

   private:
    // Whether this details section is expanded.
    bool expanded_ = false;

    DetailsView(const DetailsView&) = delete;
    DetailsView& operator=(const DetailsView&) = delete;
  };

  // Expands or collapses |details_view_|.
  void ToggleDetailLevel();

  // The view that expands or collapses when |details_link_| is clicked.
  DetailsView* details_view_ = nullptr;

  // The 'Show Details' link, which changes to 'Hide Details' when the details
  // section is expanded.
  views::Link* details_link_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXPANDABLE_CONTAINER_VIEW_H_
