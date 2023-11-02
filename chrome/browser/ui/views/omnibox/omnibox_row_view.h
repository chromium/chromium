// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_ROW_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_ROW_VIEW_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "third_party/omnibox_proto/groups.pb.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

class OmniboxEditModel;
class OmniboxResultView;
class PrefService;

// The View that's a direct child of the OmniboxPopupContentsView, one per row.
// This, in turn, has a child OmniboxResultView and an optional header that is
// painted right above it. The header is not a child of OmniboxResultView
// because it's logically not part of the result view:
//  - Hovering the header doesn't highlight the result view.
//  - Clicking the header doesn't navigate to the match.
//  - It's the header for multiple matches, it's just painted above this row.
class OmniboxRowView : public views::View {
 public:
  METADATA_HEADER(OmniboxRowView);
  OmniboxRowView(size_t line,
                 OmniboxEditModel* model,
                 std::unique_ptr<OmniboxResultView> result_view,
                 PrefService* pref_service);

  // Sets the header that appears above this row. Also shows the header.
  void ShowHeader(omnibox::GroupId suggestion_group_id,
                  const std::u16string& header_text);

  // Hides the header.
  void HideHeader();

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
  class HeaderView;

  // Line number of this row.
  const size_t line_;

  // Non-owning pointer to the backing model.
  const raw_ptr<OmniboxEditModel> model_;

  // Non-owning pointer to the header view for this row. This is initially
  // nullptr, and lazily created when a header is first set for this row.
  // Lazily creating these speeds up browser startup: https://crbug.com/1021323
  raw_ptr<HeaderView> header_view_ = nullptr;

  // Non-owning pointer to the result view for this row. This is never nullptr.
  raw_ptr<OmniboxResultView> result_view_;

  // Non-owning pointer to the preference service used for toggling headers.
  // May be nullptr in tests.
  const raw_ptr<PrefService> pref_service_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_ROW_VIEW_H_
