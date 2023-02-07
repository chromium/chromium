// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_POPUP_VIEW_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_POPUP_VIEW_VIEWS_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/autofill/autofill_popup_view.h"
#include "chrome/browser/ui/views/autofill/popup/popup_base_view.h"
#include "chrome/browser/ui/views/autofill/popup/popup_row_view.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/events/event.h"

namespace views {
class BoxLayoutView;
class ScrollView;
}  // namespace views

namespace autofill {

class AutofillPopupController;
class PopupSeparatorView;
class PopupWarningView;

// Views implementation for the autofill and password suggestion.
class PopupViewViews : public PopupBaseView, public AutofillPopupView {
 public:
  METADATA_HEADER(PopupViewViews);

  using RowPointer =
      absl::variant<PopupRowView*, PopupSeparatorView*, PopupWarningView*>;

  PopupViewViews(base::WeakPtr<AutofillPopupController> controller,
                 views::Widget* parent_widget);
  PopupViewViews(const PopupViewViews&) = delete;
  PopupViewViews& operator=(const PopupViewViews&) = delete;
  ~PopupViewViews() override;

  // views::View:
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;

  // AutofillPopupView:
  void Show() override;
  void Hide() override;
  absl::optional<int32_t> GetAxUniqueId() override;
  void AxAnnounce(const std::u16string& text) override;

  // PopupBaseView:
  // TODO(crbug.com/831603): Remove these overrides and the corresponding
  // methods in PopupBaseView.
  void OnMouseMoved(const ui::MouseEvent& event) override {}
  void OnWidgetVisibilityChanged(views::Widget* widget, bool visible) override;

  base::WeakPtr<AutofillPopupController> controller() { return controller_; }

 private:
  friend class PopupViewViewsBrowsertest;
  friend class PopupViewViewsTest;

  const std::vector<RowPointer>& GetRowsForTesting() { return rows_; }

  // Returns the `PopupRowView` at line number `index`. Assumes that there is
  // such a view at that line number - otherwise the underlying variant will
  // check false.
  PopupRowView& GetPopupRowViewAt(size_t index) {
    return *absl::get<PopupRowView*>(rows_[index]);
  }

  // Creates child views based on the suggestions given by |controller_|.
  // This method expects that all non-footer suggestions precede footer
  // suggestions.
  void CreateChildViews();

  // Applies certain rounding rules to the given width, such as matching the
  // element width when possible.
  int AdjustWidth(int width) const;

  // AutofillPopupView:
  void OnSelectedRowChanged(absl::optional<int> previous_row_selection,
                            absl::optional<int> current_row_selection) override;
  void OnSuggestionsChanged() override;

  // PopupBaseView:
  bool DoUpdateBoundsAndRedrawPopup() override;

  // Controller for this view.
  base::WeakPtr<AutofillPopupController> controller_ = nullptr;
  std::vector<RowPointer> rows_;
  raw_ptr<views::ScrollView> scroll_view_ = nullptr;
  raw_ptr<views::BoxLayoutView> body_container_ = nullptr;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_POPUP_VIEW_VIEWS_H_
