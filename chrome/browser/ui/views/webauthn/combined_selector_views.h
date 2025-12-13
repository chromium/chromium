// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEBAUTHN_COMBINED_SELECTOR_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_WEBAUTHN_COMBINED_SELECTOR_VIEWS_H_

#include <string_view>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/webauthn/sheet_models.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/views/controls/button/radio_button.h"
#include "ui/views/layout/table_layout_view.h"
#include "ui/views/view.h"

// CombinedSelectorRadioButton is a wrapper around RadioButton so that different
// radio buttons within the same CombinedSelectorListView can be grouped
// together.
class CombinedSelectorRadioButton : public views::RadioButton {
  METADATA_HEADER(CombinedSelectorRadioButton, views::RadioButton)
 public:
  class Delegate {
   public:
    virtual void OnRadioButtonChecked(int index) = 0;
  };

  explicit CombinedSelectorRadioButton(Delegate* delegate, int index);

  View* GetSelectedViewForGroup(int group) override;
  void SetChecked(bool checked) override;

 private:
  void GetRadioButtonsInList(int group, Views* views);
  bool SkipDefaultKeyEventProcessing(const ui::KeyEvent& event) override;

  raw_ptr<Delegate> delegate_;
  const int index_;
};

class CombinedSelectorTextColumnView : public views::TableLayoutView {
  METADATA_HEADER(CombinedSelectorTextColumnView, views::TableLayoutView)
 public:
  explicit CombinedSelectorTextColumnView(
      const std::vector<std::u16string_view> texts);
};

// `CombinedSelectorRowView` renders the given icon with the text in the
// following format. `radio_status` determines if a radio button should be
// rendered at the end of the row.
//
// +-------------------------------------------------------------------+
// |      |    title                                            |      |
// | icon |                                                     |radio?|
// |      |     ... more text (row by row)                      |      |
// +-------------------------------------------------------------------+
class CombinedSelectorRowView : public views::TableLayoutView {
  METADATA_HEADER(CombinedSelectorRowView, views::TableLayoutView)
 public:
  using RadioStatus = CombinedSelectorSheetModel::SelectionStatus;

  CombinedSelectorRowView(
      const ui::ImageModel& icon,
      const std::vector<std::u16string_view> texts,
      RadioStatus radio_status,
      bool enabled = true,
      CombinedSelectorRadioButton::Delegate* radio_delegate = nullptr,
      int index = 0);

  bool is_selected() const { return radio_status_ == RadioStatus::kSelected; }

 private:
  void MaybeAddRadioButton(CombinedSelectorRadioButton::Delegate* delegate,
                           int index);

  // views::TableLayoutView:
  void RequestFocus() override;
  bool OnMousePressed(const ui::MouseEvent& event) override;
  void OnMouseReleased(const ui::MouseEvent& event) override;


  raw_ptr<views::View> radio_button_;
  RadioStatus radio_status_;
  bool enabled_ = true;
};

class CombinedSelectorListView : public views::View {
  METADATA_HEADER(CombinedSelectorListView, views::View)
 public:
  static constexpr int kMaxRowHeight = 72;
  static constexpr int kRowGap = 4;

  explicit CombinedSelectorListView(
      CombinedSelectorSheetModel* model,
      CombinedSelectorRadioButton::Delegate* delegate);
  ~CombinedSelectorListView() override;

 private:
  // views::View:
  void RequestFocus() override;

  raw_ptr<views::View> selected_view_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_WEBAUTHN_COMBINED_SELECTOR_VIEWS_H_
