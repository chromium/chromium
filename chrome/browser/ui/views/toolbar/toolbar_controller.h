// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_TOOLBAR_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_TOOLBAR_CONTROLLER_H_

#include <memory>
#include <variant>
#include <vector>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/toolbar/pinned_toolbar/pinned_toolbar_actions_model.h"
#include "chrome/browser/ui/views/toolbar/overflow_button.h"
#include "chrome/browser/ui/views/toolbar/overflow_menu.h"
#include "chrome/browser/ui/views/toolbar/toolbar_button.h"
#include "ui/actions/action_id.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/layout/proposed_layout.h"
#include "ui/views/view.h"

// Manages toolbar elements' visibility using flex rules. This also owns the
// overflow menu and the logic to generate the menu model.
class ToolbarController : public OverflowMenu::Delegate {
 public:
  using ElementIdInfo = OverflowMenu::ElementIdInfo;
  using ResponsiveElementInfo = OverflowMenu::ResponsiveElementInfo;
  using OverflowableElement = OverflowMenu::OverflowableElement;

  // Manages action-based pinned toolbar elements.
  class PinnedActionsDelegate : public OverflowMenu::PinnedActionsInfo {
   public:
    // Returns true if the corresponding element is hidden.
    virtual bool IsOverflowed(actions::ActionId id) = 0;

    virtual views::View* GetContainerView() = 0;

    // Return true if any buttons should overflow given available size.
    virtual bool ShouldAnyButtonsOverflow(gfx::Size available_size) const = 0;

   protected:
    ~PinnedActionsDelegate() override = default;
  };

  // A delegate to handle overflow detection when using the WebUI toolbar, which
  // uses a single view to show multiple buttons, so needs its own logic.
  class WebUIToolbarControllerDelegate {
   public:
    WebUIToolbarControllerDelegate(WebUIToolbarControllerDelegate&&) = delete;

    // Returns true if the specified button would be hidden due to overflow,
    // given a ProposedLayout. If `proposed_layout` is null, considers the
    // current layout instead. May be called even if `identifier` is not being
    // handled by the WebUIToolbarControllerDelegate.
    virtual bool IsOverflowed(
        ui::ElementIdentifier identifier,
        const views::ProposedLayout* proposed_layout) const = 0;

    // Returns true if the specified button is enabled. Will not be called if
    // `identifier` is not being handled by the WebUIToolbarControllerDelegate.
    virtual bool IsEnabled(ui::ElementIdentifier identifier) const = 0;

    // Simulates a click on the specified element. Always simulates a left
    // click, without modifiers. Will not be called if `identifier` is
    // not being handled by the WebUIToolbarControllerDelegate.
    virtual void OverflowButtonClicked(ui::ElementIdentifier identifier) = 0;

   protected:
    WebUIToolbarControllerDelegate() = default;
    ~WebUIToolbarControllerDelegate() = default;
  };

  // `webui_toolbar_controller_delegate` may be nullptr if the WebUI toolbar is
  // not in use.
  ToolbarController(
      const std::vector<ResponsiveElementInfo>& responsive_elements,
      const std::vector<ui::ElementIdentifier>& elements_in_overflow_order,
      int element_flex_order_start,
      views::View* toolbar_container_view,
      WebUIToolbarControllerDelegate* webui_toolbar_controller_delegate,
      OverflowButton* overflow_button,
      PinnedActionsDelegate* pinned_actions_delegate,
      PinnedToolbarActionsModel* pinned_toolbar_actions_model);
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
    std::optional<views::FlexSpecification> original_spec;

    // The responsive FlexSpecification modified by ToolbarController.
    views::FlexSpecification responsive_spec;

    // Whether the element is current popped out.
    bool is_popped_out = false;

    std::unique_ptr<PopOutHandler> handler;
  };

  // OverflowMenu::Delegate:
  void ExecuteCommand(const OverflowableElement& element) override;
  bool IsCurrentlyOverflowed(const OverflowableElement& element) const override;
  bool IsEnabled(const OverflowableElement& element) const override;

  // Return the element list in desired overflow order. The list should contain
  // only the immediate children of toolbar i.e. those managed by
  // `toolbar_container_view_` layout manager. For those inside a child
  // container (e.g. PinnedToolbarActionsContainer) of `toolbar_container_view_`
  // they should have their own overflow order.
  static std::vector<ui::ElementIdentifier> GetDefaultOverflowOrder();

  // Return the action name from element identifier. Return empty if not found.
  static std::string GetActionNameFromElementIdentifier(
      std::variant<ui::ElementIdentifier, actions::ActionId> identifier);

  // Force the UI element with the identifier to show. Return whether the action
  // is successful.
  virtual bool PopOut(ui::ElementIdentifier identifier,
                      bool show_synchronously);

  // End forcing the UI element with the identifier to show. Return whether the
  // action is successful.
  virtual bool EndPopOut(ui::ElementIdentifier identifier);

  // Returns true if any overflow-able elements are hidden when
  // `toolbar_container_view_` is set to `size`. This excludes the overflow
  // button itself from the calculation, providing a much more accurate idea of
  // whether overflow would happen. Because of this, however, it must fully
  // recalculate the layout which could be expensive; call this method as little
  // as possible.
  bool ShouldShowOverflowButton(gfx::Size size);

  // Return true if any buttons overflow.
  bool InOverflowMode() const;

  OverflowButton* overflow_button() { return overflow_button_; }

  // Returns the FlexLayout order that should be used by the WebUI toolbar (if
  // enabled) for the forward and home buttons.
  int webui_toolbar_button_flex_order() const {
    return webui_toolbar_button_flex_order_;
  }

  const base::flat_map<ui::ElementIdentifier, std::unique_ptr<PopOutState>>&
  pop_out_state_for_testing() const {
    return pop_out_state_;
  }

  // Utility that recursively searches for a view with `id` from `view`.
  static views::View* FindToolbarElementWithId(views::View* view,
                                               ui::ElementIdentifier id);

  // Shows the overflow menu that is anchored to the `overflow_button_`.
  void ShowMenu();

  bool IsMenuRunning() const { return overflow_menu_.IsMenuRunning(); }

  views::MenuItemView* root_menu_item() {
    return overflow_menu_.root_menu_item();
  }

  const views::MenuItemView* root_menu_item() const {
    return overflow_menu_.root_menu_item();
  }

  const ui::SimpleMenuModel* menu_model_for_testing() const {
    return overflow_menu_.menu_model_for_testing();
  }

  OverflowMenu& overflow_menu_for_testing() { return overflow_menu_; }
  const OverflowMenu& overflow_menu_for_testing() const {
    return overflow_menu_;
  }

  // Check if element is currently overflowed.
  bool IsElementOverflowedForTesting(ui::ElementIdentifier id) const;

 private:
  friend class ToolbarControllerUiTest;
  friend class ToolbarControllerOverflowOrderingUiTest;
  friend class ToolbarControllerHighPriorityLocationBarOverflowOrderingUiTest;
  friend class ToolbarControllerUnitTest;

  // Returns currently hidden elements.
  std::vector<const ResponsiveElementInfo*> GetOverflowedElements() const;

  // Check if element has overflowed.
  bool IsOverflowed(
      const OverflowableElement& element,
      const views::ProposedLayout* proposed_layout = nullptr) const;

  // The starting flex order assigned to the last overflowed element in
  // `overflow_menu_.responsive_elements()`.
  const int element_flex_order_start_;

  // Reference to ToolbarView::container_view_. Must outlive `this`.
  const raw_ptr<views::View> toolbar_container_view_;

  const raw_ptr<WebUIToolbarControllerDelegate>
      webui_toolbar_controller_delegate_;

  // The button with a chevron icon that indicates at least one element in
  // `overflow_menu_.responsive_elements()` overflows. Owned by
  // `toolbar_container_view_`.
  raw_ptr<OverflowButton> overflow_button_;

  const raw_ptr<PinnedActionsDelegate> pinned_actions_delegate_;
  const raw_ptr<PinnedToolbarActionsModel> pinned_actions_model_;

  // A map to save the original and modified FlexSpecification of responsive
  // elements that need to pop out. Set when ToolbarController is initialized.
  base::flat_map<ui::ElementIdentifier, std::unique_ptr<PopOutState>>
      pop_out_state_;

  // Flex order specifically for the buttons that may appear on
  // kWebUIToolbarElementIdentifier. Default is used when
  // ToolbarControllerUtil::PreventOverflow() returns true, which makes order
  // for the buttons irrelevant, since they'll always be displayed at their full
  // size.
  int webui_toolbar_button_flex_order_ = 1;

  // Handles display and layout of the overflow menu, as well as keeping track
  // of and automatically sorting the ResponsiveElementInfo of all overflowable
  // elements.
  OverflowMenu overflow_menu_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_TOOLBAR_CONTROLLER_H_
