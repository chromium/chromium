// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_INPUT_METHOD_SUGGESTION_WINDOW_VIEW_H_
#define CHROME_BROWSER_UI_ASH_INPUT_METHOD_SUGGESTION_WINDOW_VIEW_H_

#include <stddef.h>

#include <memory>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/ash/input_method/announcement_label.h"
#include "chrome/browser/ui/ash/input_method/indexed_suggestion_candidate_button.h"
#include "chromeos/ash/services/ime/public/cpp/assistive_suggestions.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/chromeos/ui_chromeos_export.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/view.h"

namespace ash {
namespace input_method {
struct AssistiveWindowProperties;
}  // namespace input_method
}  // namespace ash

namespace views {
class ImageButton;
class Link;
}  // namespace views

namespace ui {
namespace ime {

class AssistiveDelegate;
struct AssistiveWindowButton;
struct SuggestionDetails;
class CompletionSuggestionView;

// SuggestionWindowView is the main container of the suggestion window UI.
class UI_CHROMEOS_EXPORT SuggestionWindowView
    : public views::BubbleDialogDelegateView {
  METADATA_HEADER(SuggestionWindowView, views::BubbleDialogDelegateView)

 public:
  enum Orientation {
    kHorizontal =
        0,  // TODO(b/215292569): Orientation needs to follow UI specs.
            // Currently only rotates the candidates horizontally.
    kVertical,
  };

  // Creates a bubble widget containing a SuggestionWindowView.  Returns a
  // pointer to the contained view.
  static SuggestionWindowView* Create(gfx::NativeView parent,
                                      AssistiveDelegate* delegate,
                                      Orientation orientation);

  // views::BubbleDialogDelegateView:
  std::unique_ptr<views::NonClientFrameView> CreateNonClientFrameView(
      views::Widget* widget) override;

  void Show(const SuggestionDetails& details);

  void ShowMultipleCandidates(
      const ash::input_method::AssistiveWindowProperties& properties,
      Orientation orientation);

  // Sets |button|'s highlight state to |highlighted|. At most one button with
  // the same id will be highlighted at any given time.
  void SetButtonHighlighted(const AssistiveWindowButton& button,
                            bool highlighted);

  views::View* multiple_candidate_area_for_testing() {
    return multiple_candidate_area_;
  }
  views::Link* setting_link_for_testing() { return setting_link_; }
  views::ImageButton* learn_more_button_for_testing() {
    return learn_more_button_;
  }

 protected:
  // views::BubbleDialogDelegateView:
  gfx::Rect GetBubbleBounds() override;
  void OnThemeChanged() override;
  void LearnMoreClicked();
  raw_ptr<views::ImageButton> getLearnMoreButton();

 private:
  SuggestionWindowView(gfx::NativeView parent,
                       AssistiveDelegate* delegate,
                       Orientation orientation);
  SuggestionWindowView(const SuggestionWindowView&) = delete;
  SuggestionWindowView& operator=(const SuggestionWindowView&) = delete;
  ~SuggestionWindowView() override;

  // Sets the number of candidates (i.e. the number of children of
  // |candidate_area_|) to |size|.
  void ResizeCandidateArea(const std::vector<std::u16string>& new_candidates,
                           bool use_legacy_candidate = false);

  void Reorient(Orientation orientation, bool extra_padding_on_right = true);

  void MakeVisible();

  // Sets |candidate|'s highlight state to |highlighted|. At most one candidate
  // will be highlighted at any given time.
  void SetCandidateHighlighted(IndexedSuggestionCandidateButton* candidate,
                               bool highlighted);

  // The delegate to handle events from this class.
  const raw_ptr<AssistiveDelegate, DanglingUntriaged> delegate_;

  // The view containing all the suggestions if multiple candidates are
  // visible.
  raw_ptr<views::View> multiple_candidate_area_;

  // The view containing the completion view. If this is visible then there is
  // only one suggestion to show.
  raw_ptr<CompletionSuggestionView> completion_view_;

  // The setting link, positioned below candidate_area_.
  // TODO(crbug.com/40138671): Rename setting to settings since there can be
  // multiple things to set.
  raw_ptr<views::Link> setting_link_;

  raw_ptr<views::ImageButton> learn_more_button_;

  // TODO(crbug.com/40137305): Add tests for mouse hovered and pressed.
  base::flat_map<views::View*, base::CallbackListSubscription> subscriptions_;

  std::unique_ptr<base::OneShotTimer> delay_timer_;
  ash::ime::AssistiveWindowType type_ = ash::ime::AssistiveWindowType::kNone;
};

}  // namespace ime
}  // namespace ui

#endif  // CHROME_BROWSER_UI_ASH_INPUT_METHOD_SUGGESTION_WINDOW_VIEW_H_
