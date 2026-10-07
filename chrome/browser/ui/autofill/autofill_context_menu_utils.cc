// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/autofill_context_menu_utils.h"

#include "base/feature_list.h"
#include "base/notreached.h"
#include "chrome/browser/password_manager/factories/password_counter_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/autofill/content/browser/content_autofill_client.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/core/browser/at_memory/at_memory_enablement_util.h"
#include "components/autofill/core/browser/foundations/autofill_driver.h"
#include "components/autofill/core/common/aliases.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/password_manager/content/browser/content_password_manager_driver.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/browser/password_counter.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "content/public/browser/context_menu_params.h"
#include "content/public/browser/render_frame_host.h"
#include "third_party/blink/public/mojom/forms/form_control_type.mojom-shared.h"

namespace autofill {

bool ShouldShowAutofillContextMenu(const content::ContextMenuParams& params) {
  if (params.is_content_editable_for_autofill) {
    return true;
  }
  if (!params.form_control_type) {
    return false;
  }
  switch (*params.form_control_type) {
    case blink::mojom::FormControlType::kInputEmail:
    case blink::mojom::FormControlType::kInputMonth:
    case blink::mojom::FormControlType::kInputNumber:
    case blink::mojom::FormControlType::kInputPassword:
    case blink::mojom::FormControlType::kInputSearch:
    case blink::mojom::FormControlType::kInputTelephone:
    case blink::mojom::FormControlType::kInputText:
    case blink::mojom::FormControlType::kInputUrl:
    case blink::mojom::FormControlType::kTextArea:
      return true;
    case blink::mojom::FormControlType::kButtonButton:
    case blink::mojom::FormControlType::kButtonSubmit:
    case blink::mojom::FormControlType::kButtonReset:
    case blink::mojom::FormControlType::kButtonPopover:
    case blink::mojom::FormControlType::kFieldset:
    case blink::mojom::FormControlType::kInputButton:
    case blink::mojom::FormControlType::kInputCheckbox:
    case blink::mojom::FormControlType::kInputColor:
    case blink::mojom::FormControlType::kInputDate:
    case blink::mojom::FormControlType::kInputDatetimeLocal:
    case blink::mojom::FormControlType::kInputFile:
    case blink::mojom::FormControlType::kInputHidden:
    case blink::mojom::FormControlType::kInputImage:
    case blink::mojom::FormControlType::kInputRadio:
    case blink::mojom::FormControlType::kInputRange:
    case blink::mojom::FormControlType::kInputReset:
    case blink::mojom::FormControlType::kInputSubmit:
    case blink::mojom::FormControlType::kInputTime:
    case blink::mojom::FormControlType::kInputWeek:
    case blink::mojom::FormControlType::kOutput:
    case blink::mojom::FormControlType::kSelectOne:
    case blink::mojom::FormControlType::kSelectMultiple:
      return false;
  }
  NOTREACHED();
}

bool ShouldShowAtMemoryContextMenuItem(
    content::RenderFrameHost& rfh,
    const content::ContextMenuParams& params) {
  if (params.form_control_type ==
      blink::mojom::FormControlType::kInputPassword) {
    return false;
  }

  if (!ShouldShowAutofillContextMenu(params)) {
    return false;
  }

  ContentAutofillDriver* autofill_driver =
      ContentAutofillDriver::GetForRenderFrameHost(&rfh);
  if (!autofill_driver || !autofill_driver->CanShowAutofillUi()) {
    return false;
  }

  return MayPerformAtMemoryAction(AtMemoryAction::kTriggerSearchUI,
                                  autofill_driver->GetAutofillClient(),
                                  params.page_url) &&
         MayPerformAtMemoryAction(AtMemoryAction::kTriggerSearchUI,
                                  autofill_driver->GetAutofillClient(),
                                  params.frame_url);
}

void ExecuteAtMemoryContextMenuCommand(
    content::RenderFrameHost& rfh,
    const content::ContextMenuParams& params) {
  AutofillDriver* autofill_driver =
      ContentAutofillDriver::GetForRenderFrameHost(&rfh);
  if (!autofill_driver) {
    return;
  }
  autofill_driver->RendererShouldTriggerSuggestions(
      {autofill_driver->GetFrameToken(),
       FieldRendererId(params.field_renderer_id.value())},
      AutofillSuggestionTriggerSource::kAtMemoryContextMenu);
}

bool ShouldAddPasswordsManualFallbackItem(
    content::RenderFrameHost& rfh,
    const content::ContextMenuParams& params) {
  if (!ShouldShowAutofillContextMenu(params) ||
      params.is_content_editable_for_autofill ||
      params.form_control_type == blink::mojom::FormControlType::kTextArea) {
    return false;
  }

  auto* driver =
      password_manager::ContentPasswordManagerDriver::GetForRenderFrameHost(
          &rfh);
  if (!driver || !driver->CanShowAutofillUi()) {
    return false;
  }

  if (base::FeatureList::IsEnabled(
          password_manager::features::kPasswordManualFallbackSecurityChecks) &&
      (!driver->HasValidURL(/*may_kill_renderer=*/false) ||
       !driver->IsRenderFrameHostSupported())) {
    return false;
  }

  return driver->GetPasswordManager()->GetClient()->IsFillingEnabled(
      driver->GetLastCommittedOrigin(), driver->GetLastCommittedURL());
}

bool ShouldShowSelectPasswordContextMenuItem(
    content::RenderFrameHost& rfh,
    const content::ContextMenuParams& params) {
  if (!ShouldAddPasswordsManualFallbackItem(rfh, params)) {
    return false;
  }

  Profile* profile = Profile::FromBrowserContext(rfh.GetBrowserContext());
  password_manager::PasswordCounter* counter =
      PasswordCounterFactory::GetForProfile(profile);
  return counter && counter->autofillable_passwords() > 0;
}

}  // namespace autofill
