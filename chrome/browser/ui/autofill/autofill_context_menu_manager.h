// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_CONTEXT_MENU_MANAGER_H_
#define CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_CONTEXT_MENU_MANAGER_H_

#include "base/containers/flat_map.h"
#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "base/types/strong_alias.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/renderer_context_menu/render_view_context_menu_base.h"
#include "content/public/browser/context_menu_params.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/models/simple_menu_model.h"

class Browser;

namespace autofill {

class AutofillProfile;
class CreditCard;
class PersonalDataManager;

// Determines the type of the item added to the context menu.
enum SubMenuType {
  // Addresses
  SUB_MENU_TYPE_ADDRESS = 0,
  // Credit Cards
  SUB_MENU_TYPE_CREDIT_CARD = 1,
  // Passwords
  SUB_MENU_TYPE_PASSWORD = 2,
  NUM_SUBMENU_TYPES
};

// Stores data about an item added to the  context menu.
struct ContextMenuItem {
  // Represents the string value that is displayed in a row in the context menu.
  // If selected, should result in filling.
  std::u16string fill_value;

  // Represents the type that this item belongs to out of address, credit cards
  // and passwords.
  SubMenuType sub_menu_type;

  // The context menu item represents a manage option. Example: Manage
  // addresses/ Manage payment methods options.
  bool is_manage_item = false;
};

// `AutofillContextMenuManager` is responsible for adding/executing Autofill
// related context menu items. `RenderViewContextMenu` is intended to own and
// control the lifetime of `AutofillContextMenuManager`.
// Here is an example of the structure of the autofill related context menu
// items:
// ...
// ...
// ...
// Fill Address Info > Alex Park, 345 Spear Street... > Alex Park
//                                                      ___________________
//                                                      345 Spear Street
//                                                      San Francisco
//                                                      94105
//                                                      ___________________
//                                                      +1 858 230 4000
//                                                      alexpark@gmail.com
//                                                      ___________________
//                                                      Other             > Alex
//                                                                          Park
//                                                                          ___________
//                                                                          345
//                                                                          Spear
//                                                                          Street
//                     ______________________________
//                     Manage Addresses
// Fill Payment      > Mastercard **** 0952           > Alex Park
//                                                      **** **** **** 0952
//                                                      ___________________
//                                                      04
//                                                      2025
//                     ______________________________
//                     Manage payment methods
//
// Provide Autofill feedback
// ....
// ....
// ....
//
// From the example, there are 3 layers:
// 1. Outermost layer that distinguishes between address or payment method
// filling. Refer to `AppendItems` and `AddAddressOrCreditCardItemsToMenu` for
// more info. It also includes an option to submit user feedback on Autofill.
// 2. Profile description layer to identify which profile to use for filling.
// Refer to `AddAddressOrCreditCardItemsToMenu` for more info.
// 3. Profile data layer that would be used for filling a single field. See
// `AddAddressOrCreditCardItemToMenu` and `AddProfileDataToMenu` for details on
// how this is done.
// 4. The Other Section, that suggests more granular data for filling. See
// `AddAddressOrCreditCardItemToMenu` and `AddProfileDataToMenu` for details.
class AutofillContextMenuManager {
 public:
  // Represents command id used to denote a row in the context menu. The
  // command ids are created when the items are added to the context menu during
  // it's initialization. For Autofill, it ranges from
  // `IDC_CONTENT_CONTEXT_AUTOFILL_CUSTOM_FIRST` to
  // `IDC_CONTENT_CONTEXT_AUTOFILL_CUSTOM_LAST`.
  using CommandId = base::StrongAlias<class CommandIdTag, int>;

  // Convert a command ID so that it fits within the range for
  // autofill context menu. `offset` is the count of the items added to the
  // autofill context menu.
  static CommandId ConvertToAutofillCustomCommandId(int offset);

  // Returns true if the given id is one generated for autofill context menu.
  static bool IsAutofillCustomCommandId(CommandId command_id);

  AutofillContextMenuManager(PersonalDataManager* personal_data_manager,
                             RenderViewContextMenuBase* delegate,
                             ui::SimpleMenuModel* menu_model,
                             Browser* browser);
  ~AutofillContextMenuManager();
  AutofillContextMenuManager(const AutofillContextMenuManager&) = delete;
  AutofillContextMenuManager& operator=(const AutofillContextMenuManager&) =
      delete;

  // Adds items such as "Addresses"/"Credit Cards"/"Passwords" to the top level
  // of the context menu.
  void AppendItems();

  // `AutofillContextMenuManager` specific, called from `RenderViewContextMenu`.
  bool IsCommandIdChecked(CommandId command_id) const;
  bool IsCommandIdVisible(CommandId command_id) const;
  bool IsCommandIdEnabled(CommandId command_id) const;
  void ExecuteCommand(CommandId command_id);

  // Getter for `command_id_to_menu_item_value_mapper_` used for testing
  // purposes.
  const base::flat_map<CommandId, ContextMenuItem>&
  command_id_to_menu_item_value_mapper_for_testing() const {
    return command_id_to_menu_item_value_mapper_;
  }

  // Setter for `params_` used for testing purposes.
  void set_params_for_testing(content::ContextMenuParams params) {
    params_ = params;
  }

 private:
  // The "Titles" refers to the description of the address and credit cards
  // respectively that is shown in the context menu.
  // This stores the mapping of the description of the card/address
  using AddressProfilesWithTitles =
      base::flat_map<std::u16string, AutofillProfile*>;
  using CreditCardProfilesWithTitles =
      base::flat_map<std::u16string, CreditCard*>;

  // Used to define the order in which the data is added to the context menu.
  struct FieldsToShow {
    // Denotes whether the item type is a submenu, separator or a text.
    ui::MenuModel::ItemType item_type;
    // Denotes the `ServerFieldType` pertaining to the row to be added to the
    // menu.
    ServerFieldType field_type;
  };

  // Depending on the type
  // `AddressProfilesWithTitles`/`CreditCardProfilesWithTitle` present in the
  // variant, it adds an address menu with all the addresses or the credit card
  // menu with the credit card data to the context menu.
  void AddAddressOrCreditCardItemsToMenu(
      std::vector<std::pair<CommandId, ContextMenuItem>>&
          detail_items_added_to_context_menu,
      absl::variant<AddressProfilesWithTitles, CreditCardProfilesWithTitles>
          addresses_or_credit_cards);

  // Creates a submenu for the `profile` data and calls `AddProfileDataToMenu`
  // to add the corresponding data to the menu.
  // Also takes care of adding "Other" section for the addresses.
  void AddAddressOrCreditCardItemToMenu(
      absl::variant<const AutofillProfile*, const CreditCard*> profile,
      const std::u16string& profile_title,
      base::span<const FieldsToShow> field_types_to_show,
      base::span<const FieldsToShow> other_fields_to_show,
      ui::SimpleMenuModel* menu,
      std::vector<std::pair<CommandId, ContextMenuItem>>&
          detail_items_added_to_context_menu);

  // Fetches value from `profile_or_credit_card` based on the type from
  // `field_types_to_show` and adds them to the `menu_model`.
  void AddProfileDataToMenu(
      absl::variant<const AutofillProfile*, const CreditCard*>
          profile_or_credit_card,
      base::span<const FieldsToShow> field_types_to_show,
      ui::SimpleMenuModel* menu_model,
      std::vector<std::pair<CommandId, ContextMenuItem>>&
          item_details_added_to_context_menu,
      SubMenuType sub_menu_type);

  // Returns true if the command ids left are sufficient for showing the whole
  // profile in the context menu.
  bool HaveEnoughIdsForProfile(
      absl::variant<const AutofillProfile*, const CreditCard*>
          profile_or_credit_card,
      base::span<const FieldsToShow> field_types_to_show,
      base::span<const FieldsToShow> other_fields_to_show);

  // Triggers the feedback flow for Autofill command.
  void ExecuteAutofillFeedbackCommand(content::RenderFrameHost* rfh);

  // Triggers the corresponding menu manager command.
  void ExecuteMenuManagerCommand(CommandId command_id,
                                 content::RenderFrameHost* rfh);

  // Returns a map of the addresses along with the title to be shown in the
  // context menu.
  AddressProfilesWithTitles GetAddressProfilesWithTitles();
  // Returns a map of the credit cards along with the title to be shown in the
  // context menu.
  CreditCardProfilesWithTitles GetCreditCardProfilesWithTitles();

  // Creates an instance of `ui::SimpleMenuModel` and adds it to
  // `cached_menu_models_` to maintain it's lifetime.
  ui::SimpleMenuModel* CreateSimpleMenuModel();

  // Returns a description for the given `profile`.
  std::u16string GetProfileDescription(const AutofillProfile& profile);

  // Returns the next available command id for adding an item to the context
  // menu for Autofill.
  absl::optional<CommandId> GetNextAvailableAutofillCommandId();

  const raw_ptr<PersonalDataManager, DanglingUntriaged> personal_data_manager_;
  const raw_ptr<ui::SimpleMenuModel> menu_model_;
  const raw_ptr<RenderViewContextMenuBase> delegate_;
  const raw_ptr<Browser, DanglingUntriaged> browser_;
  content::ContextMenuParams params_;

  // Stores the count of items added to the context menu from Autofill.
  int count_of_items_added_to_menu_model_ = 0;

  // Keep track of and clean up menu models for submenus.
  std::vector<std::unique_ptr<ui::SimpleMenuModel>> cached_menu_models_;

  // Stores the mapping of command ids with the item values added to the context
  // menu.
  // Only items that contain the address or credit card details are stored.
  base::flat_map<CommandId, ContextMenuItem>
      command_id_to_menu_item_value_mapper_;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_CONTEXT_MENU_MANAGER_H_
