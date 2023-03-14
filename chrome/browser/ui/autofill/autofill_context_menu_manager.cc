// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/autofill_context_menu_manager.h"

#include <string>

#include "base/containers/adapters.h"
#include "base/containers/flat_map.h"
#include "base/functional/overloaded.h"
#include "base/values.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/grit/generated_resources.h"
#include "components/autofill/content/browser/content_autofill_client.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/core/browser/autofill_feedback_data.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/models/menu_model.h"
#include "ui/gfx/favicon_size.h"

namespace autofill {

namespace {

// The range of command IDs reserved for autofill's custom menus.
static constexpr int kAutofillContextCustomFirst =
    IDC_CONTENT_CONTEXT_AUTOFILL_CUSTOM_FIRST;
static constexpr int kAutofillContextCustomLast =
    IDC_CONTENT_CONTEXT_AUTOFILL_CUSTOM_LAST;
static constexpr int kAutofillContextFeedback =
    IDC_CONTENT_CONTEXT_AUTOFILL_FEEDBACK;

base::Value::Dict LoadTriggerFormAndFieldLogs(
    AutofillManager* manager,
    content::RenderFrameHost* rfh,
    const content::ContextMenuParams& params) {
  if (!params.form_renderer_id) {
    return base::Value::Dict();
  }

  LocalFrameToken frame_token(rfh->GetFrameToken().value());
  FormGlobalId form_global_id = {frame_token,
                                 FormRendererId(*params.form_renderer_id)};

  base::Value::Dict trigger_form_logs;
  if (FormStructure* form = manager->FindCachedFormById(form_global_id)) {
    trigger_form_logs.Set("triggerFormSignature", form->FormSignatureAsStr());

    if (params.field_renderer_id) {
      FieldGlobalId field_global_id = {
          frame_token, FieldRendererId(*params.field_renderer_id)};
      auto field =
          base::ranges::find_if(*form, [&field_global_id](const auto& field) {
            return field->global_id() == field_global_id;
          });
      if (field != form->end()) {
        trigger_form_logs.Set("triggerFieldSignature",
                              (*field)->FieldSignatureAsStr());
      }
    }
  }
  return trigger_form_logs;
}

}  // namespace

// static
AutofillContextMenuManager::CommandId
AutofillContextMenuManager::ConvertToAutofillCustomCommandId(int offset) {
  return AutofillContextMenuManager::CommandId(
      kAutofillContextCustomFirst + SubMenuType::NUM_SUBMENU_TYPES + offset);
}

// static
bool AutofillContextMenuManager::IsAutofillCustomCommandId(
    CommandId command_id) {
  if (command_id.value() == kAutofillContextFeedback) {
    return true;
  }
  return command_id.value() >= kAutofillContextCustomFirst &&
         command_id.value() <= kAutofillContextCustomLast;
}

AutofillContextMenuManager::AutofillContextMenuManager(
    PersonalDataManager* personal_data_manager,
    RenderViewContextMenuBase* delegate,
    ui::SimpleMenuModel* menu_model,
    Browser* browser)
    : personal_data_manager_(personal_data_manager),
      menu_model_(menu_model),
      delegate_(delegate),
      browser_(browser) {
  DCHECK(delegate_);
  params_ = delegate_->params();
}

AutofillContextMenuManager::~AutofillContextMenuManager() {
  cached_menu_models_.clear();
  command_id_to_menu_item_value_mapper_.clear();
}

base::flat_map<std::u16string, AutofillProfile*>
AutofillContextMenuManager::GetAddressProfilesWithTitles() {
  std::vector<std::pair<std::u16string, AutofillProfile*>> profiles;
  for (AutofillProfile* profile : personal_data_manager_->GetProfiles())
    profiles.emplace_back(GetProfileDescription(*profile), profile);

  return base::flat_map<std::u16string, AutofillProfile*>(std::move(profiles));
}

base::flat_map<std::u16string, CreditCard*>
AutofillContextMenuManager::GetCreditCardProfilesWithTitles() {
  std::vector<std::pair<std::u16string, CreditCard*>> cards;
  for (CreditCard* card : personal_data_manager_->GetCreditCards())
    cards.emplace_back(card->CardIdentifierStringForAutofillDisplay(), card);

  return base::flat_map<std::u16string, CreditCard*>(std::move(cards));
}

void AutofillContextMenuManager::AppendItems() {
  content::RenderFrameHost* rfh = delegate_->GetRenderFrameHost();
  if (!rfh)
    return;

  ContentAutofillDriver* driver =
      ContentAutofillDriver::GetForRenderFrameHost(rfh);
  // Do not show autofill context menu options for input fields that cannot be
  // filled by the driver. See crbug.com/1367547.
  if (!driver || !driver->CanShowAutofillUi())
    return;

  if (params_.field_renderer_id) {
    LocalFrameToken frame_token(rfh->GetFrameToken().value());
    // Formless fields have default form renderer id.
    FormGlobalId form_global_id = {
        frame_token, params_.form_renderer_id
                         ? FormRendererId(*params_.form_renderer_id)
                         : FormRendererId()};
    driver->OnContextMenuShownInField(
        form_global_id,
        {frame_token, FieldRendererId(*params_.field_renderer_id)});
  }

  if (base::FeatureList::IsEnabled(
          features::kAutofillShowManualFallbackInContextMenu)) {
    DCHECK(personal_data_manager_);
    DCHECK(menu_model_);

    content::WebContents* web_contents = delegate_->GetWebContents();
    AutofillClient* autofill_client =
        autofill::ContentAutofillClient::FromWebContents(web_contents);
    // If the autofill popup is shown and the user double clicks from within the
    // bounds of the initiating field, it is assumed that the context menu would
    // overlap with the autofill popup. In that case, hide the autofill popup.
    if (autofill_client) {
      autofill_client->HideAutofillPopup(
          PopupHidingReason::kOverlappingWithAutofillContextMenu);
    }

    // Stores all the profile values added to the context menu along with the
    // command id of the row.
    std::vector<std::pair<CommandId, ContextMenuItem>>
        detail_items_added_to_context_menu;

    AddAddressOrCreditCardItemsToMenu(detail_items_added_to_context_menu,
                                      GetAddressProfilesWithTitles());
    AddAddressOrCreditCardItemsToMenu(detail_items_added_to_context_menu,
                                      GetCreditCardProfilesWithTitles());

    command_id_to_menu_item_value_mapper_ =
        base::flat_map<CommandId, ContextMenuItem>(
            std::move(detail_items_added_to_context_menu));
    menu_model_->AddSeparator(ui::NORMAL_SEPARATOR);
  }

  // Includes the option of submitting feedback on Autofill.
  if (base::FeatureList::IsEnabled(features::kAutofillFeedback)) {
    menu_model_->AddItemWithStringIdAndIcon(
        IDC_CONTENT_CONTEXT_AUTOFILL_FEEDBACK,
        IDS_CONTENT_CONTEXT_AUTOFILL_FEEDBACK,
        ui::ImageModel::FromVectorIcon(vector_icons::kDogfoodIcon));
    menu_model_->AddSeparator(ui::NORMAL_SEPARATOR);
  }
}

bool AutofillContextMenuManager::IsCommandIdChecked(
    CommandId command_id) const {
  return false;
}

bool AutofillContextMenuManager::IsCommandIdVisible(
    CommandId command_id) const {
  return true;
}

bool AutofillContextMenuManager::IsCommandIdEnabled(
    CommandId command_id) const {
  return true;
}

void AutofillContextMenuManager::ExecuteCommand(CommandId command_id) {
  content::RenderFrameHost* rfh = delegate_->GetRenderFrameHost();
  if (!rfh)
    return;

  DCHECK(IsAutofillCustomCommandId(command_id));

  if (command_id.value() == kAutofillContextFeedback) {
    ExecuteAutofillFeedbackCommand(rfh);
    return;
  }

  ExecuteMenuManagerCommand(command_id, rfh);
}

void AutofillContextMenuManager::ExecuteAutofillFeedbackCommand(
    content::RenderFrameHost* rfh) {
  AutofillManager* manager =
      ContentAutofillDriver::GetForRenderFrameHost(rfh)->autofill_manager();
  if (!manager) {
    return;
  }

  chrome::ShowFeedbackPage(
      browser_, chrome::kFeedbackSourceAutofillContextMenu,
      /*description_template=*/std::string(),
      /*description_placeholder_text=*/std::string(),
      /*category_tag=*/"dogfood_autofill_feedback",
      /*extra_diagnostics=*/std::string(),
      /*autofill_metadata=*/
      data_logs::FetchAutofillFeedbackData(
          manager, LoadTriggerFormAndFieldLogs(manager, rfh, params_)));
}

void AutofillContextMenuManager::ExecuteMenuManagerCommand(
    CommandId command_id,
    content::RenderFrameHost* rfh) {
  auto it = command_id_to_menu_item_value_mapper_.find(command_id);
  if (it == command_id_to_menu_item_value_mapper_.end()) {
    return;
  }

  // Field Renderer id should be present because the context menu is triggered
  // on a input field. Otherwise, Autofill context menu models would not have
  // been added to the context menu.
  if (!params_.field_renderer_id) {
    return;
  }

  if (it->second.is_manage_item) {
    DCHECK(browser_);
    switch (it->second.sub_menu_type) {
      case SubMenuType::SUB_MENU_TYPE_ADDRESS:
        chrome::ShowAddresses(browser_);
        break;
      case SubMenuType::SUB_MENU_TYPE_CREDIT_CARD:
        chrome::ShowPaymentMethods(browser_);
        break;
      case SubMenuType::SUB_MENU_TYPE_PASSWORD:
        chrome::ShowPasswordManager(browser_);
        break;
      case SubMenuType::NUM_SUBMENU_TYPES:
        [[fallthrough]];
      default:
        NOTREACHED();
    }
    return;
  }

  // TODO(crbug.com/1325811): When filling credit card number via the context
  // masked number is filled, fix it + implement reauth mechanism.
  ContentAutofillDriver* driver =
      ContentAutofillDriver::GetForRenderFrameHost(rfh);
  driver->browser_events().RendererShouldFillFieldWithValue(
      {LocalFrameToken(rfh->GetFrameToken().value()),
       FieldRendererId(params_.field_renderer_id.value())},
      it->second.fill_value);

  // TODO(crbug.com/1325811): Use `it->second.sub_menu_type` to record the usage
  // of the context menu based on the type.
}

void AutofillContextMenuManager::AddAddressOrCreditCardItemsToMenu(
    std::vector<std::pair<CommandId, ContextMenuItem>>&
        detail_items_added_to_context_menu,
    absl::variant<AddressProfilesWithTitles, CreditCardProfilesWithTitles>
        profiles) {
  bool is_address_menu =
      absl::holds_alternative<AddressProfilesWithTitles>(profiles);
  if (is_address_menu &&
      absl::get<AddressProfilesWithTitles>(profiles).empty()) {
    return;
  }

  if (!is_address_menu &&
      absl::get<CreditCardProfilesWithTitles>(profiles).empty()) {
    return;
  }

  SubMenuType sub_menu_type =
      is_address_menu ? SUB_MENU_TYPE_ADDRESS : SUB_MENU_TYPE_CREDIT_CARD;

  // Address field types that are supposed to be shown in the menu.
  static constexpr FieldsToShow kAddressFieldTypesToShow[] = {
      {ui::MenuModel::ItemType::TYPE_TITLE, NAME_FULL},
      {ui::MenuModel::ItemType::TYPE_SEPARATOR, UNKNOWN_TYPE},
      {ui::MenuModel::ItemType::TYPE_TITLE, ADDRESS_HOME_STREET_ADDRESS},
      {ui::MenuModel::ItemType::TYPE_TITLE, ADDRESS_HOME_CITY},
      {ui::MenuModel::ItemType::TYPE_TITLE, ADDRESS_HOME_ZIP},
      {ui::MenuModel::ItemType::TYPE_SEPARATOR, UNKNOWN_TYPE},
      {ui::MenuModel::ItemType::TYPE_TITLE, PHONE_HOME_WHOLE_NUMBER},
      {ui::MenuModel::ItemType::TYPE_TITLE, EMAIL_ADDRESS}};

  // Address menu of Others.
  static constexpr FieldsToShow kAddressFieldTypesToShowOtherSection[] = {
      {ui::MenuModel::ItemType::TYPE_TITLE, NAME_FIRST},
      {ui::MenuModel::ItemType::TYPE_TITLE, NAME_LAST},
      {ui::MenuModel::ItemType::TYPE_SEPARATOR, UNKNOWN_TYPE},
      {ui::MenuModel::ItemType::TYPE_TITLE, ADDRESS_HOME_LINE1},
      {ui::MenuModel::ItemType::TYPE_TITLE, ADDRESS_HOME_LINE2},
  };

  // Credit card field types that are supposed to be shown in the menu.
  static constexpr FieldsToShow kCardFieldTypesToShow[] = {
      {ui::MenuModel::ItemType::TYPE_TITLE, CREDIT_CARD_NAME_FULL},
      {ui::MenuModel::ItemType::TYPE_TITLE, CREDIT_CARD_NUMBER},
      {ui::MenuModel::ItemType::TYPE_SEPARATOR, UNKNOWN_TYPE},
      {ui::MenuModel::ItemType::TYPE_TITLE, CREDIT_CARD_EXP_MONTH},
      {ui::MenuModel::ItemType::TYPE_TITLE, CREDIT_CARD_EXP_2_DIGIT_YEAR}};

  // Used to create menu model for storing address/card description. Would be
  // attached to the top level "Fill Address Info/Fill Payment" item in the
  // context menu.
  ui::SimpleMenuModel* menu = CreateSimpleMenuModel();

  // True if a row is added in the menu.
  bool profile_added = false;

  auto field_types_to_show = is_address_menu
                                 ? base::span(kAddressFieldTypesToShow)
                                 : base::span(kCardFieldTypesToShow);
  auto field_types_to_show_in_other =
      is_address_menu ? base::span(kAddressFieldTypesToShowOtherSection)
                      : base::span<const FieldsToShow>();

  absl::visit(
      [&](const auto& addresses_or_cards) {
        for (const auto& [profile_title, profile] : addresses_or_cards) {
          if (!HaveEnoughIdsForProfile(profile, field_types_to_show,
                                       field_types_to_show_in_other)) {
            break;
          }
          AddAddressOrCreditCardItemToMenu(profile, profile_title,
                                           field_types_to_show,
                                           field_types_to_show_in_other, menu,
                                           detail_items_added_to_context_menu);
          profile_added = true;
        }
      },
      profiles);

  if (!profile_added)
    return;

  menu->AddSeparator(ui::NORMAL_SEPARATOR);
  absl::optional<CommandId> manage_item_command_id =
      GetNextAvailableAutofillCommandId();
  DCHECK(manage_item_command_id);
  // TODO(crbug.com/1325811): Use i18n string.
  menu->AddItem(
      manage_item_command_id->value(),
      is_address_menu ? u"Manage addresses" : u"Manage payment methods");
  detail_items_added_to_context_menu.emplace_back(
      *manage_item_command_id, ContextMenuItem{u"", sub_menu_type, true});

  // Add a menu option to suggest filling address/card in the context menu.
  // Hovering over it opens a submenu suggesting all the address/card profiles
  // stored in the profile.
  // TODO(crbug.com/1325811): Use i18n string.
  menu_model_->AddSubMenu(
      kAutofillContextCustomFirst + sub_menu_type,
      is_address_menu ? u"Fill Address Info" : u"Fill Payment", menu);
}

void AutofillContextMenuManager::AddAddressOrCreditCardItemToMenu(
    absl::variant<const AutofillProfile*, const CreditCard*> profile,
    const std::u16string& profile_title,
    base::span<const FieldsToShow> field_types_to_show,
    base::span<const FieldsToShow> other_fields_to_show,
    ui::SimpleMenuModel* menu,
    std::vector<std::pair<CommandId, ContextMenuItem>>&
        detail_items_added_to_context_menu) {
  bool is_address_menu =
      absl::holds_alternative<const AutofillProfile*>(profile);
  SubMenuType sub_menu_type =
      is_address_menu ? SUB_MENU_TYPE_ADDRESS : SUB_MENU_TYPE_CREDIT_CARD;

  // Creates a menu model for storing address/card details.
  // Is attached to the address/card description menu item as a submenu.
  ui::SimpleMenuModel* details_submenu = CreateSimpleMenuModel();

  // Create a submenu for each address/card profile with their details.
  AddProfileDataToMenu(profile, field_types_to_show, details_submenu,
                       detail_items_added_to_context_menu, sub_menu_type);

  // Add "Other" section for addresses.
  if (is_address_menu) {
    details_submenu->AddSeparator(ui::NORMAL_SEPARATOR);
    ui::SimpleMenuModel* other_menu_model = CreateSimpleMenuModel();
    AddProfileDataToMenu(profile, other_fields_to_show, other_menu_model,
                         detail_items_added_to_context_menu, sub_menu_type);

    absl::optional<CommandId> others_menu_id =
        GetNextAvailableAutofillCommandId();
    DCHECK(others_menu_id);

    // TODO(crbug.com/1325811): Use i18n string.
    details_submenu->AddSubMenu(others_menu_id->value(), u"Other",
                                other_menu_model);
  }

  // Add a menu item showing address/card profile description. Hovering
  // over it opens a submenu with the address/card details.
  absl::optional<CommandId> menu_id = GetNextAvailableAutofillCommandId();
  if (menu_id)
    menu->AddSubMenu(menu_id->value(), profile_title, details_submenu);
}

void AutofillContextMenuManager::AddProfileDataToMenu(
    absl::variant<const AutofillProfile*, const CreditCard*>
        profile_or_credit_card,
    base::span<const FieldsToShow> field_types_to_show,
    ui::SimpleMenuModel* menu_model,
    std::vector<std::pair<CommandId, ContextMenuItem>>&
        detail_items_added_to_context_menu,
    SubMenuType sub_menu_type) {
  std::vector<std::pair<ui::MenuModel::ItemType, std::u16string>>
      items_to_add_to_menu;
  items_to_add_to_menu.reserve(field_types_to_show.size());

  // True if the separator needs to be shown.
  bool is_separator_required = false;

  // Iterate over the `field_types_to_show` from the back and check if there is
  // a need for a separator. We do not want to show consecutive separators if
  // the data does not exist in the profile.
  for (const auto& [item_type, field_type] :
       base::Reversed(field_types_to_show)) {
    if (item_type == ui::MenuModel::ItemType::TYPE_SEPARATOR) {
      if (is_separator_required)
        items_to_add_to_menu.emplace_back(item_type, u"");
      is_separator_required = false;
      continue;
    }

    std::u16string value = absl::visit(
        base::Overloaded{
            [&type = field_type](const CreditCard* card) {
              if (type == CREDIT_CARD_NUMBER) {
                return card->ObfuscatedNumberWithVisibleLastFourDigits();
              }
              return card->GetRawInfo(type);
            },
            [&type = field_type](const AutofillProfile* profile) {
              return profile->GetRawInfo(type);
            }},
        profile_or_credit_card);

    if (value.empty())
      continue;

    items_to_add_to_menu.emplace_back(item_type, value);
    is_separator_required = true;
  }

  // Iterate in reverse again and add items to the menu model.
  for (const auto& [item_type, value] : base::Reversed(items_to_add_to_menu)) {
    if (item_type == ui::MenuModel::ItemType::TYPE_SEPARATOR) {
      menu_model->AddSeparator(ui::NORMAL_SEPARATOR);
    } else {
      absl::optional<CommandId> value_menu_id =
          GetNextAvailableAutofillCommandId();
      DCHECK(value_menu_id);

      // Create a menu item with the address/credit card details and attach
      // to the model.
      menu_model->AddItem(value_menu_id->value(), value);
      detail_items_added_to_context_menu.emplace_back(
          *value_menu_id, ContextMenuItem{value, sub_menu_type});
    }
  }
}

bool AutofillContextMenuManager::HaveEnoughIdsForProfile(
    absl::variant<const AutofillProfile*, const CreditCard*>
        profile_or_credit_card,
    base::span<const FieldsToShow> field_types_to_show,
    base::span<const FieldsToShow> other_fields_to_show) {
  // Count of items to be added to the context menu. Empty values are not
  // considered.
  auto non_empty_values_in_profile = [&](const auto& entry) {
    ServerFieldType field_type = entry.field_type;
    return field_type != UNKNOWN_TYPE &&
           !absl::visit(
                [field_type](const auto& alternative) {
                  return alternative->GetRawInfo(field_type);
                },
                profile_or_credit_card)
                .empty();
  };
  int count_of_items_to_be_added =
      base::ranges::count_if(field_types_to_show, non_empty_values_in_profile);
  // For addresses, include the "Other" section in the count. For credit cards,
  // this should be empty.
  if (absl::holds_alternative<const CreditCard*>(profile_or_credit_card)) {
    DCHECK(other_fields_to_show.empty());
  }
  count_of_items_to_be_added +=
      base::ranges::count_if(other_fields_to_show, non_empty_values_in_profile);

  // Check if there are enough command ids for adding all the items to the
  // context menu.
  // 1 is added to count for the address/credit card description.
  // Another 1 is added to account for the manage addresses/payment methods
  // option.
  if (!IsAutofillCustomCommandId(CommandId(
          kAutofillContextCustomFirst + SubMenuType::NUM_SUBMENU_TYPES +
          count_of_items_added_to_menu_model_ + count_of_items_to_be_added + 1 +
          1))) {
    return false;
  }

  return true;
}

ui::SimpleMenuModel* AutofillContextMenuManager::CreateSimpleMenuModel() {
  cached_menu_models_.push_back(
      std::make_unique<ui::SimpleMenuModel>(delegate_));
  return cached_menu_models_.back().get();
}

absl::optional<AutofillContextMenuManager::CommandId>
AutofillContextMenuManager::GetNextAvailableAutofillCommandId() {
  int max_index = kAutofillContextCustomLast - kAutofillContextCustomFirst;

  if (count_of_items_added_to_menu_model_ >= max_index)
    return absl::nullopt;

  return ConvertToAutofillCustomCommandId(
      count_of_items_added_to_menu_model_++);
}

std::u16string AutofillContextMenuManager::GetProfileDescription(
    const AutofillProfile& profile) {
  // All user-visible fields.
  static constexpr ServerFieldType kDetailsFields[] = {
      NAME_FULL,
      ADDRESS_HOME_LINE1,
      ADDRESS_HOME_LINE2,
      ADDRESS_HOME_DEPENDENT_LOCALITY,
      ADDRESS_HOME_CITY,
      ADDRESS_HOME_STATE,
      ADDRESS_HOME_ZIP,
      EMAIL_ADDRESS,
      PHONE_HOME_WHOLE_NUMBER,
      COMPANY_NAME,
      ADDRESS_HOME_COUNTRY};

  return profile.ConstructInferredLabel(
      kDetailsFields, std::size(kDetailsFields),
      /*num_fields_to_include=*/2, personal_data_manager_->app_locale());
}

}  // namespace autofill
