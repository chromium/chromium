// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_CONTEXT_MENU_MANAGER_H_
#define CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_CONTEXT_MENU_MANAGER_H_

#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "base/types/strong_alias.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/renderer_context_menu/render_view_context_menu_observer.h"
#include "content/public/browser/context_menu_params.h"
#include "ui/menus/simple_menu_model.h"

class RenderViewContextMenuBase;

namespace password_manager {
class ContentPasswordManagerDriver;
}  // namespace password_manager

namespace autofill {

class AutofillDriver;
class AutofillManager;
class ContentAutofillDriver;

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

  AutofillContextMenuManager(RenderViewContextMenuBase* delegate,
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

  // Conditionally adds the address, payments and / or passwords Autofill manual
  // fallbacks to the context menu model depending on whether there's data to
  // suggest.
  void MaybeAddAutofillManualFallbackItems();

  // Checks if the plus address context menu entry can be shown for the
  // currently focused field.
  bool ShouldAddPlusAddressManualFallbackItem(
      ContentAutofillDriver& autofill_driver);

  // Checks if the currently focused field is a password field and whether
  // password filling is enabled.
  bool ShouldAddPasswordsManualFallbackItem(
      password_manager::ContentPasswordManagerDriver& password_manager_driver);

  // Adds the passwords manual fallback context menu entries.
  //
  // The entries are displayed in the following order:
  // - "Select password" iff the user has passwords saved. This entry triggers
  // password suggestions.
  // - "Suggest password..." iff the user can generate passwords for the current
  // field.
  // - "Use passkey from another device" iff the field suppors passkeys.
  // - "Import passwords" iff the user does not have password saves. This entry
  // opens chrome://password-manager.
  //
  // Not all 4 entries have to be displayed. If an entry does not meet its
  // criterion to be displayed, the entry will be skipped.
  void AddPasswordsManualFallbackItems(
      password_manager::ContentPasswordManagerDriver& password_manager_driver,
      bool add_select_password_option);

  // Out of all password entries, this method is only interested in the "select
  // password" entry, because the rest of them don't trigger suggestions and are
  // recorded by default separately (outside `AutofillContextMenuManager`).
  void LogSelectPasswordManualFallbackContextMenuEntryShown(
      password_manager::ContentPasswordManagerDriver& password_manager_drivern);

  void LogSelectPasswordManualFallbackContextMenuEntryAccepted();

  // Triggers the feedback flow for Autofill command.
  void ExecuteAutofillFeedbackCommand(const LocalFrameToken& frame_token,
                                      AutofillManager& manager);

  // Triggers Plus Address suggestions on the field that the context menu was
  // opened on.
  void ExecuteFallbackForPlusAddressesCommand(AutofillDriver& driver);

  // Triggers passwords suggestions on the field that the context menu was
  // opened on.
  void ExecuteFallbackForSelectPasswordCommand(AutofillDriver& driver);

  // Marks the last added menu item as a new feature, depending on the response
  // from the `UserEducationService`.
  void MaybeMarkLastItemAsNewFeature(const base::Feature& feature);

  const raw_ptr<ui::SimpleMenuModel> menu_model_;
  const raw_ptr<RenderViewContextMenuBase> delegate_;
  content::ContextMenuParams params_;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_CONTEXT_MENU_MANAGER_H_
