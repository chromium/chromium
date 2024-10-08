// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webauthn/context_menu_helper.h"

#include "base/feature_list.h"
#include "chrome/browser/password_manager/chrome_webauthn_credentials_delegate_factory.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "device/fido/features.h"

namespace webauthn {

namespace {

password_manager::WebAuthnCredentialsDelegate* GetWebAuthnCredentialsDelegate(
    content::RenderFrameHost* render_frame_host) {
  return ChromeWebAuthnCredentialsDelegateFactory::GetFactory(
             content::WebContents::FromRenderFrameHost(render_frame_host))
      ->GetDelegateForFrame(render_frame_host);
}

}  // namespace

bool IsPasskeyFromAnotherDeviceContextMenuEnabled(
    content::RenderFrameHost* render_frame_host,
    uint64_t form_renderer_id,
    uint64_t field_renderer_id) {
  auto* af_driver =
      autofill::ContentAutofillDriver::GetForRenderFrameHost(render_frame_host);
  if (!af_driver) {
    return false;
  }

  const autofill::LocalFrameToken frame_token = af_driver->GetFrameToken();
  autofill::FormStructure* form =
      af_driver->GetAutofillManager().FindCachedFormById(
          {frame_token, autofill::FormRendererId(form_renderer_id)});
  if (!form) {
    return false;
  }

  // If the field does not have autocomplete="webauthn", the entry is disabled:
  auto* field = form->GetFieldById(
      {frame_token, autofill::FieldRendererId(field_renderer_id)});
  if (!field || !field->parsed_autocomplete()
                     .value_or(autofill::AutocompleteParsingResult())
                     .webauthn) {
    return false;
  }

  // Enable the entry if a conditional request is made and the security key or
  // hybrid flow is available:
  auto* webauthn_delegate = GetWebAuthnCredentialsDelegate(render_frame_host);
  return webauthn_delegate && webauthn_delegate->GetPasskeys().has_value() &&
         webauthn_delegate->IsSecurityKeyOrHybridFlowAvailable();
}

void OnPasskeyFromAnotherDeviceContextMenuItemSelected(
    content::RenderFrameHost* render_frame_host) {
  auto* delegate = GetWebAuthnCredentialsDelegate(render_frame_host);
  if (!delegate) {
    return;
  }
  delegate->LaunchSecurityKeyOrHybridFlow();
}

}  // namespace webauthn
