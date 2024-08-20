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
#include "ui/actions/action_id.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/layout/proposed_layout.h"
#include "ui/views/view.h"

class Browser;

// Manages toolbar elements' visibility using flex rules. This also owns the
// overflow menu and the logic to generate the menu model. It also listens to
// action item changes and updates the menu as required.
class ToolbarController : public views::MenuDelegate,
                          public ui::SimpleMenuModel::Delegate {
 public:
  // Manages action-based pinned toolbar elements.
  class PinnedActionsDelegate {
   public:
    virtual actions::ActionItem* GetActionItemFor(
        const actions::ActionId& id) = 0;

    // Returns true if the corresponding element is hidden.
    virtual bool IsOverflowed(const actions::ActionId& id) = 0;

    virtual views::View* GetContainerView() = 0;

    // Return true if any buttons should overflow given available size.
    virtual bool ShouldAnyButtonsOverflow(gfx::Size available_size) const = 0;

   protected:
    virtual ~PinnedActionsDelegate() = default;
  };

  // Data structure to store information specifically used to support
  // ui::ElementIdentifier as element reference.
  struct ElementIdInfo {
    // The identifier of toolbar element that potentially overflows.
    ui::ElementIdentifier overflow_identifier;

    // Menu text when the element is overflow to the overflow menu. For
    // ActionId-based elements this value is supplied when constructing action
    // items.
    int menu_text_id;

    // Menu item icon. nullptr if this menu item has no icon.
    raw_ptr<const gfx::VectorIcon> menu_icon = nullptr;

    // The toolbar button to be activated with menu text pressed. This is not
    // necessarily the same as the element that overflows. E.g. when the
    // overflowed element is kToolbarExtensionsContainerElementId the
    // `activate_identifier` should be kExtensionsMenuButtonElementId.
    ui::ElementIdentifier activate_identifier;
  };

  // Data structure to store information of responsive elements. Supports both
  // ui::ElementIdentifier and ActionId as element reference.
  struct ResponsiveElementInfo {
    // Overflow menu structure:
    // -------------------
    // | Forward         |
    // |-----------------|
    // | Home            | -> section end
    // |=================| -> potential separator
    // | Reading list    |
    // |-----------------|
    // | Bookmarks       | -> section end
    // |=================| -> potential separator
    // | Labs            |
    // |-----------------|
    // | Cast            |
    // |-----------------|
    // | Media controls  |
    // |-----------------|
    // | Downloads       | -> section end
    // |=================| -> potential separator
    // | Profile         |
    // |-----------------|

    ResponsiveElementInfo(absl::variant<ElementIdInfo, actions::ActionId>,
                          bool = false,
                          std::optional<ui::ElementIdentifier> = std::nullopt);
    ResponsiveElementInfo(const ResponsiveElementInfo&);
    ~ResponsiveElementInfo();

    // The toolbar element that potentially overflows.
    absl::variant<ElementIdInfo, actions::ActionId> overflow_id;

    // True if current element is a section end in overflow menu structure.
    bool is_section_end = false;

    // Pop out button when `observed_identifier` is shown. End pop out when it's
    // hidden. Could be empty e.g. when `overflow_key` is an ActionId that opens
    // a side panel rather than a View bubble.
    std::optional<ui::ElementIdentifier> observed_identifier;
  };

  ToolbarController(
      const std::vector<ResponsiveElementInfo>& responsive_elements,
      const std::vector<ui::ElementIdentifier>& elements_in_overflow_order,
      int element_flex_order_start,
      views::View* toolbar_container_view,
      OverflowButton* overflow_button,
      PinnedActionsDelegate* PinnedActionsDelegate);
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

  // Return the default responsive elements list in the toolbar.
  static std::vector<ResponsiveElementInfo> GetDefaultResponsiveElements(
      Browser* browser);

  // Return the element list in desired overflow order. The list should contain
  // only the immediate children of toolbar i.e. those managed by
  // `toolbar_container_view_` layout manager. For those inside a child
  // container (e.g. PinnedToolbarActionsContainer) of `toolbar_container_view_`
  // they should have their own overflow order.
  static std::vector<ui::ElementIdentifier> GetDefaultOverflowOrder();

  // Return the action name from element identifier. Return empty if not found.
  static std::string GetActionNameFromElementIdentifier(
      absl::variant<ui::ElementIdentifier, actions::ActionId> identifier);

  // Force the UI element with the identifier to show. Return whether the action
  // is successful.
  virtual bool PopOut(ui::ElementIdentifier identifier);

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

  const base::flat_map<ui::ElementIdentifier, std::unique_ptr<PopOutState>>&
  pop_out_state_for_testing() const {
    return pop_out_state_;
  }

  // Create the overflow menu model for hidden buttons.
  std::unique_ptr<ui::SimpleMenuModel> CreateOverflowMenuModel();

  // Generate menu text from the responsive element.
  virtual std::u16string GetMenuText(
      const ResponsiveElementInfo& element_info) const;

  // Get menu icon from the responsive element.
  std::optional<ui::ImageModel> GetMenuIcon(
      const ResponsiveElementInfo& element_info) const;

  // Utility that recursively searches for a view with `id` from `view`.
  static views::View* FindToolbarElementWithId(views::View* view,
                                               ui::ElementIdentifier id);

  // Shows the overflow menu that is anchored to the `overflow_button_`.
  void ShowMenu();

  bool IsMenuRunning() const;

  const views::MenuItemView* root_menu_item() const {
    return root_menu_item_.get();
  }

  const ui::SimpleMenuModel* menu_model_for_testing() const {
    return menu_model_.get();
  }

 private:
  friend class ToolbarControllerUiTest;
  friend class ToolbarControllerUnitTest;

  // Returns currently hidden elements.
  std::vector<const ResponsiveElementInfo*> GetOverflowedElements();

  // Check if element has overflowed. Check the visibility in proposed_layout if
  // provided.
  bool IsOverflowed(
      const ResponsiveElementInfo& element,
      const views::ProposedLayout* proposed_layout = nullptr) const;

  void PopulateMenu(views::MenuItemView* parent);
  void CloseMenu();

  // Adds the status indicator to all the menu items and makes it visible if
  // needed.
  void ShowStatusIndicator();

  // Listens to changes in `action_item` and updates the visibility of the
  // status indicator.
  void ActionItemChanged(actions::ActionItem* action_item);

  // ui::SimpleMenuModel::Delegate:
  void ExecuteCommand(int command_id, int event_flags) override;
  bool IsCommandIdEnabled(int command_id) const override;

  // The toolbar elements managed by this controller.
  // This also serves as a map that its indices are used as command ids in
  // overflowed menu model. To facilitate menu creation elements order should
  // match overflow menu top to bottom.
  const std::vector<ResponsiveElementInfo> responsive_elements_;

  std::vector<base::CallbackListSubscription> action_changed_subscription_;

  // The starting flex order assigned to the last overflowed element in
  // `responsive_elements_`.
  const int element_flex_order_start_;

  // Reference to ToolbarView::container_view_. Must outlive `this`.
  const raw_ptr<views::View> toolbar_container_view_;

  // The button with a chevron icon that indicates at least one element in
  // `responsive_elements_` overflows. Owned by `toolbar_container_view_`.
  raw_ptr<OverflowButton> overflow_button_;

  std::unique_ptr<views::MenuRunner> menu_runner_;
  std::unique_ptr<ui::SimpleMenuModel> menu_model_;
  raw_ptr<views::MenuItemView> root_menu_item_ = nullptr;

  const raw_ptr<PinnedActionsDelegate> pinned_actions_delegate_;

  // A map to save the original and modified FlexSpecification of responsive
  // elements that need to pop out. Set when ToolbarController is initialized.
  base::flat_map<ui::ElementIdentifier, std::unique_ptr<PopOutState>>
      pop_out_state_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_TOOLBAR_CONTROLLER_H_
