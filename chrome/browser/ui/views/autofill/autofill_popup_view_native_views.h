// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_AUTOFILL_POPUP_VIEW_NATIVE_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_AUTOFILL_POPUP_VIEW_NATIVE_VIEWS_H_

#include <memory>
#include <vector>

#include "base/optional.h"
#include "chrome/browser/ui/autofill/autofill_popup_view.h"
#include "chrome/browser/ui/views/autofill/autofill_popup_base_view.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/font_list.h"
#include "ui/views/metadata/metadata_header_macros.h"

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
  AutofillPopupViewNativeViews* popup_view_;
  const int line_number_;
  bool selected_ = false;
};

// Views implementation for the autofill and password suggestion.
// TODO(https://crbug.com/831603): Rename to AutofillPopupViewViews.
class AutofillPopupViewNativeViews : public AutofillPopupBaseView,
                                     public AutofillPopupView {
 public:
  METADATA_HEADER(AutofillPopupViewNativeViews);
  AutofillPopupViewNativeViews(AutofillPopupController* controller,
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
  base::Optional<int32_t> GetAxUniqueId() override;

  // AutofillPopupBaseView:
  // TODO(crbug.com/831603): Remove these overrides and the corresponding
  // methods in AutofillPopupBaseView.
  void OnMouseMoved(const ui::MouseEvent& event) override {}

  AutofillPopupController* controller() { return controller_; }

 private:
  void OnSelectedRowChanged(base::Optional<int> previous_row_selection,
                            base::Optional<int> current_row_selection) override;
  void OnSuggestionsChanged() override;

  // Creates child views based on the suggestions given by |controller_|.
  void CreateChildViews();

  // Applies certain rounding rules to the given width, such as matching the
  // element width when possible.
  int AdjustWidth(int width) const;

  // AutofillPopupBaseView:
  bool DoUpdateBoundsAndRedrawPopup() override;

  // Controller for this view.
  AutofillPopupController* controller_ = nullptr;
  std::vector<AutofillPopupRowView*> rows_;
  views::BoxLayout* layout_ = nullptr;
  views::ScrollView* scroll_view_ = nullptr;
  views::View* body_container_ = nullptr;
  views::View* footer_container_ = nullptr;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_AUTOFILL_POPUP_VIEW_NATIVE_VIEWS_H_
