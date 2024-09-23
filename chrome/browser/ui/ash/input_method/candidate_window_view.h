// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_INPUT_METHOD_CANDIDATE_WINDOW_VIEW_H_
#define CHROME_BROWSER_UI_ASH_INPUT_METHOD_CANDIDATE_WINDOW_VIEW_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "ui/base/ime/candidate_window.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/chromeos/ui_chromeos_export.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/metadata/view_factory.h"

namespace ash::input_method {
struct TextFieldContextualInfo;
}  // namespace ash::input_method

namespace ui {
namespace ime {

class CandidateView;
class InformationTextArea;

// CandidateWindowView is the main container of the candidate window UI.
class UI_CHROMEOS_EXPORT CandidateWindowView
    : public views::BubbleDialogDelegateView {
  METADATA_HEADER(CandidateWindowView, views::BubbleDialogDelegateView)

 public:
  // The object can be monitored by the observer.
  class Observer {
   public:
    virtual ~Observer() {}
    // The function is called when a candidate is committed.
    virtual void OnCandidateCommitted(int index) = 0;
  };

  explicit CandidateWindowView(gfx::NativeView parent);
  CandidateWindowView(const CandidateWindowView&) = delete;
  CandidateWindowView& operator=(const CandidateWindowView&) = delete;
  ~CandidateWindowView() override;
  views::Widget* InitWidget();

  // views::BubbleDialogDelegateView:
  void OnThemeChanged() override;

  // Adds the given observer. The ownership is not transferred.
  void AddObserver(Observer* observer) { observers_.AddObserver(observer); }

  // Removes the given observer.
  void RemoveObserver(Observer* observer) {
    observers_.RemoveObserver(observer);
  }

  // Hides the lookup table.
  void HideLookupTable();

  // Hides the auxiliary text.
  void HideAuxiliaryText();

  // Hides the preedit text.
  void HidePreeditText();

  // Shows the lookup table.
  void ShowLookupTable();

  // Shows the auxiliary text.
  void ShowAuxiliaryText();

  // Shows the preedit text.
  void ShowPreeditText();

  // Updates the preedit text.
  void UpdatePreeditText(const std::u16string& text);

  // Updates candidates of the candidate window from |candidate_window|.
  // Candidates are arranged per |orientation|.
  void UpdateCandidates(const ui::CandidateWindow& candidate_window);

  void SetCursorAndCompositionBounds(const gfx::Rect& cursor_bounds,
                                     const gfx::Rect& composition_bounds);

 private:
  friend class CandidateWindowViewTest;

  void SelectCandidateAt(int index_in_page);
  void UpdateVisibility();

  // Initializes the candidate views if needed.
  void MaybeInitializeCandidateViews(
      const ui::CandidateWindow& candidate_window);

  void CandidateViewPressed(int index);

  // Only used when ash::features::kImeKoreanModeSwitchDebug flag is enabled.
  // TODO(b/302460634): Remove when no longer needed.
  void OnTextFieldContextualInfoAvailable(
      const ash::input_method::TextFieldContextualInfo& info);

  // The candidate window data model.
  ui::CandidateWindow candidate_window_;

  // The index in the current page of the candidate currently being selected.
  int selected_candidate_index_in_page_;

  // The observers of the object.
  base::ObserverList<Observer>::Unchecked observers_;

  // Views created in the class will be part of tree of |this|, so these
  // child views will be deleted when |this| is deleted.
  raw_ptr<InformationTextArea> auxiliary_text_;
  raw_ptr<InformationTextArea> preedit_;
  raw_ptr<views::View> candidate_area_;

  // The candidate views are used for rendering candidates.
  std::vector<raw_ptr<CandidateView, VectorExperimental>> candidate_views_;

  // Current columns size in |candidate_area_|.
  gfx::Size previous_shortcut_column_size_;
  gfx::Size previous_candidate_column_size_;
  gfx::Size previous_annotation_column_size_;

  // Only used when ash::features::kImeKoreanModeSwitchDebug flag is enabled.
  // TODO(b/302460634): Remove when no longer needed.
  gfx::Rect pending_anchor_rect_;
};

BEGIN_VIEW_BUILDER(UI_CHROMEOS_EXPORT,
                   CandidateWindowView,
                   views::BubbleDialogDelegateView)
END_VIEW_BUILDER

}  // namespace ime
}  // namespace ui

DEFINE_VIEW_BUILDER(UI_CHROMEOS_EXPORT, ui::ime::CandidateWindowView)

#endif  // CHROME_BROWSER_UI_ASH_INPUT_METHOD_CANDIDATE_WINDOW_VIEW_H_
