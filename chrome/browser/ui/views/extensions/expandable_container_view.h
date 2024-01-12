// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXPANDABLE_CONTAINER_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXPANDABLE_CONTAINER_VIEW_H_

#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace views {
class Link;
}

// A view that displays a list of details, along with a link that expands and
// collapses those details.
class ExpandableContainerView : public views::View {
  METADATA_HEADER(ExpandableContainerView, views::View)

 public:
  explicit ExpandableContainerView(const std::vector<std::u16string>& details);
  ExpandableContainerView(const ExpandableContainerView&) = delete;
  ExpandableContainerView& operator=(const ExpandableContainerView&) = delete;
  ~ExpandableContainerView() override;

  // views::View:
  void ChildPreferredSizeChanged(views::View* child) override;

  // Accessors for testing.
  View* details_view() { return details_view_; }
  void ToggleDetailLevelForTest() { ToggleDetailLevel(); }

 private:
  // Helper class representing the list of details, that can hide itself.
  class DetailsView : public views::View {
    METADATA_HEADER(DetailsView, views::View)

   public:
    explicit DetailsView(const std::vector<std::u16string>& details);
    DetailsView(const DetailsView&) = delete;
    DetailsView& operator=(const DetailsView&) = delete;
    ~DetailsView() override;

    // Expands or collapses this view.
    void SetExpanded(bool expanded);
    bool GetExpanded() const;

   private:
    // Whether this details section is expanded.
    bool expanded_ = false;
  };

  // Expands or collapses |details_view_|.
  void ToggleDetailLevel();

  // The view that expands or collapses when |details_link_| is clicked.
  raw_ptr<DetailsView> details_view_ = nullptr;

  // The 'Show Details' link, which changes to 'Hide Details' when the details
  // section is expanded.
  raw_ptr<views::Link> details_link_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXPANDABLE_CONTAINER_VIEW_H_
