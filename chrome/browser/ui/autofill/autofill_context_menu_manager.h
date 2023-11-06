// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_CONTEXT_MENU_MANAGER_H_
#define CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_CONTEXT_MENU_MANAGER_H_

#include "base/memory/raw_ptr.h"
#include "base/types/strong_alias.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/renderer_context_menu/render_view_context_menu_base.h"
#include "components/renderer_context_menu/render_view_context_menu_observer.h"
#include "content/public/browser/context_menu_params.h"
#include "ui/base/models/simple_menu_model.h"

namespace autofill {

class AutofillField;
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

  // Returns true if the given id is one generated for autofill context menu.
  static bool IsAutofillCustomCommandId(CommandId command_id);

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
  // Triggers the feedback flow for Autofill command.
  void ExecuteAutofillFeedbackCommand(const LocalFrameToken& frame_token,
                                      AutofillManager& manager);

  // Conditionally adds the address and / or payments Autofill manual fallbacks
  // to the context menu model depending on whether there's data to suggest
  // and corresponding feature flags are enabled.
  void MaybeAddAutofillManualFallbackItems(ContentAutofillDriver& driver);

  // Checks if the manual fallback context menu entry can be shown for the
  // currently focused field.
  bool ShouldAddAddressManualFallbackItem(ContentAutofillDriver& driver);

  // Checks if the currently focused field has unrecognized autocomplete but is
  // classified and can be filled with user address data.
  bool ShouldAddAddressManualFallbackForAutocompleteUnrecognized(
      ContentAutofillDriver& driver);

  // Emits metrics about showing the manual fallback context menu entry to the
  // user.
  void LogManualFallbackContextMenuEntryShown(ContentAutofillDriver& driver);

  // Triggers Autofill suggestions on the field that the context menu was
  // opened on.
  void ExecuteFallbackForAutocompleteUnrecognizedCommand(
      AutofillManager& manager);

  // Gets the `AutofillField` described by the `params_` from the `manager`.
  // The `frame_token` is used to map from the `params_` renderer id to a global
  // id.
  AutofillField* GetAutofillField(AutofillManager& manager,
                                  const LocalFrameToken& frame_token) const;

  const raw_ptr<PersonalDataManager> personal_data_manager_;
  const raw_ptr<ui::SimpleMenuModel> menu_model_;
  const raw_ptr<RenderViewContextMenuBase> delegate_;
  content::ContextMenuParams params_;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_CONTEXT_MENU_MANAGER_H_
