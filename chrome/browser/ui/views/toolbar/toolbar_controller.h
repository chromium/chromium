// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_TOOLBAR_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_TOOLBAR_CONTROLLER_H_

#include <vector>

#include "base/callback_list.h"
#include "base/gtest_prod_util.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/views/toolbar/overflow_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/view.h"

// A map from identifier to its observed identifier. Used to create
// PopOutHandlers for each element in the map.
using PopOutIdentifierMap =
    base::flat_map<ui::ElementIdentifier, ui::ElementIdentifier>;

// Manages toolbar elements' visibility using flex rules.
class ToolbarController : public ui::SimpleMenuModel::Delegate {
 public:
  ToolbarController(std::vector<ui::ElementIdentifier> element_ids,
                    PopOutIdentifierMap pop_out_identifier_map,
                    int element_flex_order_start,
                    views::View* toolbar_container_view,
                    views::View* overflow_button);
  ToolbarController(const ToolbarController&) = delete;
  ToolbarController& operator=(const ToolbarController&) = delete;
  ~ToolbarController() override;

  // Handler to pop out `identifier` when `observed_identier` is shown and end
  // the pop out when it's hidden. For example, a toolbar button needs to pop
  // out when a bubble is anchored to it.
  class PopOutHandler {
   public:
    PopOutHandler(ToolbarController* controller,
                  ui::ElementContext context,
                  ui::ElementIdentifier identifier,
                  ui::ElementIdentifier observed_identifier);
    PopOutHandler(const PopOutHandler&) = delete;
    PopOutHandler& operator=(const PopOutHandler&) = delete;
    virtual ~PopOutHandler();

   private:
    // Called when element with `observed_identifier` is shown.
    void OnElementShown(ui::TrackedElement* element);

    // Called when element with `observed_identifier` is hidden.
    void OnElementHidden(ui::TrackedElement* element);

    const raw_ptr<ToolbarController> controller_;
    const ui::ElementIdentifier identifier_;
    const ui::ElementIdentifier observed_identifier_;
    base::CallbackListSubscription shown_subscription_;
    base::CallbackListSubscription hidden_subscription_;
  };

  // Data structure to store the state of the responsive element. It's used for
  // pop out/end pop out.
  struct PopOutState {
    PopOutState();
    PopOutState(const PopOutState&) = delete;
    PopOutState& operator=(const PopOutState&) = delete;
    ~PopOutState();

    // The original FlexSpecification.
    absl::optional<views::FlexSpecification> original_spec;

    // The responsive FlexSpecification modified by ToolbarController.
    views::FlexSpecification responsive_spec;

    // Whether the element is current popped out.
    bool is_popped_out = false;

    std::unique_ptr<PopOutHandler> handler;
  };

  // Force the UI element with the identifier to show. Return whether the action
  // is successful.
  virtual bool PopOut(ui::ElementIdentifier identifier);

  // End forcing the UI element with the identifier to show. Return whether the
  // action is successful.
  virtual bool EndPopOut(ui::ElementIdentifier identifier);

  // Returns true if layout manager of `toolbar_container_view_` hides any
  // toolbar elements.
  bool ShouldShowOverflowButton();

  void SetOverflowButtonVisible(bool should_show) {
    overflow_button_->SetVisible(should_show);
  }

  const base::flat_map<ui::ElementIdentifier, std::unique_ptr<PopOutState>>&
  pop_out_state_for_testing() const {
    return pop_out_state_;
  }

  // Create the overflow menu model for hidden buttons.
  std::unique_ptr<ui::SimpleMenuModel> CreateOverflowMenuModel();

 private:
  friend class ToolbarControllerInteractiveTest;
  friend class ToolbarControllerUnitTest;

  // Searches for a toolbar element from `toolbar_container_view_` with `id`.
  views::View* FindToolbarElementWithId(ui::ElementIdentifier id) {
    return const_cast<views::View*>(
        std::as_const(*this).FindToolbarElementWithId(id));
  }
  const views::View* FindToolbarElementWithId(ui::ElementIdentifier id) const;

  // Returns currently hidden elements.
  std::vector<const views::View*> GetOverflowedElements();

  // ui::SimpleMenuModel::Delegate:
  void ExecuteCommand(int command_id, int event_flags) override;

  // The toolbar elements managed by this controller.
  // Order matters as each will be assigned with a flex order that increments by
  // 1 starting from `element_flex_order_start_`. So the last element drops out
  // first once overflow starts.
  const std::vector<ui::ElementIdentifier> element_ids_;

  // The starting flex order assigned to the first element in `elements_ids_`.
  const int element_flex_order_start_;

  // Reference to ToolbarView::container_view_. Must outlive `this`.
  const raw_ptr<const views::View> toolbar_container_view_;

  // The button with a chevron icon that indicates at least one element in
  // `element_ids_` overflows. Owned by `toolbar_container_view_`.
  raw_ptr<views::View> overflow_button_;

  // A map to save the original and modified FlexSpecification of responsive
  // elements that need to pop out. Set when ToolbarController is initialized.
  base::flat_map<ui::ElementIdentifier, std::unique_ptr<PopOutState>>
      pop_out_state_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_TOOLBAR_CONTROLLER_H_
