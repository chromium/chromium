// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_POPUP_ROW_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_POPUP_ROW_VIEW_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "components/autofill/core/browser/ui/popup_types.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/color/color_id.h"
#include "ui/events/event.h"
#include "ui/gfx/font.h"
#include "ui/views/controls/label.h"
#include "ui/views/view.h"

namespace views {
class BoxLayout;
}  // namespace views

namespace autofill {

class PopupViewViews;

// `PopupRowView` represents a single selectable item. Subclasses distinguish
// between footer and suggestion rows, which are structurally similar, but have
// distinct styling.
class PopupRowView : public views::View {
 public:
  METADATA_HEADER(PopupRowView);
  PopupRowView(const PopupRowView&) = delete;
  PopupRowView& operator=(const PopupRowView&) = delete;
  ~PopupRowView() override;

  void SetSelected(bool selected);

  // Show the in-product-help promo anchored to this bubble if applicable. The
  // in-product-help promo is a bubble anchored to this item to show educational
  // messages. The promo bubble should only be shown once in one session and has
  // a limit for how many times it can be shown at most in a period of time.
  void MaybeShowIphPromo();

  // `CreateContent` handles initialization tasks which require virtual methods.
  // Subclasses should have private/protected constructors and implement a
  // factory function that calls `CreateContent` after creating the object.
  virtual void CreateContent();

  // views::View:
  bool HandleAccessibleAction(const ui::AXActionData& action_data) override;
  // Drags and presses on any row should be a no-op; subclasses instead rely on
  // entry/release events. Returns true to indicate that those events have been
  // processed (i.e., intentionally ignored).
  bool OnMouseDragged(const ui::MouseEvent& event) override;
  bool OnMousePressed(const ui::MouseEvent& event) override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  void OnPaint(gfx::Canvas* canvas) override;
  void OnThemeChanged() override;
  void OnMouseEntered(const ui::MouseEvent& event) override;
  void OnMouseExited(const ui::MouseEvent& event) override;
  void OnMouseReleased(const ui::MouseEvent& event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;

 protected:
  PopupRowView(PopupViewViews& popup_view, int line_number, int frontend_id);

  PopupViewViews& popup_view() { return popup_view_.get(); }
  int GetLineNumber() const { return line_number_; }
  int GetFrontendId() const { return frontend_id_; }
  bool GetSelected() const { return selected_; }
  ui::ColorId GetBackgroundColorId() const;

  virtual void RefreshStyle();

  virtual int GetPrimaryTextStyle() = 0;
  // Returns a main text label view. The label part is optional but allow caller
  // to keep track of all the labels for background color update.
  virtual std::unique_ptr<views::Label> CreateMainTextView();
  // Returns a minor text label view. The label is shown side by side with the
  // main text view, but in a secondary style. Can be nullptr.
  virtual std::unique_ptr<views::Label> CreateMinorTextView();
  // The description view can be nullptr.
  virtual std::unique_ptr<views::View> CreateDescriptionView();

  // Returns the font weight to be applied to primary info.
  virtual gfx::Font::Weight GetPrimaryTextWeight() const = 0;

  void AddSpacerWithSize(int spacer_width,
                         bool resize,
                         views::BoxLayout* layout);

  void KeepLabel(views::Label* label) {
    if (label) {
      inner_labels_.push_back(label);
    }
  }

  // Returns the string to be set as the name of the ui::AXNodeData.
  std::u16string GetVoiceOverString();

 private:
  // Returns a vector of optional labels to be displayed beneath value.
  virtual std::vector<std::unique_ptr<views::View>> CreateSubtextViews();

  // Returns the minimum cross axis size depending on the length of
  // GetSubtexts();
  void UpdateLayoutSize(views::BoxLayout* layout_manager, int64_t num_subtexts);

  // Returns true if the mouse is within the bounds of this item. This is not
  // affected by whether or not the item is overlaid by another popup.
  bool IsMouseInsideItemBounds() const { return IsMouseHovered(); }

  // We want a mouse click to accept a suggestion only if the user has made an
  // explicit choice. Therefore, we shall ignore mouse clicks unless the mouse
  // has been moved into the item's screen bounds. For example, if the item is
  // hovered by the mouse at the time it's first shown, we want to ignore clicks
  // until the mouse has left and re-entered the bounds of the item
  // (crbug.com/1240472, crbug.com/1241585, crbug.com/1287364).
  bool mouse_observed_outside_item_bounds_ = false;

  // The parent view containing this row.
  const raw_ref<PopupViewViews> popup_view_;
  const int line_number_;
  const int frontend_id_;

  // All the labels inside this view.
  std::vector<views::Label*> inner_labels_;

  // Whether this row is currently selected.
  bool selected_ = false;
};

// This represents a suggestion, i.e., a row containing data that will be filled
// into the page if selected.
class PopupSuggestionView : public PopupRowView {
 public:
  METADATA_HEADER(PopupSuggestionView);
  PopupSuggestionView(const PopupSuggestionView&) = delete;
  PopupSuggestionView& operator=(const PopupSuggestionView&) = delete;
  ~PopupSuggestionView() override = default;

  static std::unique_ptr<PopupSuggestionView> Create(PopupViewViews& popup_view,
                                                     int line_number,
                                                     int frontend_id,
                                                     PopupType popup_type);

 protected:
  // PopupItemView:
  int GetPrimaryTextStyle() override;
  gfx::Font::Weight GetPrimaryTextWeight() const override;
  std::unique_ptr<views::Label> CreateMainTextView() override;
  std::vector<std::unique_ptr<views::View>> CreateSubtextViews() override;
  PopupSuggestionView(PopupViewViews& popup_view,
                      int line_number,
                      int frontend_id,
                      PopupType popup_type);

 private:
  // The popup type to which this suggestion belongs.
  PopupType popup_type_;
};

// This represents a password suggestion row, i.e., a username and password.
class PopupPasswordSuggestionView : public PopupSuggestionView {
 public:
  METADATA_HEADER(PopupPasswordSuggestionView);
  PopupPasswordSuggestionView(const PopupPasswordSuggestionView&) = delete;
  PopupPasswordSuggestionView& operator=(const PopupPasswordSuggestionView&) =
      delete;
  ~PopupPasswordSuggestionView() override = default;

  static std::unique_ptr<PopupPasswordSuggestionView>
  Create(PopupViewViews& popup_view, int line_number, int frontend_id);

 protected:
  // PopupItemView:
  std::unique_ptr<views::Label> CreateMainTextView() override;
  std::vector<std::unique_ptr<views::View>> CreateSubtextViews() override;
  std::unique_ptr<views::View> CreateDescriptionView() override;
  gfx::Font::Weight GetPrimaryTextWeight() const override;

 private:
  PopupPasswordSuggestionView(PopupViewViews& popup_view,
                              int line_number,
                              int frontend_id);
  std::u16string origin_;
  std::u16string masked_password_;
};

// This represents an option which appears in the footer of the dropdown, such
// as a row which will open the Autofill settings page when selected.
class PopupFooterView : public PopupRowView {
 public:
  METADATA_HEADER(PopupFooterView);
  ~PopupFooterView() override = default;

  static std::unique_ptr<PopupFooterView> Create(PopupViewViews& popup_view,
                                                 int line_number,
                                                 int frontend_id);

 protected:
  // PopupItemView:
  void CreateContent() override;
  int GetPrimaryTextStyle() override;
  gfx::Font::Weight GetPrimaryTextWeight() const override;

 private:
  PopupFooterView(PopupViewViews& popup_view, int line_number, int frontend_id);
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_POPUP_ROW_VIEW_H_
