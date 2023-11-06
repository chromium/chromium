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

// Manages toolbar elements' visibility using flex rules.
class ToolbarController : public ui::SimpleMenuModel::Delegate {
 public:
  // Data structure to store information of responsive elements.
  struct ResponsiveElementInfo {
    // Menu text when the element is overflow to the overflow menu.
    int menu_text_id;

    // The toolbar button to be activated with menu text pressed. This is not
    // necessarily the same as the element that overflows. E.g. when the
    // overflowed element is kToolbarExtensionsContainerElementId the
    // `activate_identifier` should be kExtensionsMenuButtonElementId.
    ui::ElementIdentifier activate_identifier;

    // Pop out button when `observed_identifier` is shown. End pop out when it's
    // hidden.
    absl::optional<ui::ElementIdentifier> observed_identifier;
  };

  // A map from identifier to its observed identifier. Used to create
  // PopOutHandlers for each element in the map.
  using ResponsiveElementInfoMap =
      base::flat_map<ui::ElementIdentifier,
                     ToolbarController::ResponsiveElementInfo>;

  ToolbarController(
      std::vector<ui::ElementIdentifier> element_ids,
      const ToolbarController::ResponsiveElementInfoMap& element_info_map,
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

  // Return the default element info map used by the browser.
  static ToolbarController::ResponsiveElementInfoMap GetDefaultElementInfoMap();

  // Return the action name from element identifier. Return empty if not found.
  static std::string GetActionNameFromElementIdentifier(
      ui::ElementIdentifier identifier);

  // Force the UI element with the identifier to show. Return whether the action
  // is successful.
  virtual bool PopOut(ui::ElementIdentifier identifier);

  // End forcing the UI element with the identifier to show. Return whether the
  // action is successful.
  virtual bool EndPopOut(ui::ElementIdentifier identifier);

  // Returns true if layout manager of `toolbar_container_view_` hides any
  // toolbar elements.
  bool ShouldShowOverflowButton();

  views::View* overflow_button() { return overflow_button_; }

  const base::flat_map<ui::ElementIdentifier, std::unique_ptr<PopOutState>>&
  pop_out_state_for_testing() const {
    return pop_out_state_;
  }

  // Create the overflow menu model for hidden buttons.
  std::unique_ptr<ui::SimpleMenuModel> CreateOverflowMenuModel();

  // Generate menu text from the responsive element.
  virtual std::u16string GetMenuText(ui::ElementIdentifier id);

  // Utility that recursively searches for a view with `id` from `view`.
  static views::View* FindToolbarElementWithId(views::View* view,
                                               ui::ElementIdentifier id);

 private:
  friend class ToolbarControllerInteractiveTest;
  friend class ToolbarControllerUnitTest;

  // Returns currently hidden elements.
  std::vector<ui::ElementIdentifier> GetOverflowedElements();

  // Check if element has overflowed.
  bool IsOverflowed(ui::ElementIdentifier id);

  // ui::SimpleMenuModel::Delegate:
  void ExecuteCommand(int command_id, int event_flags) override;
  bool IsCommandIdEnabled(int command_id) const override;

  // Returns the element in `element_ids_` at index `command_id`.
  ui::ElementIdentifier GetHiddenElementOfCommandId(int command_id) const;

  // The toolbar elements managed by this controller.
  // Order matters as each will be assigned with a flex order that increments by
  // 1 starting from `element_flex_order_start_`. So the last element drops out
  // first once overflow starts. This also serves as a map that its indices are
  // used as command ids in overflowed menu model.
  const std::vector<ui::ElementIdentifier> element_ids_;

  const ToolbarController::ResponsiveElementInfoMap element_info_map_;

  // The starting flex order assigned to the first element in `elements_ids_`.
  const int element_flex_order_start_;

  // Reference to ToolbarView::container_view_. Must outlive `this`.
  const raw_ptr<views::View> toolbar_container_view_;

  // The button with a chevron icon that indicates at least one element in
  // `element_ids_` overflows. Owned by `toolbar_container_view_`.
  raw_ptr<views::View> overflow_button_;

  // A map to save the original and modified FlexSpecification of responsive
  // elements that need to pop out. Set when ToolbarController is initialized.
  base::flat_map<ui::ElementIdentifier, std::unique_ptr<PopOutState>>
      pop_out_state_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_TOOLBAR_CONTROLLER_H_
