// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_AUTOFILL_POPUP_VIEW_NATIVE_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_AUTOFILL_POPUP_VIEW_NATIVE_VIEWS_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/autofill/autofill_popup_view.h"
#include "chrome/browser/ui/views/autofill/autofill_popup_base_view.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/font_list.h"
#include "ui/views/bubble/bubble_border.h"

namespace views {
class BoxLayout;
}

namespace autofill {

class AutofillPopupController;
class AutofillPopupViewNativeViews;

// Child view representing one row in the Autofill Popup. This could represent
// a UI control (e.g., a suggestion which can be autofilled), or decoration like
// separators.
class AutofillPopupRowView : public views::View {
 public:
  METADATA_HEADER(AutofillPopupRowView);
  AutofillPopupRowView(const AutofillPopupRowView&) = delete;
  AutofillPopupRowView& operator=(const AutofillPopupRowView&) = delete;
  ~AutofillPopupRowView() override = default;
  void SetSelected(bool selected);

  // Show the in-product-help promo anchored to this bubble if applicable. The
  // in-product-help promo is a bubble anchored to this item to show educational
  // messages. The promo bubble should only be shown once in one session and has
  // a limit for how many times it can be shown at most in a period of time.
  void MaybeShowIphPromo();

  // views::View:
  bool HandleAccessibleAction(const ui::AXActionData& action_data) override;
  void OnThemeChanged() override;
  // Drags and presses on any row should be a no-op; subclasses instead rely on
  // entry/release events. Returns true to indicate that those events have been
  // processed (i.e., intentionally ignored).
  bool OnMouseDragged(const ui::MouseEvent& event) override;
  bool OnMousePressed(const ui::MouseEvent& event) override;

 protected:
  AutofillPopupRowView(AutofillPopupViewNativeViews* popup_view,
                       int line_number);

  // Init handles initialization tasks which require virtual methods. Subclasses
  // should have private/protected constructors and implement a static Create
  // method which calls Init before returning.
  void Init();

  AutofillPopupViewNativeViews* popup_view() { return popup_view_; }
  int GetLineNumber() const;
  bool GetSelected() const;

  virtual void CreateContent() = 0;
  virtual void RefreshStyle() = 0;
  virtual std::unique_ptr<views::Background> CreateBackground() = 0;

 private:
  raw_ptr<AutofillPopupViewNativeViews> popup_view_;
  const int line_number_;
  bool selected_ = false;
};

// Views implementation for the autofill and password suggestion.
// TODO(https://crbug.com/831603): Rename to AutofillPopupViewViews.
class AutofillPopupViewNativeViews : public AutofillPopupBaseView,
                                     public AutofillPopupView {
 public:
  METADATA_HEADER(AutofillPopupViewNativeViews);
  AutofillPopupViewNativeViews(
      base::WeakPtr<AutofillPopupController> controller,
      views::Widget* parent_widget);
  AutofillPopupViewNativeViews(const AutofillPopupViewNativeViews&) = delete;
  AutofillPopupViewNativeViews& operator=(const AutofillPopupViewNativeViews&) =
      delete;
  ~AutofillPopupViewNativeViews() override;

  const std::vector<AutofillPopupRowView*>& GetRowsForTesting() {
    return rows_;
  }

  // views::View:
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  void OnThemeChanged() override;

  // AutofillPopupView:
  void Show() override;
  void Hide() override;
  absl::optional<int32_t> GetAxUniqueId() override;
  void AxAnnounce(const std::u16string& text) override;

  // AutofillPopupBaseView:
  // TODO(crbug.com/831603): Remove these overrides and the corresponding
  // methods in AutofillPopupBaseView.
  void OnMouseMoved(const ui::MouseEvent& event) override {}
  void OnWidgetVisibilityChanged(views::Widget* widget, bool visible) override;

  base::WeakPtr<AutofillPopupController> controller() { return controller_; }

 private:
  void OnSelectedRowChanged(absl::optional<int> previous_row_selection,
                            absl::optional<int> current_row_selection) override;
  void OnSuggestionsChanged() override;

  // Creates child views based on the suggestions given by |controller_|.
  // This method expects that all non-footer suggestions precede footer
  // suggestions.
  void CreateChildViews();

  // Applies certain rounding rules to the given width, such as matching the
  // element width when possible.
  int AdjustWidth(int width) const;

  // AutofillPopupBaseView:
  bool DoUpdateBoundsAndRedrawPopup() override;

  // Controller for this view.
  base::WeakPtr<AutofillPopupController> controller_ = nullptr;
  std::vector<AutofillPopupRowView*> rows_;
  raw_ptr<views::BoxLayout, DanglingUntriaged> layout_ = nullptr;
  raw_ptr<views::ScrollView, DanglingUntriaged> scroll_view_ = nullptr;
  raw_ptr<views::View, DanglingUntriaged> body_container_ = nullptr;
  raw_ptr<views::View, DanglingUntriaged> footer_container_ = nullptr;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_AUTOFILL_POPUP_VIEW_NATIVE_VIEWS_H_
