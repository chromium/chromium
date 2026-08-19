// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_OVERFLOW_MENU_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_OVERFLOW_MENU_H_

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>

#include "base/callback_list.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "chrome/browser/ui/toolbar/pinned_toolbar/pinned_toolbar_actions_model.h"
#include "ui/actions/action_id.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/models/image_model.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/menus/simple_menu_model.h"
#include "ui/views/controls/button/menu_button_controller.h"
#include "ui/views/controls/menu/menu_delegate.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/menu_runner.h"

class Browser;

namespace views {
class Widget;
}  // namespace views

namespace actions {
class ActionItem;
}  // namespace actions

// Manages the toolbar's overflow menu. Tracks responsive items that should be
// shown on the toolbar and monitors them for changes. Consumers can call
// ShowMenu() to start showing the menu (destroying any old menu, if there was
// one).
class OverflowMenu : public views::MenuDelegate,
                     public ui::SimpleMenuModel::Delegate,
                     public PinnedToolbarActionsModel::Observer {
 public:
  // Used to retrieve information about action-based pinned toolbar elements.
  class PinnedActionsInfo {
   public:
    virtual actions::ActionItem* GetActionItemFor(actions::ActionId id) = 0;

    // Returns the ordered list of pinned ActionIds.
    virtual const std::vector<actions::ActionId>& PinnedActionIds() const = 0;

   protected:
    virtual ~PinnedActionsInfo() = default;
  };

  // Data structure to store information specifically used to support
  // ui::ElementIdentifier as element reference.
  struct ElementIdInfo {
    explicit ElementIdInfo(ui::ElementIdentifier overflow_identifier,
                           int menu_text_id,
                           raw_ptr<const gfx::VectorIcon> menu_icon,
                           ui::ElementIdentifier activate_identifier,
                           std::optional<ui::ElementIdentifier>
                               observed_identifier = std::nullopt);

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

    // Pop out button when `observed_identifier` is shown. End pop out when it's
    // hidden.
    std::optional<ui::ElementIdentifier> observed_identifier;
  };

  using OverflowableElement = std::variant<ElementIdInfo, actions::ActionId>;

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
    explicit ResponsiveElementInfo(OverflowableElement overflow_id,
                                   bool is_section_end = false);
    ResponsiveElementInfo(const ResponsiveElementInfo&);
    ~ResponsiveElementInfo();

    // The toolbar element that potentially overflows.
    OverflowableElement overflow_id;

    // True if current element is a section end in overflow menu structure.
    bool is_section_end = false;
  };

  class Delegate {
   public:
    virtual void ExecuteCommand(const OverflowableElement& element) = 0;
    virtual bool IsCurrentlyOverflowed(
        const OverflowableElement& element) const = 0;
    virtual bool IsEnabled(const OverflowableElement& element) const = 0;

   protected:
    virtual ~Delegate() = default;
  };

  static std::vector<ResponsiveElementInfo> GetDefaultResponsiveElements(
      Browser* browser);

  // All passed in pointers must outlive the created OverflowMenu.
  OverflowMenu(const std::vector<ResponsiveElementInfo>& responsive_elements,
               Delegate* delegate,
               PinnedActionsInfo* pinned_actions_info,
               PinnedToolbarActionsModel* pinned_toolbar_actions_model);
  OverflowMenu(const OverflowMenu&) = delete;
  OverflowMenu& operator=(const OverflowMenu&) = delete;
  ~OverflowMenu() override;

  // PinnedToolbarActionsModel::Observer
  void OnActionsChanged() override;

  // See responsive_elements_.
  const std::vector<ResponsiveElementInfo>& responsive_elements() const {
    return responsive_elements_;
  }

  std::vector<ResponsiveElementInfo> GetResponsiveElementsWithOrderedActions()
      const;

  // Destroys any pre-existing overflow menu, and starts showing a new one.
  // `parent`, `button_controller`, and `bounds` match the first three arguments
  // of MenuRunner::RunMenuAt().
  void ShowMenu(views::Widget* parent,
                views::MenuButtonController* button_controller,
                const gfx::Rect& bounds);

  // Generate menu text from the responsive element.
  std::u16string GetMenuText(const ResponsiveElementInfo& element_info) const;

  // Get menu icon from the responsive element.
  std::optional<ui::ImageModel> GetMenuIcon(
      const ResponsiveElementInfo& element_info) const;

  bool IsMenuRunning() const;

  views::MenuItemView* root_menu_item() { return root_menu_item_.get(); }
  const views::MenuItemView* root_menu_item() const {
    return root_menu_item_.get();
  }

  const ui::SimpleMenuModel* menu_model_for_testing() const {
    return menu_model_.get();
  }

  std::unique_ptr<ui::SimpleMenuModel> CreateMenuModel();

  using MenuTextCallback =
      base::RepeatingCallback<std::u16string(const ResponsiveElementInfo&)>;

  // Sets callback to be used by GetMenuText(), instead of its normal logic.
  void set_menu_text_callback_for_testing(MenuTextCallback callback) {
    menu_text_callback_for_testing_ = std::move(callback);
  }

  // ui::SimpleMenuModel::Delegate:
  void ExecuteCommand(int command_id, int event_flags) override;
  bool IsCommandIdEnabled(int command_id) const override;

 private:
  void PopulateMenu(views::MenuItemView* parent);
  void CloseMenu();

  // Adds the status indicator to all the menu items and makes it visible if
  // needed.
  void ShowStatusIndicator();

  // Listens to changes in `action_item` and updates the visibility of the
  // status indicator.
  void ActionItemChanged(actions::ActionItem* action_item);

  // The list of all elements that support overflow.
  // Actions are kept in order by observing the PinnedToolbarActionsModel.
  // To facilitate menu creation elements order should match overflow
  // menu top to bottom.
  std::vector<ResponsiveElementInfo> responsive_elements_;

  std::vector<base::CallbackListSubscription> action_changed_subscription_;

  std::unique_ptr<views::MenuRunner> menu_runner_;
  std::unique_ptr<ui::SimpleMenuModel> menu_model_;
  raw_ptr<views::MenuItemView> root_menu_item_ = nullptr;

  const raw_ref<Delegate> delegate_;
  const raw_ptr<PinnedActionsInfo> pinned_actions_info_;
  const raw_ptr<PinnedToolbarActionsModel> pinned_actions_model_;

  MenuTextCallback menu_text_callback_for_testing_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_OVERFLOW_MENU_H_
