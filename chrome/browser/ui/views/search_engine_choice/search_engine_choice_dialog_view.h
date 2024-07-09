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
  METADATA_HEADER(SearchEngineChoiceDialogView, views::View)

 public:
  SearchEngineChoiceDialogView(
      Browser* browser,
      std::optional<gfx::Size> boundary_dimensions_for_test,
      std::optional<double> zoom_factor_for_test);
  ~SearchEngineChoiceDialogView() override;

  // Initialize SearchEngineChoiceDialogView's web_view_ element.
  void Initialize();

  // Returns a closure that can be executed to close the view (see
  // `SearchEngineChoiceDialogView::CloseView()`). Uses a weak pointer
  // internally, so it can be called after the view has been destroyed.
  base::OnceClosure GetCloseViewClosure();

 private:
  // Show the dialog widget.
  void ShowNativeView();

  // Close the dialog widget.
  void CloseView();

  raw_ptr<views::WebView> web_view_ = nullptr;
  const raw_ptr<Browser> browser_;
  const std::optional<gfx::Size> boundary_dimensions_for_test_;
  const std::optional<double> zoom_factor_for_test_;
  base::WeakPtrFactory<SearchEngineChoiceDialogView> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_SEARCH_ENGINE_CHOICE_SEARCH_ENGINE_CHOICE_DIALOG_VIEW_H_
