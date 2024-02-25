// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_ROW_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_ROW_VIEW_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

class OmniboxPopupViewViews;
class OmniboxResultView;
class OmniboxHeaderView;

// The View that's a direct child of the OmniboxPopupViewViews, one per row.
// This, in turn, has a child OmniboxResultView and an optional header that is
// painted right above it. The header is not a child of OmniboxResultView
// because it's logically not part of the result view:
//  - Hovering the header doesn't highlight the result view.
//  - Clicking the header doesn't navigate to the match.
//  - It's the header for multiple matches, it's just painted above this row.
class OmniboxRowView : public views::View {
  METADATA_HEADER(OmniboxRowView, views::View)

 public:
  OmniboxRowView(size_t line, OmniboxPopupViewViews* popup_view);

  // Sets the header that appears above this row. Also shows the header.
  void ShowHeader(const std::u16string& header_text,
                  bool suggestion_group_hidden);

  // Hides the header.
  void HideHeader();

  // The header view associated with this row.
  OmniboxHeaderView* header_view() const { return header_view_; }

  // The result view associated with this row.
  OmniboxResultView* result_view() const { return result_view_; }

  // Invoked when the model's selection state has changed.
  void OnSelectionStateChanged();

  // Fetches the active descendant button for accessibility purposes.
  // Returns nullptr if no descendant auxiliary button is active.
  views::View* GetActiveAuxiliaryButtonForAccessibility() const;

  // views::View:
  gfx::Insets GetInsets() const override;

 private:
  // Line number of this row.
  const size_t line_;

  // Non-owning pointer to the popup view for this row. This is never nullptr.
  const raw_ptr<OmniboxPopupViewViews> popup_view_;

  // Non-owning pointer to the header view for this row. This is initially
  // nullptr, and lazily created when a header is first set for this row.
  // Lazily creating these speeds up browser startup: https://crbug.com/1021323
  raw_ptr<OmniboxHeaderView> header_view_ = nullptr;

  // Non-owning pointer to the result view for this row. This is never nullptr.
  raw_ptr<OmniboxResultView> result_view_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_ROW_VIEW_H_
