// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_CONTEXT_MENU_MANAGER_H_
#define CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_CONTEXT_MENU_MANAGER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/types/strong_alias.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/renderer_context_menu/render_view_context_menu_base.h"
#include "components/renderer_context_menu/render_view_context_menu_observer.h"
#include "content/public/browser/context_menu_params.h"
#include "ui/base/models/simple_menu_model.h"

namespace password_manager {
class ContentPasswordManagerDriver;
}  // namespace password_manager

namespace autofill {

class AutofillField;
class AutofillPredictionImprovementsDelegate;
class PersonalDataManager;

// `AutofillContextMenuManager` is responsible for adding/executing Autofill
// related context menu items. `RenderViewContextMenu` is intended to own and
// control the lifetime of `AutofillContextMenuManager`.
// The options include:
// Provide Autofill feedback
// Fill in Form
class AutofillContextMenuManager : public RenderViewContextMenuObserver {
 public:
  // Represents command id used to denote a row in the context menu. The
  // command ids are created when the items are added to the context menu during
  // it's initialization.
  using CommandId = base::StrongAlias<class CommandIdTag, int>;

  AutofillContextMenuManager(PersonalDataManager* personal_data_manager,
                             RenderViewContextMenuBase* delegate,
                             ui::SimpleMenuModel* menu_model);
  ~AutofillContextMenuManager() override;
  AutofillContextMenuManager(const AutofillContextMenuManager&) = delete;
  AutofillContextMenuManager& operator=(const AutofillContextMenuManager&) =
      delete;

  // Adds items to the context menu.
  // Note: This doesn't use `RenderViewContextMenuObserver::InitMenu()`, since
  // Autofill context menu entries are conditioned on
  // `ContextMenuContentType::ITEM_GROUP_AUTOFILL`.
  void AppendItems();

  // `RenderViewContextMenuObserver` overrides.
  bool IsCommandIdSupported(int command_id) override;
  bool IsCommandIdEnabled(int command_id) override;
  void ExecuteCommand(int command_id) override;

  // Setter for `params_` used for testing purposes.
  void set_params_for_testing(content::ContextMenuParams params) {
    params_ = params;
  }

 private:
  // Conditionally adds the feedback manual fallback item if Autofill is
  // available for the field.
  void MaybeAddAutofillFeedbackItem();

  // Conditionally adds the item to trigger filling with prediction
  // improvements.
  void MaybeAddAutofillPredictionImprovementsItem();

  // Conditionally adds the address, payments and / or passwords Autofill manual
  // fallbacks to the context menu model depending on whether there's data to
  // suggest.
  void MaybeAddAutofillManualFallbackItems();

  // Checks if the plus address context menu entry can be shown for the
  // currently focused field.
  bool ShouldAddPlusAddressManualFallbackItem(
      ContentAutofillDriver& autofill_driver);

  // Returns if the item to trigger prediction improvements should be added.
  bool ShouldAddPredictionImprovementsItem(
      AutofillPredictionImprovementsDelegate* delegate,
      const GURL& url);

  // Checks if the manual fallback context menu entry can be shown for the
  // currently focused field.
  bool ShouldAddAddressManualFallbackItem(
      ContentAutofillDriver& autofill_driver);

  // Checks if the currently focused field is a password field and whether
  // password filling is enabled.
  bool ShouldAddPasswordsManualFallbackItem(
      password_manager::ContentPasswordManagerDriver& password_manager_driver);

  // Adds the passwords manual fallback context menu entries.
  //
  // Regardless of the state of the user, only one entry is displayed in the
  // top-level context menu: "Passwords".
  //
  // If the user has passwords saved and cannot generate passwords, clicking on
  // the "Passwords" entry behaves exactly like "Select password" (it will
  // trigger password suggestions).
  //
  // In all the other cases, the "Passwords" entry doesn't do anything upon
  // clicking, but hovering on it opens a sub-menu.
  //
  // In the sub-menu, if the user doesn't have passwords saved, the first entry
  // is "No saved passwords". This entry is greyed out and doesn't do anything
  // upon clicking. It is just informative. If the user has passwords saved,
  // this entry is missing.
  //
  // The next entry in the sub-menu is either "Select password" (which triggers
  // password suggestions) or "Import passwords" (which opens
  // chrome://password-manager), depending on whether the user has passwords
  // saved or not.
  //
  // If the user can also generate passwords for the current field, the final
  // entry is "Suggest password...". Otherwise, this entry is missing.
  void AddPasswordsManualFallbackItems(
      password_manager::ContentPasswordManagerDriver& password_manager_driver);

  void LogAddressManualFallbackContextMenuEntryShown(
      ContentAutofillDriver& autofill_driver);

  void LogPaymentsManualFallbackContextMenuEntryShown(
      ContentAutofillDriver& autofill_driver);

  // Out of all password entries, this method is only interested in the "select
  // password" entry, because the rest of them don't trigger suggestions and are
  // recorded by default separately (outside `AutofillContextMenuManager`).
  void LogSelectPasswordManualFallbackContextMenuEntryShown(
      password_manager::ContentPasswordManagerDriver& password_manager_drivern);

  void LogAddressManualFallbackContextMenuEntryAccepted(
      AutofillDriver& autofill_driver);

  void LogPaymentsManualFallbackContextMenuEntryAccepted(
      AutofillDriver& autofill_driver);

  void LogSelectPasswordManualFallbackContextMenuEntryAccepted();

  // Triggers the filling with prediction improvements flow.
  void ExecutePredictionImprovementsCommand(
      const LocalFrameToken& frame_token,
      ContentAutofillDriver& autofill_driver);

  // Triggers the feedback flow for Autofill command.
  void ExecuteAutofillFeedbackCommand(const LocalFrameToken& frame_token,
                                      AutofillManager& manager);

  // Triggers Plus Address suggestions on the field that the context menu was
  // opened on.
  void ExecuteFallbackForPlusAddressesCommand(AutofillDriver& driver);

  // Triggers Autofill payments suggestions on the field that the context menu
  // was opened on.
  void ExecuteFallbackForPaymentsCommand(AutofillDriver& driver);

  // Triggers passwords suggestions on the field that the context menu was
  // opened on.
  void ExecuteFallbackForSelectPasswordCommand(AutofillDriver& driver);

  // Triggers Autofill address suggestions on the field that the context menu
  // was opened on.
  void ExecuteFallbackForAddressesCommand(
      ContentAutofillDriver& autofill_driver);

  // Marks the last added menu item as a new feature, depending on the response
  // from the `UserEducationService`.
  void MaybeMarkLastItemAsNewFeature(const base::Feature& feature);

  // Gets the `AutofillField` described by the `params_` from the
  // `autofill_driver`'s manager.
  AutofillField* GetAutofillField(AutofillDriver& autofill_driver) const;

  // Dangling on linux-lacros-rel in:
  // AutofillContextMenuManagerFeedbackUILacrosBrowserTest
  //   .CloseTabWhileUIIsOpenShouldNotCrash.
  const raw_ptr<PersonalDataManager, DanglingUntriaged> personal_data_manager_;
  const raw_ptr<ui::SimpleMenuModel> menu_model_;
  const raw_ptr<RenderViewContextMenuBase> delegate_;
  ui::SimpleMenuModel passwords_submenu_model_;
  content::ContextMenuParams params_;

  base::WeakPtrFactory<AutofillContextMenuManager> weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_CONTEXT_MENU_MANAGER_H_
