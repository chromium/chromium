// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/email_verifier/email_verification_controller_android.h"

#include <memory>
#include <utility>

#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/android/autofill/email_verification_bottom_sheet_bridge.h"
#include "chrome/browser/ui/autofill/autofill_message_controller.h"
#include "chrome/browser/ui/autofill/autofill_message_model.h"
#include "chrome/browser/ui/autofill/chrome_autofill_client.h"
#include "components/autofill/content/browser/content_autofill_client.h"
#include "components/autofill/core/browser/suggestions/suggestion_type.h"
#include "components/messages/android/message_enums.h"
#include "content/public/browser/web_contents.h"
#include "net/base/schemeful_site.h"

namespace autofill {

EmailVerificationControllerAndroid::EmailVerificationControllerAndroid(
    content::WebContents* web_contents)
    : EmailVerificationController(web_contents),
      open_settings_callback_(base::BindRepeating(
          [](base::WeakPtr<content::WebContents> web_contents) {
            if (web_contents) {
              if (ContentAutofillClient* client =
                      ContentAutofillClient::FromWebContents(
                          web_contents.get())) {
                client->ShowAutofillSettings(SuggestionType::kManageAddress);
              }
            }
          },
          web_contents->GetWeakPtr())),
      bottom_sheet_bridge_(
          std::make_unique<EmailVerificationBottomSheetBridge>(web_contents)) {}

EmailVerificationControllerAndroid::~EmailVerificationControllerAndroid() =
    default;

void EmailVerificationControllerAndroid::HideLoadingToastImpl() {
  // `Dismiss()` dismisses all active messages on `GetMessageController()`.
  // This is safe because `HideLoadingToastImpl()` is only invoked when a
  // subsequent-run loading message is active, and Android Messages shows at
  // most one web-contents-scoped Autofill banner at a time.
  GetMessageController().Dismiss(messages::DismissReason::DISMISSED_BY_FEATURE);
}

void EmailVerificationControllerAndroid::ShowPopupImpl(
    const gfx::RectF& /*element_bounds*/,
    const url::Origin& issuer_origin,
    const std::u16string& email,
    base::OnceCallback<void(
        AutofillClient::EmailVerificationPermissionUiStatus)> decision_callback,
    base::OnceClosure on_dismissed) {
  // `EmailVerificationBottomSheetBridge` takes separate `on_accepted` and
  // `on_dismissed` callbacks. When the user accepts the prompt, `on_accepted`
  // fires immediately (transitioning the sheet to its loading spinner) and
  // `on_dismissed` fires later when the sheet closes. Share `decision_callback`
  // via `RefCountedData` so whichever event occurs first delivers the initial
  // permission decision to `EmailVerificationController`.
  auto shared_decision_callback =
      base::MakeRefCounted<base::RefCountedData<base::OnceCallback<void(
          AutofillClient::EmailVerificationPermissionUiStatus)>>>(
          std::move(decision_callback));

  auto on_accepted = base::BindOnce(
      [](scoped_refptr<base::RefCountedData<base::OnceCallback<void(
             AutofillClient::EmailVerificationPermissionUiStatus)>>> cb) {
        if (cb->data) {
          std::move(cb->data).Run(
              AutofillClient::EmailVerificationPermissionUiStatus::kAllowed);
        }
      },
      shared_decision_callback);

  auto on_dismissed_callback = base::BindOnce(
      [](base::OnceClosure on_prompt_dismissed,
         scoped_refptr<base::RefCountedData<base::OnceCallback<void(
             AutofillClient::EmailVerificationPermissionUiStatus)>>> cb,
         AutofillClient::EmailVerificationPermissionUiStatus reason) {
        if (cb->data) {
          std::move(cb->data).Run(reason);
        } else if (on_prompt_dismissed) {
          // If the user already accepted the prompt (`cb->data` was consumed by
          // `on_accepted`), a subsequent dismissal means the loading bottom
          // sheet was closed. For programmatic hides, `DismissActiveUi()`
          // already reset `loading_start_time_`, making `OnPromptDismissed` a
          // no-op.
          std::move(on_prompt_dismissed).Run();
        }
      },
      std::move(on_dismissed), shared_decision_callback);

  bottom_sheet_bridge_->RequestShowContent(
      base::UTF8ToUTF16(net::SchemefulSite(issuer_origin).Serialize()), email,
      std::move(on_accepted), std::move(on_dismissed_callback));
}

void EmailVerificationControllerAndroid::HidePopupImpl() {
  bottom_sheet_bridge_->Hide();
}

void EmailVerificationControllerAndroid::ShowLoadingToastImpl(
    base::OnceClosure on_cancelled) {
  if (!IsWebContentsVisible()) {
    return;
  }
  GetMessageController().Show(
      AutofillMessageModel::CreateForEmailVerificationLoading(base::BindOnce(
          [](base::OnceClosure on_cancelled, messages::DismissReason reason) {
            // `DISMISSED_BY_FEATURE` is programmatic replacement by a
            // subsequent EVP UI state. Any other dismissal (e.g. the user
            // swiping the message away) cancels the verification.
            if (reason != messages::DismissReason::DISMISSED_BY_FEATURE) {
              std::move(on_cancelled).Run();
            }
          },
          std::move(on_cancelled))));
}

void EmailVerificationControllerAndroid::ShowErrorToastImpl() {
  if (!IsWebContentsVisible()) {
    return;
  }
  GetMessageController().Show(
      AutofillMessageModel::CreateForEmailVerificationError());
}

void EmailVerificationControllerAndroid::ShowVerifiedToastImpl(
    const GURL& url) {
  if (!IsWebContentsVisible()) {
    return;
  }
  GetMessageController().Show(AutofillMessageModel::CreateForEmailVerified(
      url, open_settings_callback_));
}

AutofillMessageController&
EmailVerificationControllerAndroid::GetMessageController() {
  if (message_controller_for_testing_) {
    return *message_controller_for_testing_;
  }
  return CHECK_DEREF(
      static_cast<ChromeAutofillClient&>(
          CHECK_DEREF(ContentAutofillClient::FromWebContents(&web_contents())))
          .GetAutofillMessageController());
}

base::WeakPtr<EmailVerificationController>
EmailVerificationControllerAndroid::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace autofill
