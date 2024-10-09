// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/webid/account_selection_view_android.h"

#include <memory>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/ui/webid/account_selection_view.h"
#include "content/public/browser/identity_request_dialog_controller.h"
#include "third_party/blink/public/mojom/webid/federated_auth_request.mojom-shared.h"
#include "third_party/blink/public/mojom/webid/federated_auth_request.mojom.h"
#include "ui/android/color_utils_android.h"
#include "ui/android/window_android.h"
#include "ui/gfx/android/java_bitmap.h"
#include "url/android/gurl_android.h"
#include "url/gurl.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/ui/android/webid/internal/jni/AccountSelectionBridge_jni.h"
#include "chrome/browser/ui/android/webid/jni_headers/Account_jni.h"
#include "chrome/browser/ui/android/webid/jni_headers/ClientIdMetadata_jni.h"
#include "chrome/browser/ui/android/webid/jni_headers/IdentityCredentialTokenError_jni.h"
#include "chrome/browser/ui/android/webid/jni_headers/IdentityProviderData_jni.h"
#include "chrome/browser/ui/android/webid/jni_headers/IdentityProviderMetadata_jni.h"

using base::android::AppendJavaStringArrayToStringVector;
using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;
using DismissReason = content::IdentityRequestDialogController::DismissReason;

namespace {

ScopedJavaLocalRef<jobject> ConvertToJavaAccount(
    JNIEnv* env,
    content::IdentityRequestAccount* account) {
  ScopedJavaLocalRef<jobject> decoded_picture = nullptr;
  if (!account->decoded_picture.IsEmpty()) {
    decoded_picture =
        gfx::ConvertToJavaBitmap(*account->decoded_picture.ToSkBitmap());
  }
  return Java_Account_Constructor(
      env, ConvertUTF8ToJavaString(env, account->id),
      ConvertUTF8ToJavaString(env, account->email),
      ConvertUTF8ToJavaString(env, account->name),
      ConvertUTF8ToJavaString(env, account->given_name),
      url::GURLAndroid::FromNativeGURL(env, account->picture), decoded_picture,
      account->login_state == Account::LoginState::kSignIn,
      account->browser_trusted_login_state == Account::LoginState::kSignIn);
}

ScopedJavaLocalRef<jobject> ConvertToJavaIdentityProviderMetadata(
    JNIEnv* env,
    const content::IdentityProviderMetadata& metadata) {
  ScopedJavaLocalRef<jstring> java_brand_icon_url =
      base::android::ConvertUTF8ToJavaString(env,
                                             metadata.brand_icon_url.spec());
  return Java_IdentityProviderMetadata_Constructor(
      env, ui::OptionalSkColorToJavaColor(metadata.brand_text_color),
      ui::OptionalSkColorToJavaColor(metadata.brand_background_color),
      java_brand_icon_url,
      url::GURLAndroid::FromNativeGURL(env, metadata.config_url),
      url::GURLAndroid::FromNativeGURL(env, metadata.idp_login_url),
      metadata.supports_add_account);
}

ScopedJavaLocalRef<jobject> ConvertToJavaIdentityCredentialTokenError(
    JNIEnv* env,
    const std::optional<TokenError>& error) {
  return Java_IdentityCredentialTokenError_Constructor(
      env,
      base::android::ConvertUTF8ToJavaString(env, error ? error->code : ""),
      url::GURLAndroid::FromNativeGURL(env, error ? error->url : GURL()));
}

ScopedJavaLocalRef<jobject> ConvertToJavaClientIdMetadata(
    JNIEnv* env,
    const content::ClientMetadata& metadata) {
  ScopedJavaLocalRef<jstring> java_brand_icon_url =
      base::android::ConvertUTF8ToJavaString(env,
                                             metadata.brand_icon_url.spec());
  return Java_ClientIdMetadata_Constructor(
      env, url::GURLAndroid::FromNativeGURL(env, metadata.terms_of_service_url),
      url::GURLAndroid::FromNativeGURL(env, metadata.privacy_policy_url),
      java_brand_icon_url);
}

ScopedJavaLocalRef<jobjectArray> ConvertToJavaAccounts(
    JNIEnv* env,
    const std::vector<IdentityRequestAccountPtr>& accounts) {
  ScopedJavaLocalRef<jclass> account_clazz = base::android::GetClass(
      env, "org/chromium/chrome/browser/ui/android/webid/data/Account");
  ScopedJavaLocalRef<jobjectArray> array(
      env, env->NewObjectArray(accounts.size(), account_clazz.obj(), nullptr));

  base::android::CheckException(env);

  for (size_t i = 0; i < accounts.size(); ++i) {
    ScopedJavaLocalRef<jobject> item =
        ConvertToJavaAccount(env, accounts[i].get());
    env->SetObjectArrayElement(array.obj(), i, item.obj());
  }
  return array;
}

inline ScopedJavaLocalRef<jintArray> ConvertFieldsToJavaArray(
    JNIEnv* env,
    const std::vector<content::IdentityRequestDialogDisclosureField>& fields) {
  std::vector<int> int_array;
  for (auto field : fields) {
    int_array.push_back(static_cast<int>(field));
  }
  return base::android::ToJavaIntArray(env, int_array);
}

ScopedJavaLocalRef<jobject> ConvertToJavaIdentityProviderData(
    JNIEnv* env,
    content::IdentityProviderData* idp_data) {
  return Java_IdentityProviderData_Constructor(
      env, idp_data->idp_for_display,
      ConvertToJavaIdentityProviderMetadata(env, idp_data->idp_metadata),
      ConvertToJavaClientIdMetadata(env, idp_data->client_metadata),
      static_cast<jint>(idp_data->rp_context),
      ConvertFieldsToJavaArray(env, idp_data->disclosure_fields),
      idp_data->has_login_status_mismatch);
}

IdentityRequestAccountPtr ConvertFieldsToAccount(
    JNIEnv* env,
    const std::vector<std::string>& string_fields,
    const GURL& picture_url,
    bool is_sign_in) {
  auto account_id = string_fields[0];
  auto email = string_fields[1];
  auto name = string_fields[2];
  auto given_name = string_fields[3];

  Account::LoginState login_state =
      is_sign_in ? Account::LoginState::kSignIn : Account::LoginState::kSignUp;

  // The following fields are only used before account selection.
  std::vector<std::string> login_hints;
  std::vector<std::string> domain_hints;
  std::vector<std::string> labels;
  Account::LoginState browser_trusted_login_state =
      Account::LoginState::kSignUp;

  return base::MakeRefCounted<Account>(
      account_id, email, name, given_name, picture_url, std::move(login_hints),
      std::move(domain_hints), std::move(labels), login_state,
      browser_trusted_login_state);
}

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class FedCmJavaObjectCreationOutcome {
  kNewObjectCreated = 0,
  kObjectReused = 1,
  kObjectCreationFailed = 2,
  kNoNativeView = 3,
  kNoWindow = 4,

  kMaxValue = kNoWindow
};

void RecordJavaObjectCreationOutcome(
    std::optional<blink::mojom::RpMode> rp_mode,
    FedCmJavaObjectCreationOutcome outcome) {
  // Rp mode may be unavailable in cases that the request is invoked from CCT.
  // There's no need to record metrics in such case.
  if (!rp_mode) {
    return;
  }
  const char* mode =
      *rp_mode == blink::mojom::RpMode::kPassive ? "Passive" : "Active";
  base::UmaHistogramEnumeration(
      base::StringPrintf("Blink.FedCm.JavaObjectCreationOutcome.%s", mode),
      outcome);
}

}  // namespace

AccountSelectionViewAndroid::AccountSelectionViewAndroid(
    AccountSelectionView::Delegate* delegate)
    : AccountSelectionView(delegate) {}

AccountSelectionViewAndroid::~AccountSelectionViewAndroid() {
  if (java_object_internal_) {
    // Don't create an object just for destruction.
    Java_AccountSelectionBridge_destroy(AttachCurrentThread(),
                                        java_object_internal_);
  }
}

bool AccountSelectionViewAndroid::Show(
    const std::string& rp_for_display,
    const std::vector<IdentityProviderDataPtr>& idp_list,
    const std::vector<IdentityRequestAccountPtr>& accounts,
    Account::SignInMode sign_in_mode,
    blink::mojom::RpMode rp_mode,
    const std::vector<IdentityRequestAccountPtr>& new_accounts) {
  if (!MaybeCreateJavaObject(rp_mode)) {
    // It's possible that the constructor cannot access the bottom sheet clank
    // component. That case may be temporary but we can't let users in a
    // waiting state so report that AccountSelectionView is dismissed instead.
    delegate_->OnDismiss(DismissReason::kOther);
    return false;
  }

  // Serialize the `idp_list` and `accounts` into a Java array and
  // instruct the bridge to show it together with |url| to the user.
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobjectArray> accounts_obj =
      ConvertToJavaAccounts(env, accounts);

  ScopedJavaLocalRef<jobjectArray> new_accounts_obj =
      ConvertToJavaAccounts(env, new_accounts);

  // Multi IDP support does not currently work on mobile. Hence, we use the
  // first index from the `idp_list` for the IDP-specific
  // information.
  ScopedJavaLocalRef<jobject> idp_obj =
      ConvertToJavaIdentityProviderData(env, idp_list[0].get());

  // TODO(crbug.com/329235198): Support auto re-authn on Android.
  Java_AccountSelectionBridge_showAccounts(
      env, java_object_internal_, rp_for_display, idp_list[0]->idp_for_display,
      accounts_obj, idp_obj,
      sign_in_mode == Account::SignInMode::kAuto &&
          rp_mode == blink::mojom::RpMode::kPassive,
      new_accounts_obj);
  return true;
}

bool AccountSelectionViewAndroid::ShowFailureDialog(
    const std::string& rp_for_display,
    const std::string& idp_for_display,
    blink::mojom::RpContext rp_context,
    blink::mojom::RpMode rp_mode,
    const content::IdentityProviderMetadata& idp_metadata) {
  // ShowFailureDialog is never called in active mode.
  // TODO(crbug.com/347736746): Remove rp_mode from this method.
  CHECK(rp_mode == blink::mojom::RpMode::kPassive);

  if (!MaybeCreateJavaObject(rp_mode)) {
    // It's possible that the constructor cannot access the bottom sheet clank
    // component. That case may be temporary but we can't let users in a
    // waiting state so report that AccountSelectionView is dismissed instead.
    delegate_->OnDismiss(DismissReason::kOther);
    return false;
  }
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> idp_metadata_obj =
      ConvertToJavaIdentityProviderMetadata(env, idp_metadata);
  Java_AccountSelectionBridge_showFailureDialog(
      env, java_object_internal_, rp_for_display, idp_for_display,
      idp_metadata_obj, static_cast<jint>(rp_context));
  return true;
}

bool AccountSelectionViewAndroid::ShowErrorDialog(
    const std::string& rp_for_display,
    const std::string& idp_for_display,
    blink::mojom::RpContext rp_context,
    blink::mojom::RpMode rp_mode,
    const content::IdentityProviderMetadata& idp_metadata,
    const std::optional<TokenError>& error) {
  // TODO(crbug.com/347117752): Implement active mode error dialog.
  if (rp_mode == blink::mojom::RpMode::kActive ||
      !MaybeCreateJavaObject(rp_mode)) {
    // It's possible that the constructor cannot access the bottom sheet clank
    // component. That case may be temporary but we can't let users in a
    // waiting state so report that AccountSelectionView is dismissed instead.
    delegate_->OnDismiss(DismissReason::kOther);
    return false;
  }
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> idp_metadata_obj =
      ConvertToJavaIdentityProviderMetadata(env, idp_metadata);
  Java_AccountSelectionBridge_showErrorDialog(
      env, java_object_internal_, rp_for_display, idp_for_display,
      idp_metadata_obj, static_cast<jint>(rp_context),
      ConvertToJavaIdentityCredentialTokenError(env, error));
  return true;
}

bool AccountSelectionViewAndroid::ShowLoadingDialog(
    const std::string& rp_for_display,
    const std::string& idp_for_display,
    blink::mojom::RpContext rp_context,
    blink::mojom::RpMode rp_mode) {
  if (!MaybeCreateJavaObject(rp_mode)) {
    // It's possible that the constructor cannot access the bottom sheet clank
    // component. That case may be temporary but we can't let users in a
    // waiting state so report that AccountSelectionView is dismissed instead.
    delegate_->OnDismiss(DismissReason::kOther);
    return false;
  }
  JNIEnv* env = AttachCurrentThread();
  Java_AccountSelectionBridge_showLoadingDialog(env, java_object_internal_,
                                                rp_for_display, idp_for_display,
                                                static_cast<jint>(rp_context));
  return true;
}

std::string AccountSelectionViewAndroid::GetTitle() const {
  JNIEnv* env = AttachCurrentThread();
  return Java_AccountSelectionBridge_getTitle(env, java_object_internal_);
}

std::optional<std::string> AccountSelectionViewAndroid::GetSubtitle() const {
  JNIEnv* env = AttachCurrentThread();
  return Java_AccountSelectionBridge_getSubtitle(env, java_object_internal_);
}

void AccountSelectionViewAndroid::ShowUrl(LinkType link_type, const GURL& url) {
  JNIEnv* env = AttachCurrentThread();
  Java_AccountSelectionBridge_showUrl(env, java_object_internal_,
                                      static_cast<int>(link_type), url);
}

content::WebContents* AccountSelectionViewAndroid::ShowModalDialog(
    const GURL& url,
    blink::mojom::RpMode rp_mode) {
  if (!MaybeCreateJavaObject(rp_mode)) {
    // The Java object is tied to the bottomsheet availability, so if we hadn't
    // created one and the bottomsheet is not available then the CCT will not be
    // opened.
    delegate_->OnDismiss(DismissReason::kOther);
    return nullptr;
  }
  JNIEnv* env = AttachCurrentThread();
  return content::WebContents::FromJavaWebContents(
      Java_AccountSelectionBridge_showModalDialog(env, java_object_internal_,
                                                  url));
}

void AccountSelectionViewAndroid::CloseModalDialog() {
  // Since this is triggered only after the CCT is opened, leaving it out of the metrics
  // to focus on cases where a UI cannot be displayed.
  if (!MaybeCreateJavaObject(/*rp_mode=*/std::nullopt)) {
    return;
  }
  JNIEnv* env = AttachCurrentThread();
  Java_AccountSelectionBridge_closeModalDialog(env, java_object_internal_);
}

content::WebContents* AccountSelectionViewAndroid::GetRpWebContents() {
  // The Java object needs to be recreated, as this is invoked for the
  // CCT. Rp mode isn't meaningful in this case so we don't pass it for metrics.
  if (!MaybeCreateJavaObject(/*rp_mode=*/std::nullopt)) {
    return nullptr;
  }
  JNIEnv* env = AttachCurrentThread();
  return content::WebContents::FromJavaWebContents(
      Java_AccountSelectionBridge_getRpWebContents(env, java_object_internal_));
}

void AccountSelectionViewAndroid::OnAccountSelected(
    JNIEnv* env,
    const GURL& idp_config_url,
    const std::vector<std::string>& account_string_fields,
    const GURL& account_picture_url,
    bool is_sign_in) {
  delegate_->OnAccountSelected(
      idp_config_url, *ConvertFieldsToAccount(env, account_string_fields,
                                              account_picture_url, is_sign_in));
  // The AccountSelectionViewAndroid may be destroyed.
  // AccountSelectionView::Delegate::OnAccountSelected() might delete this.
  // See https://crbug.com/1393650 for details.
}

void AccountSelectionViewAndroid::OnDismiss(JNIEnv* env, jint dismiss_reason) {
  delegate_->OnDismiss(static_cast<DismissReason>(dismiss_reason));
}

void AccountSelectionViewAndroid::OnLoginToIdP(JNIEnv* env,
                                               const GURL& idp_config_url,
                                               const GURL& idp_login_url) {
  delegate_->OnLoginToIdP(idp_config_url, idp_login_url);
}

void AccountSelectionViewAndroid::OnMoreDetails(JNIEnv* env) {
  delegate_->OnMoreDetails();
}

void AccountSelectionViewAndroid::OnAccountsDisplayed(JNIEnv* env) {
  delegate_->OnAccountsDisplayed();
}

bool AccountSelectionViewAndroid::MaybeCreateJavaObject(
    std::optional<blink::mojom::RpMode> rp_mode) {
  if (!delegate_->GetNativeView()) {
    RecordJavaObjectCreationOutcome(
        rp_mode, FedCmJavaObjectCreationOutcome::kNoNativeView);
    return false;
  }
  if (!delegate_->GetNativeView()->GetWindowAndroid()) {
    RecordJavaObjectCreationOutcome(rp_mode,
                                    FedCmJavaObjectCreationOutcome::kNoWindow);
    return false;  // No window attached (yet or anymore).
  }
  if (java_object_internal_) {
    RecordJavaObjectCreationOutcome(
        rp_mode, FedCmJavaObjectCreationOutcome::kObjectReused);
    return true;
  }
  JNIEnv* env = AttachCurrentThread();
  java_object_internal_ = Java_AccountSelectionBridge_create(
      env, reinterpret_cast<intptr_t>(this),
      delegate_->GetWebContents()->GetJavaWebContents(),
      delegate_->GetNativeView()->GetWindowAndroid()->GetJavaObject(),
      static_cast<jint>(rp_mode.value_or(blink::mojom::RpMode::kPassive)));

  if (!!java_object_internal_) {
    RecordJavaObjectCreationOutcome(
        rp_mode, FedCmJavaObjectCreationOutcome::kNewObjectCreated);
  } else {
    RecordJavaObjectCreationOutcome(
        rp_mode, FedCmJavaObjectCreationOutcome::kObjectCreationFailed);
  }
  return !!java_object_internal_;
}

// static
std::unique_ptr<AccountSelectionView> AccountSelectionView::Create(
    AccountSelectionView::Delegate* delegate) {
  return std::make_unique<AccountSelectionViewAndroid>(delegate);
}

// static
int AccountSelectionView::GetBrandIconMinimumSize(
    blink::mojom::RpMode rp_mode) {
  return Java_AccountSelectionBridge_getBrandIconMinimumSize(
      base::android::AttachCurrentThread(), static_cast<jint>(rp_mode));
}

// static
int AccountSelectionView::GetBrandIconIdealSize(blink::mojom::RpMode rp_mode) {
  return Java_AccountSelectionBridge_getBrandIconIdealSize(
      base::android::AttachCurrentThread(), static_cast<jint>(rp_mode));
}
