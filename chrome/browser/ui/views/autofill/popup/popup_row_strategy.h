// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_POPUP_ROW_STRATEGY_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_POPUP_ROW_STRATEGY_H_

#include <memory>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/views/autofill/popup/popup_cell_view.h"
#include "components/autofill/core/browser/ui/popup_types.h"
#include "components/autofill/core/browser/ui/suggestion.h"

namespace views {
class View;
}

namespace autofill {

class AutofillPopupController;

// `PopupRowStrategy` is an interface for content providers of `PopupRowView`.
// `PopupRowView` is the actual `views::View` that owns the content and control
// surfaces. Different types of popup rows can be created by passing different
// implementations of `PopupRowStrategy` to the constructor of `PopupRowView`,
// e.g. for password rows, Autofill rows, footer rows, etc.
class PopupRowStrategy {
 public:
  PopupRowStrategy() = default;
  PopupRowStrategy(const PopupRowStrategy&) = delete;
  PopupRowStrategy& operator=(const PopupRowStrategy&) = delete;
  virtual ~PopupRowStrategy() = default;

  // Creates the `PopupCellView` that contains the content area of the popup
  // row.
  virtual std::unique_ptr<PopupCellView> CreateContent() = 0;
  // Creates the `PopupCellView` that contains the control area of the popup
  // row. It will be `nullptr` by default for most types of popup suggestions.
  virtual std::unique_ptr<PopupCellView> CreateControl() = 0;

  // Returns the line number of the popup row that this strategy is for.
  virtual int GetLineNumber() const = 0;
};

// A base class of methods shared between most strategies. It should never be
// instantiated directly.
class PopupRowBaseStrategy : public PopupRowStrategy {
 public:
  PopupRowBaseStrategy(const PopupRowBaseStrategy&) = delete;
  PopupRowBaseStrategy& operator=(const PopupRowBaseStrategy&) = delete;

  int GetLineNumber() const override;
  base::WeakPtr<AutofillPopupController> GetController() { return controller_; }
  base::WeakPtr<const AutofillPopupController> GetController() const {
    return controller_;
  }

 protected:
  PopupRowBaseStrategy(base::WeakPtr<AutofillPopupController> controller,
                       int line_number);
  ~PopupRowBaseStrategy() override;

 private:
  // The controller of the popup.
  const base::WeakPtr<AutofillPopupController> controller_;
  // The line number of the popup.
  const int line_number_;
};

// A `PopupRowStrategy` that creates the content for autocomplete, address, and
// credit card popups.
class PopupSuggestionStrategy : public PopupRowBaseStrategy {
 public:
  PopupSuggestionStrategy(base::WeakPtr<AutofillPopupController> controller,
                          int line_number);
  PopupSuggestionStrategy(const PopupSuggestionStrategy&) = delete;
  PopupSuggestionStrategy& operator=(const PopupSuggestionStrategy&) = delete;
  ~PopupSuggestionStrategy() override;

  // PopupRowStrategy:
  std::unique_ptr<PopupCellView> CreateContent() override;
  std::unique_ptr<PopupCellView> CreateControl() override;
  std::unique_ptr<PopupCellView> CreateAutocompleteRow();

 private:
  // Returns the type of the popup that this row belongs to.
  PopupType GetPopupType() const { return popup_type_; }

  // Applies additional formatting to the `label` based on the popup's type and
  // the parameters in `text`.
  void FormatLabel(views::Label& label, const Suggestion::Text& text) const;

  // Creates the subtext views for this suggestion. Since it registers the
  // labels inside them for tracking with `content_view`, it assumes that the
  // returned views are added to `content_view` afterwards.
  std::vector<std::unique_ptr<views::View>> CreateAndTrackSubtextViews(
      PopupCellView& content_view) const;

  const PopupType popup_type_;
};

// A `PopupRowStrategy` that creates the content for password suggestion rows.
class PopupPasswordSuggestionStrategy : public PopupRowBaseStrategy {
 public:
  PopupPasswordSuggestionStrategy(
      base::WeakPtr<AutofillPopupController> controller,
      int line_number);
  PopupPasswordSuggestionStrategy(const PopupPasswordSuggestionStrategy&) =
      delete;
  PopupPasswordSuggestionStrategy& operator=(
      const PopupPasswordSuggestionStrategy&) = delete;
  ~PopupPasswordSuggestionStrategy() override;

  // PopupRowStrategy:
  std::unique_ptr<PopupCellView> CreateContent() override;
  std::unique_ptr<PopupCellView> CreateControl() override;

 private:
  // Creates the description label for this suggestion.
  std::unique_ptr<views::Label> CreateDescriptionLabel() const;

  // Creates the subtext views for this suggestion. Since it registers the
  // labels inside them for tracking with `content_view`, it assumes that the
  // returned views are added to `content_view` afterwards.
  std::vector<std::unique_ptr<views::View>> CreateAndTrackSubtextViews(
      PopupCellView& content_view) const;
};

// A `PopupRowStrategy` that creates the content for Autofill popup footers.
class PopupFooterStrategy : public PopupRowBaseStrategy {
 public:
  PopupFooterStrategy(base::WeakPtr<AutofillPopupController> controller,
                      int line_number);
  PopupFooterStrategy(const PopupFooterStrategy&) = delete;
  PopupFooterStrategy& operator=(const PopupFooterStrategy&) = delete;
  ~PopupFooterStrategy() override;

  // PopupRowStrategy:
  std::unique_ptr<PopupCellView> CreateContent() override;
  std::unique_ptr<PopupCellView> CreateControl() override;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_POPUP_ROW_STRATEGY_H_
