// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_APP_MENU_APP_MENU_SEARCH_BAR_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_APP_MENU_APP_MENU_SEARCH_BAR_VIEW_H_

#include <string_view>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/layout/box_layout_view.h"

// Search bar placed at the top of the Block Style ChroMenu: a search field
// above a full-width separator.
class AppMenuSearchBarView : public views::BoxLayoutView {
  METADATA_HEADER(AppMenuSearchBarView, views::BoxLayoutView)

 public:
  // The icon occupying the leading slot.
  enum class LeadingIcon { kSearch, kBack };

  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kSearchFieldElementId);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kSearchIconElementId);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kBackButtonElementId);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kClearButtonElementId);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kSeparatorElementId);

  AppMenuSearchBarView();
  AppMenuSearchBarView(const AppMenuSearchBarView&) = delete;
  AppMenuSearchBarView& operator=(const AppMenuSearchBarView&) = delete;
  ~AppMenuSearchBarView() override;

  void SetLeadingIcon(LeadingIcon icon);

  std::u16string_view GetText() const;
  void SetText(std::u16string_view text);
  [[nodiscard]] base::CallbackListSubscription AddTextChangedCallback(
      views::PropertyChangedCallback callback);

  // Runs `callback` when the back button is pressed, before the query is
  // cleared.
  [[nodiscard]] base::CallbackListSubscription AddBackPressedCallback(
      base::RepeatingClosure callback);

 private:
  // The textfield, with the focus and key handling it needs inside the menu.
  class SearchField;

  raw_ptr<SearchField> search_field_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_VIEWS_APP_MENU_APP_MENU_SEARCH_BAR_VIEW_H_
