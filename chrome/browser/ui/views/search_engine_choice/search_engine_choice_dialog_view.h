// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SEARCH_ENGINE_CHOICE_SEARCH_ENGINE_CHOICE_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_SEARCH_ENGINE_CHOICE_SEARCH_ENGINE_CHOICE_DIALOG_VIEW_H_

#include "base/memory/weak_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

class Browser;

namespace views {
class WebView;
}

// Implements the Search Engine Choice dialog as a View. The view contains a
// WebView into which is loaded a WebUI page which renders the actual dialog
// content.
class SearchEngineChoiceDialogView : public views::View {
 public:
  METADATA_HEADER(SearchEngineChoiceDialogView);
  SearchEngineChoiceDialogView(
      Browser* browser,
      absl::optional<gfx::Size> boundary_dimensions_for_test,
      absl::optional<double> zoom_factor_for_test);
  ~SearchEngineChoiceDialogView() override;

  // Initialize SearchEngineChoiceDialogView's web_view_ element.
  void Initialize();

 private:
  // Show the dialog widget.
  void ShowNativeView();

  // Close the dialog widget.
  void CloseView();

  raw_ptr<views::WebView> web_view_ = nullptr;
  const raw_ptr<Browser> browser_;
  const absl::optional<gfx::Size> boundary_dimensions_for_test_;
  const absl::optional<double> zoom_factor_for_test_;
  base::WeakPtrFactory<SearchEngineChoiceDialogView> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_SEARCH_ENGINE_CHOICE_SEARCH_ENGINE_CHOICE_DIALOG_VIEW_H_
