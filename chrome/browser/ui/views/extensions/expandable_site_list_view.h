// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXPANDABLE_SITE_LIST_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXPANDABLE_SITE_LIST_VIEW_H_

#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace views {
class Link;
}

// A view that displays a bulleted list of host permission sites, along with an
// optional button-styled link that expands and collapses additional sites into
// a two-column layout.
class ExpandableSiteListView : public views::View {
  METADATA_HEADER(ExpandableSiteListView, views::View)

 public:
  ExpandableSiteListView(
      const std::u16string& visible_details,
      const std::u16string& collapsed_details,
      base::RepeatingClosure on_toggle_callback = base::RepeatingClosure());
  ExpandableSiteListView(const ExpandableSiteListView&) = delete;
  ExpandableSiteListView& operator=(const ExpandableSiteListView&) = delete;
  ~ExpandableSiteListView() override;

  // views::View:
  void ChildPreferredSizeChanged(views::View* child) override;

  // Returns the newline-joined list of currently displayed sites without
  // leading bullet characters.
  std::u16string GetSitesText() const;

  // Accessors for testing.
  views::View* details_view_for_testing() { return details_view_; }
  views::Link* details_link_for_testing() { return details_link_; }
  bool IsExpandedForTesting() const;
  void ToggleDetailLevelForTest() { ToggleDetailLevel(); }

 private:
  class DetailsView : public views::View {
    METADATA_HEADER(DetailsView, views::View)

   public:
    DetailsView(const std::u16string& visible_details,
                const std::u16string& collapsed_details);
    DetailsView(const DetailsView&) = delete;
    DetailsView& operator=(const DetailsView&) = delete;
    ~DetailsView() override;

    void SetExpanded(bool expanded);
    bool GetExpanded() const;
    std::u16string GetSitesText() const;

   private:
    void RebuildSiteListColumns();

    bool expanded_ = false;
    std::vector<std::u16string> visible_sites_;
    std::vector<std::u16string> all_sites_;
  };

  void ToggleDetailLevel();
  void UpdateDetailsLinkState();

  base::RepeatingClosure on_toggle_callback_;
  raw_ptr<DetailsView> details_view_ = nullptr;

  // The "Show all" toggle control, which changes to "Show less" when the site
  // list is expanded. Null when `collapsed_details` is empty.
  raw_ptr<views::Link> details_link_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXPANDABLE_SITE_LIST_VIEW_H_
