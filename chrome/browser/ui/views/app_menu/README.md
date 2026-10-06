# App Menu

This directory contains the Views implementation of the redesigned, action-based Chrome Application Menu ("Block Style ChroMenu")

Rather than using `ui::MenuModel` / `ui::SimpleMenuModel` and command ID (`IDC_*`) dispatch, `ActionAppMenu` is built directly on top of the `ui/actions` framework (`actions::ActionItem`, `actions::IndirectActionItem`, and `views::ActionViewController`). Using `ui/actions` provides a single source of truth for action metadata and execution shared across browser UI surfaces, reactive view-state synchronization while the menu is open, and a composable tree structure with extensible `ui::ClassProperty` metadata that drives both custom menu layouts and in-menu search indexing.

---

## Architecture & Core Components

```
BrowserAppMenuButton
  └── ActionAppMenu (Coordinator, views::MenuDelegate, AppMenuDragAndDropDelegate::Host)
        ├── AppMenuActionManager (Builds the ActionItem hierarchy via AppMenuBuilder)
        │     ├── RecentTabsDynamicMenu
        │     ├── BookmarksDynamicMenu (also implements AppMenuDragAndDropDelegate)
        │     ├── TabGroupDynamicMenu
        │     ├── SendTabToSelfDynamicMenu
        │     └── ProfileDynamicMenu
        |     └── ...
        ├── ActionAppMenuMetrics (Logs WrenchMenu.* histograms & user actions)
        └── Custom Views (Block, Footer, SearchBar, Zoom, Chip, MinorText)
```

### 1. Model & Hierarchy Layer (`AppMenuActionManager` & `AppMenuActionItem`)
* **`BrowserActions` (`chrome/browser/ui/browser_actions.cc`)**: Owns the canonical per-browser `actions::ActionItem` instances (state, text, icons, accelerators, and invocation callbacks) under the browser's root action item, including the `kActionAppMenuRoot` anchor. There is a 1:many relationship between `ActionItem`s and UI elements—a single `ActionItem` can simultaneously back multiple surfaces (e.g., a toolbar button, a side panel entry, a keyboard shortcut, and an app menu item).
* **`AppMenuActionItem` (`app_menu_action_item.h`)**: Defines the menu-specific `ui::ClassProperty` keys (e.g., `kDisplayTypeKey`, `kContainerColorKey`, `kItemHeightKey`, `kTextOverrideKey`, `kIconOverrideKey`, `kChipTextKey`, `kIsAlertedKey`, `kDragAndDropDelegateKey`) and factory helpers:
  * `AppMenuActionItem::CreateIndirect()`: Wraps a canonical `ActionItem` from `BrowserActions` in an `actions::IndirectActionItem`. This allows the menu to compose an ephemeral hierarchy and attach per-menu overrides (like custom labels or IPH alert states) without mutating or reparenting the underlying `BrowserActions` items.
  * `AppMenuActionItem::CreateHeader()` / `CreateDivider()`: Creates structural `ActionItem` nodes for section titles and separators.
* **`AppMenuActionManager` (`app_menu_action_manager.h`)**: Constructs the menu's `BaseAction` tree under `kActionAppMenuRoot` when the menu opens (`CreateMenuHierarchy()`) and tears it down when the menu closes (`OnMenuClosed()`). Uses the internal `AppMenuBuilder` fluent helper to declaratively compose the menu using `AppMenuActionItem::DisplayType`s:
  * **Searchable Display Types (`< DisplayType::kMaxSearchable`)**:
    * **`kRow`**: Standard clickable menu item or submenu entry (the default for `AddAction()` and `AddSubmenu()`).
    * **`kSection`**: Structural grouping container (`AddSection(DisplayType::kSection, ...)`) that applies a shared section background color (`kContainerColorKey`) and automatic top/bottom corner rounding to its child rows while rendering them inline in the parent menu.
    * **`kBlock`**: Horizontal block-button container and its child actions (`AppMenuBlockView`), used for prominent top-level actions (e.g., New Tab, New Window).
    * **`kNotification`**: Standalone alert row at the top of the menu with individual rounded corners, used for high-priority prompts (e.g., browser updates, Safety Hub notifications, global errors).
    * **`kCustom`**: Row that lays out child actions as inline interactive controls within the same row rather than spawning a popup submenu (e.g., `AppMenuZoomView`), while keeping child actions in the hierarchy for search and state binding.
  * **Non-Searchable Display Types (`>= DisplayType::kMaxSearchable`)**:
    * **`kSearch`**: Top-level search input row (`AppMenuSearchBarView`) that filters searchable menu items in real time.
    * **`kHeader`**: Non-interactive section title heading (`AddHeader()`).
    * **`kDivider`**: Visual line or spacing separator (`AddDivider()`).
    * **`kFooter`**: Bottom container (`AppMenuFooterView`) that renders child actions as horizontal pill buttons alongside optional status rows (e.g., enterprise management banner).

### 2. View & Coordinator Layer (`ActionAppMenu`)
* **`ActionAppMenu` (`action_app_menu.h`)**: Owns the `views::MenuRunner` and serves as the `views::MenuDelegate` and `AppMenuDragAndDropDelegate::Host`.
  * Recursively traverses the `BaseAction` hierarchy (`PopulateMenu()`) and translates each node into `views::MenuItemView` rows or embedded custom `views::View`s based on `AppMenuActionItem::DisplayType`.
  * Binds `MenuItemView` instances to their underlying `ActionItem` via `views::ActionViewController` so state updates (enabled, visible, checked, text, icon) sync reactively.
  * Automatically computes rounded container backgrounds (`ShouldRoundTopCorners`, `ShouldRoundBottomCorners`) and vertical section padding for grouped rows sharing a `kContainerColorKey`.
  * Defers populating dynamic submenus in the view tree until `WillShowMenu()` is triggered, while still allowing their action children to be indexed for search.

### 3. Dynamic Submenus (`*_dynamic_menu.h`)
Dynamic submenus are used for user data (such as bookmarks, history, tab groups, synced devices, and profiles) where the menu items and their actions are not known at compile time and must be generated at runtime from browser services:
* **`BookmarksDynamicMenu`**: Populates bookmark folders/nodes from `BookmarkMergedSurfaceService`, observes model mutations while the menu is open, and implements `AppMenuDragAndDropDelegate`.
* **`RecentTabsDynamicMenu`**: Populates recently closed tabs, windows, tab groups, split tabs, and foreign synced tabs.
* **`TabGroupDynamicMenu`**: Populates saved tab groups and per-group submenus.
* **`SendTabToSelfDynamicMenu`**: Populates target sync devices for sharing the active tab.
* **`ProfileDynamicMenu`**: Populates sync/sign-in status actions and other local profiles.

### 4. Search (`AppMenuSearchController` & `AppMenuSearchItem`)
* **`AppMenuSearchController` (`app_menu_search_controller.h`)**: Traverses the `kActionAppMenuRoot` hierarchy via `InitializeSearchIndex()`, extracting visible and enabled leaf actions with `DisplayType < DisplayType::kMaxSearchable` into a flat list of `AppMenuSearchItem`s.
* Queries `FuzzyFinder` (`Search()`) and partitions matches into capped sections (**Actions**, **Bookmarks**, **Tab Groups**, **Recent Tabs**), returning an ephemeral `ActionItem` results tree wrapped in `IndirectActionItem`s with secondary breadcrumb labels.

### 5. Custom Views
* **`AppMenuBlockView` / `AppMenuBlockButton`**: Horizontal row of equal-width rectangular buttons at the top of the menu.
* **`AppMenuFooterView` / `AppMenuFooterButton`**: Bottom bar with pill-shaped buttons (Settings, Help, Exit) and an optional full-width management row.
* **`AppMenuZoomView`**: Custom inline controls (`-`, zoom percentage, `+`, Fullscreen) embedded in the `kActionZoomSubmenu` row.
* **`AppMenuSearchBarView`**: Top search `views::Textfield` with custom focus and event routing inside the menu popup.
* **`AppMenuChipView` & `AppMenuMinorTextView`**: Helper views attached to standard `MenuItemView` rows for status chips (e.g., sign-in state) and non-accelerator minor text.

---

## Rules & Guidelines for Extending the Menu

### 1. Adding a Standard Menu Item
1. **Define or reuse an `ActionId` in `BrowserActions`**:
   * **If creating a new action**: Declare the `ActionId` in `chrome/browser/ui/actions/chrome_action_id.h` and register the `ActionItem` in `BrowserActions` (`chrome/browser/ui/browser_actions.cc`) with its title, icon, accelerator, and invocation callback. Do **not** embed command execution logic directly inside `ActionAppMenu` or `AppMenuActionManager`.
   * **If reusing an existing `ActionId`**: Because a single `ActionItem` may be shared across multiple UI elements (such as toolbar buttons or side panels), do **not** mutate the canonical `ActionItem` directly for app-menu-specific presentation differences. Instead, apply menu-specific customizations on the `IndirectActionItem` via `AppMenuActionItem::ActionParams` (such as `.text_override` and `.icon_override`).
   * If the action's enabled/visible state depends on browser command state, wire it in `CommandActionUpdater` (`chrome/browser/ui/actions/command_action_updater.cc`).
2. **Add the item in `AppMenuActionManager`**:
   * Use `AppMenuBuilder::AddAction(kActionYourNewItem, params)` in the appropriate section method (`AddYourChromeActions`, `AddToolsAndActionsActions`, etc.).
   * **Rule**: Always use `AppMenuBuilder::AddAction` (which calls `AppMenuActionItem::CreateIndirect`) for registered `ActionId`s rather than creating raw `ActionItem`s, so the browser action tree is not reparented.
   * Use `AppMenuActionItem::ActionParams` only for menu-specific presentation overrides:
     * `.text_override` / `.icon_override`: Contextual label/icon changes when opening the menu.
     * `.element_id`: `ui::ElementIdentifier` for User Education / IPH / interactive UI tests (`AppMenuBuilder` automatically checks if `element_id` is targeted by an active IPH promo or running tutorial and sets `kIsAlertedKey`).
     * `.new_badge_feature`: Pointer to a `base::Feature` to display a "New" badge via `UserEducationService`.
     * `.is_checkable = true`: Renders the item as a checkbox menu item bound to `ActionItem::GetChecked()`.
     * `.chip_text` / `.minor_text` / `.secondary_text`: Attaches supplementary text or a status chip.
3. **Register Metrics in `ActionAppMenuMetrics`**:
   * **Rule**: `ActionAppMenuMetrics::LogMenuActionWithId()` (`action_app_menu_metrics.cc`) has a `NOTREACHED()` default branch. Every `ActionId` added to the menu **must** be handled in `LogMenuActionWithId()`—either calling `RecordAction(MENU_ACTION_*, "<TimeToActionSuffix>")` or `RecordTimeToAction()`.
4. **Update `ActionAppMenuTestBase`**:
   * Register the test `ActionId` in `ActionAppMenuTestBase::SetUp()` (`action_app_menu_test_base.cc`) and add unit tests in `action_app_menu_manager_unittest.cc` and `action_app_menu_unittest.cc`.

### 2. Adding a Static Submenu
Use `AppMenuBuilder::AddSubmenu()` with a parent submenu `ActionId` (registered via `ChromeMenuAction` in `BrowserActions::InitializeSubmenuActions()`) and a builder lambda:
```cpp
section.AddSubmenu(
    kActionMySubmenu,
    [](AppMenuBuilder& sub) {
      sub.AddAction(kActionFirstChild)
          .AddDivider()
          .AddAction(kActionSecondChild);
    },
    {.element_id = kMySubmenuElementId});
```
* Alert states on any child inside `build_submenu` automatically propagate upward to highlight the parent submenu item.

### 3. Adding a Dynamic Submenu or Section
When submenu items depend on runtime models (e.g., user data, synced entities, devices):
1. Create a dedicated `*DynamicMenu` helper class owned by `AppMenuActionManager`.
2. Register the submenu in `AppMenuActionManager` using `AppMenuBuilder::AddDynamicSubmenu(id, populate_callback, optional_static_children_lambda, params)` (or `AddDynamicSection()` if injecting into an existing submenu like Profile).
3. **Rules for Dynamic Items**:
   * Dynamic user entries (such as individual bookmarks, recent tabs, or profiles) should be created with `actions::ActionItem::Builder()` **without** a predefined `ActionId`, and with their own `SetInvokeActionCallback()`.
   * Built-in commands inside a dynamic submenu (e.g., "Bookmark this tab") should still use `AppMenuActionItem::CreateIndirect()` with their `ActionId`.
   * Why this distinction matters:
     * **Search (`AppMenuSearchController`)**: Items with an `ActionId` are categorized as `AppMenuSearchItem::Type::kAction`, whereas items without an `ActionId` inherit their enclosing submenu's category (`kBookmark`, `kRecentTabs`, `kTabGroup`). If you add a new searchable dynamic category, update `GetSubmenuType()` and `SearchResults` in `app_menu_search_controller.cc`.
     * **Metrics (`ActionAppMenuMetrics`)**: When a dynamic item without an `ActionId` is invoked, `ActionAppMenuMetrics::LogMenuAction()` walks up `base_action->GetParent()` to match the enclosing submenu's `ActionId` (e.g., `kActionBookmarksSubmenu`) and logs the appropriate histogram. Add your submenu's `ActionId` to that parent-walk loop in `action_app_menu_metrics.cc`.
   * Read `chrome::kDispositionKey` from `actions::ActionInvocationContext` in your invoke callback if the item supports middle-click / modifier-click window dispositions.

### 4. Supporting Drag-and-Drop in a Dynamic Submenu
To support dragging and dropping items within a dynamic submenu while keeping the menu open:
1. Implement `AppMenuDragAndDropDelegate` (`app_menu_drag_and_drop_delegate.h`) on your dynamic menu class.
2. Attach the delegate to the root action of your dynamic submenu via:
   ```cpp
   parent_item->SetProperty(AppMenuActionItem::kDragAndDropDelegateKey,
                            static_cast<AppMenuDragAndDropDelegate*>(this));
   ```
   `ActionAppMenu` automatically walks up the `BaseAction` ancestor chain (`GetDragAndDropDelegate()`) for any `MenuItemView` inside that submenu and forwards all `views::MenuDelegate` drag-and-drop calls to your delegate.
3. When the underlying data model mutates while the menu is open, call `AppMenuDragAndDropDelegate::Host::UpdateMenuItem(action, target_parent_action, insert_after)` to incrementally move or remove the `BaseAction` and its corresponding `MenuItemView` in place, or `Host::CloseMenu()` if the menu must be dismissed.

### 5. Adding a Custom Inline Row or Section View
* **Prefer standard `DisplayType::kRow` items** with properties (`kChipTextKey`, `kMinorTextKey`, `kSecondaryTextKey`, `kMinorIconKey`, `kItemHeightKey`) whenever possible.
* If a row requires custom interactive child controls inline (like `AppMenuZoomView`):
  1. Set `.display_type = DisplayType::kCustom` on the parent action item and add its child actions as children in the `ActionItem` hierarchy.
  2. Handle the parent's `ActionId` in `ActionAppMenu::PopulateCustomRow()` (`action_app_menu.cc`).
  3. Bind child controls to their `ActionItem`s via `views::ActionViewController` and record metrics via `ActionAppMenuMetrics`.
* **Searchability rule**: Note in `AppMenuActionItem::DisplayType` that enum values `< DisplayType::kMaxSearchable` (`kBlock`, `kCustom`, `kNotification`, `kRow`, `kSection`) are traversed during search indexing, while values `>= DisplayType::kMaxSearchable` (`kDivider`, `kFooter`, `kHeader`, `kSearch`) are skipped.

---

## Testing

* **Unit Tests**: Run `unit_tests --gtest_filter="*ActionAppMenu*:*AppMenu*:*ProfileDynamicMenu*"`
  * [`action_app_menu_manager_unittest.cc`](action_app_menu_manager_unittest.cc): Tests the `ActionItem` tree structure, visibility conditions, overrides, and dynamic menu population.
  * [`action_app_menu_unittest.cc`](action_app_menu_unittest.cc): Tests `MenuItemView` generation, styling, rounded corners, command execution, drag-and-drop updates, and UMA metrics.
  * [`app_menu_search_controller_unittest.cc`](app_menu_search_controller_unittest.cc): Tests hierarchy flattening, breadcrumb extraction, and categorized fuzzy search results.
* **Browser Tests**: Run `browser_tests --gtest_filter="ActionAppMenu*"`
  * [`action_app_menu_browsertest.cc`](action_app_menu_browsertest.cc): End-to-end integration tests with `BrowserAppMenuButton` and theme/color provider validation.
