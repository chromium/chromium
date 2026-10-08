// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXPANDABLE_CONTAINER_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXPANDABLE_CONTAINER_VIEW_H_

#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/view.h"

namespace views {
class Label;
class Link;
}

// A view that displays a list of details, along with a link that expands and
// collapses those details.
class ExpandableContainerView : public views::View {
  METADATA_HEADER(ExpandableContainerView, views::View)

 public:
  ExpandableContainerView(const std::u16string& visible_details,
                          const std::u16string& collapsed_details);
  ExpandableContainerView(const ExpandableContainerView&) = delete;
  ExpandableContainerView& operator=(const ExpandableContainerView&) = delete;
  ~ExpandableContainerView() override;

  // views::View:
  void ChildPreferredSizeChanged(views::View* child) override;

  // Accessors for testing.
  View* details_view() { return details_view_; }
  void ToggleDetailLevelForTest() { ToggleDetailLevel(); }
  std::u16string_view GetDetailsTextForTest() const;

 private:
  // Helper class representing the list of details, that can hide itself.
  class DetailsView : public views::BoxLayoutView {
    METADATA_HEADER(DetailsView, views::BoxLayoutView)

   public:
    DetailsView(const std::u16string& visible_details,
                const std::u16string& collapsed_details);
    DetailsView(const DetailsView&) = delete;
    DetailsView& operator=(const DetailsView&) = delete;
    ~DetailsView() override;

    // Expands or collapses this view.
    void SetExpanded(bool expanded);
    bool GetExpanded() const;

    std::u16string_view GetText() const;

   private:
    // Whether this details section is expanded.
    bool expanded_ = false;
    raw_ptr<views::Label> label_ = nullptr;
    std::u16string visible_details_;
    std::u16string all_details_;
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
