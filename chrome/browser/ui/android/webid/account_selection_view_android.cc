// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/webid/account_selection_view_android.h"

#include <memory>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/strings/string_piece.h"
#include "chrome/browser/ui/android/webid/internal/jni/AccountSelectionBridge_jni.h"
#include "chrome/browser/ui/android/webid/jni_headers/Account_jni.h"
#include "chrome/browser/ui/android/webid/jni_headers/ClientIdMetadata_jni.h"
#include "chrome/browser/ui/android/webid/jni_headers/IdentityCredentialTokenError_jni.h"
#include "chrome/browser/ui/android/webid/jni_headers/IdentityProviderMetadata_jni.h"
#include "chrome/browser/ui/webid/account_selection_view.h"
#include "content/public/browser/identity_request_dialog_controller.h"
#include "third_party/blink/public/mojom/webid/federated_auth_request.mojom.h"
#include "ui/android/color_utils_android.h"
#include "ui/android/window_android.h"
#include "url/android/gurl_android.h"
#include "url/gurl.h"

using base::android::AppendJavaStringArrayToStringVector;
using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;
using DismissReason = content::IdentityRequestDialogController::DismissReason;

namespace {

ScopedJavaLocalRef<jobject> ConvertToJavaAccount(JNIEnv* env,
                                                 const Account& account) {
  return Java_Account_Constructor(
      env, ConvertUTF8ToJavaString(env, account.id),
      ConvertUTF8ToJavaString(env, account.email),
      ConvertUTF8ToJavaString(env, account.name),
      ConvertUTF8ToJavaString(env, account.given_name),
      url::GURLAndroid::FromNativeGURL(env, account.picture),
      account.login_state == Account::LoginState::kSignIn);
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
      url::GURLAndroid::FromNativeGURL(env, metadata.idp_login_url));
}

ScopedJavaLocalRef<jobject> ConvertToJavaIdentityCredentialTokenError(
    JNIEnv* env,
    const absl::optional<TokenError>& error) {
  return Java_IdentityCredentialTokenError_Constructor(
      env,
      base::android::ConvertUTF8ToJavaString(env, error ? error->code : ""),
      url::GURLAndroid::FromNativeGURL(env, error ? error->url : GURL()));
}

ScopedJavaLocalRef<jobject> ConvertToJavaClientIdMetadata(
    JNIEnv* env,
    const content::ClientMetadata& metadata) {
  return Java_ClientIdMetadata_Constructor(
      env, url::GURLAndroid::FromNativeGURL(env, metadata.terms_of_service_url),
      url::GURLAndroid::FromNativeGURL(env, metadata.privacy_policy_url));
}

ScopedJavaLocalRef<jobjectArray> ConvertToJavaAccounts(
    JNIEnv* env,
    const std::vector<Account>& accounts) {
  ScopedJavaLocalRef<jclass> account_clazz = base::android::GetClass(
      env, "org/chromium/chrome/browser/ui/android/webid/data/Account");
  ScopedJavaLocalRef<jobjectArray> array(
      env, env->NewObjectArray(accounts.size(), account_clazz.obj(), nullptr));

  base::android::CheckException(env);

  for (size_t i = 0; i < accounts.size(); ++i) {
    ScopedJavaLocalRef<jobject> item = ConvertToJavaAccount(env, accounts[i]);
    env->SetObjectArrayElement(array.obj(), i, item.obj());
  }
  return array;
}

Account ConvertFieldsToAccount(
    JNIEnv* env,
    const JavaParamRef<jobjectArray>& string_fields_obj,
    const JavaParamRef<jobject>& picture_url_obj,
    bool is_sign_in) {
  std::vector<std::string> string_fields;
  AppendJavaStringArrayToStringVector(env, string_fields_obj, &string_fields);
  auto account_id = string_fields[0];
  auto email = string_fields[1];
  auto name = string_fields[2];
  auto given_name = string_fields[3];

  Account::LoginState login_state =
      is_sign_in ? Account::LoginState::kSignIn : Account::LoginState::kSignUp;

  GURL picture_url = *url::GURLAndroid::ToNativeGURL(env, picture_url_obj);

  // The login hints and domain hints are only used before account selection.
  std::vector<std::string> login_hints;
  std::vector<std::string> domain_hints;
  return Account(account_id, email, name, given_name, picture_url,
                 std::move(login_hints), std::move(domain_hints), login_state);
}

ScopedJavaLocalRef<jstring> ConvertRpContextToJavaString(
    JNIEnv* env,
    blink::mojom::RpContext rp_context) {
  std::string rp_context_string;
  switch (rp_context) {
    case blink::mojom::RpContext::kSignUp:
      rp_context_string = "signup";
      break;
    case blink::mojom::RpContext::kUse:
      rp_context_string = "use";
      break;
    case blink::mojom::RpContext::kContinue:
      rp_context_string = "continue";
      break;
    default:
      rp_context_string = "signin";
  }
  return ConvertUTF8ToJavaString(env, rp_context_string);
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

void AccountSelectionViewAndroid::Show(
    const std::string& top_frame_for_display,
    const absl::optional<std::string>& iframe_for_display,
    const std::vector<content::IdentityProviderData>& identity_provider_data,
    Account::SignInMode sign_in_mode,
    bool show_auto_reauthn_checkbox) {
  if (!MaybeCreateJavaObject()) {
    // It's possible that the constructor cannot access the bottom sheet clank
    // component. That case may be temporary but we can't let users in a
    // waiting state so report that AccountSelectionView is dismissed instead.
    delegate_->OnDismiss(DismissReason::kOther);
    return;
  }

  // Serialize the `identity_provider_data.accounts` into a Java array and
  // instruct the bridge to show it together with |url| to the user.
  JNIEnv* env = AttachCurrentThread();
  // Multi IDP support does not currently work on mobile. Hence, we use the
  // first index from the `identity_provider_data` for the IDP-specific
  // information.
  ScopedJavaLocalRef<jobjectArray> accounts_obj =
      ConvertToJavaAccounts(env, identity_provider_data[0].accounts);
  ScopedJavaLocalRef<jobject> idp_metadata_obj =
      ConvertToJavaIdentityProviderMetadata(
          env, identity_provider_data[0].idp_metadata);
  ScopedJavaLocalRef<jobject> client_id_metadata_obj =
      ConvertToJavaClientIdMetadata(env,
                                    identity_provider_data[0].client_metadata);

  Java_AccountSelectionBridge_showAccounts(
      env, java_object_internal_,
      ConvertUTF8ToJavaString(env, top_frame_for_display),
      ConvertUTF8ToJavaString(env, iframe_for_display.value_or("")),
      ConvertUTF8ToJavaString(env, identity_provider_data[0].idp_for_display),
      accounts_obj, idp_metadata_obj, client_id_metadata_obj,
      sign_in_mode == Account::SignInMode::kAuto,
      ConvertRpContextToJavaString(env, identity_provider_data[0].rp_context));
}

void AccountSelectionViewAndroid::ShowFailureDialog(
    const std::string& top_frame_for_display,
    const absl::optional<std::string>& iframe_for_display,
    const std::string& idp_for_display,
    const blink::mojom::RpContext& rp_context,
    const content::IdentityProviderMetadata& idp_metadata) {
  if (!MaybeCreateJavaObject()) {
    // It's possible that the constructor cannot access the bottom sheet clank
    // component. That case may be temporary but we can't let users in a
    // waiting state so report that AccountSelectionView is dismissed instead.
    delegate_->OnDismiss(DismissReason::kOther);
    return;
  }
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> idp_metadata_obj =
      ConvertToJavaIdentityProviderMetadata(env, idp_metadata);
  Java_AccountSelectionBridge_showFailureDialog(
      env, java_object_internal_,
      ConvertUTF8ToJavaString(env, top_frame_for_display),
      ConvertUTF8ToJavaString(env, iframe_for_display.value_or("")),
      ConvertUTF8ToJavaString(env, idp_for_display), idp_metadata_obj,
      ConvertRpContextToJavaString(env, rp_context));
}

void AccountSelectionViewAndroid::ShowErrorDialog(
    const std::string& top_frame_for_display,
    const absl::optional<std::string>& iframe_for_display,
    const std::string& idp_for_display,
    const blink::mojom::RpContext& rp_context,
    const content::IdentityProviderMetadata& idp_metadata,
    const absl::optional<TokenError>& error) {
  if (!MaybeCreateJavaObject()) {
    // It's possible that the constructor cannot access the bottom sheet clank
    // component. That case may be temporary but we can't let users in a
    // waiting state so report that AccountSelectionView is dismissed instead.
    delegate_->OnDismiss(DismissReason::kOther);
    return;
  }
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> idp_metadata_obj =
      ConvertToJavaIdentityProviderMetadata(env, idp_metadata);
  Java_AccountSelectionBridge_showErrorDialog(
      env, java_object_internal_,
      ConvertUTF8ToJavaString(env, top_frame_for_display),
      ConvertUTF8ToJavaString(env, iframe_for_display.value_or("")),
      ConvertUTF8ToJavaString(env, idp_for_display), idp_metadata_obj,
      ConvertRpContextToJavaString(env, rp_context),
      ConvertToJavaIdentityCredentialTokenError(env, error));
}

std::string AccountSelectionViewAndroid::GetTitle() const {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jstring> title =
      Java_AccountSelectionBridge_getTitle(env, java_object_internal_);
  CHECK(title);
  return ConvertJavaStringToUTF8(title);
}

absl::optional<std::string> AccountSelectionViewAndroid::GetSubtitle() const {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jstring> subtitle =
      Java_AccountSelectionBridge_getSubtitle(env, java_object_internal_);
  if (!subtitle) {
    return absl::nullopt;
  }
  return ConvertJavaStringToUTF8(subtitle);
}

content::WebContents* AccountSelectionViewAndroid::ShowModalDialog(
    const GURL& url) {
  JNIEnv* env = AttachCurrentThread();
  return content::WebContents::FromJavaWebContents(
      Java_AccountSelectionBridge_showModalDialog(
          env, java_object_internal_,
          url::GURLAndroid::FromNativeGURL(env, url)));
}

void AccountSelectionViewAndroid::CloseModalDialog() {
  // The Java object needs to be recreated, as this is invoked for the
  // CCT that was closed.
  if (!MaybeCreateJavaObject()) {
    return;
  }
  JNIEnv* env = AttachCurrentThread();
  Java_AccountSelectionBridge_closeModalDialog(env, java_object_internal_);
}

void AccountSelectionViewAndroid::OnAccountSelected(
    JNIEnv* env,
    const JavaParamRef<jobject>& idp_config_url,
    const JavaParamRef<jobjectArray>& account_string_fields,
    const JavaParamRef<jobject>& account_picture_url,
    bool is_sign_in) {
  GURL config_url = *url::GURLAndroid::ToNativeGURL(env, idp_config_url);
  delegate_->OnAccountSelected(
      config_url, ConvertFieldsToAccount(env, account_string_fields,
                                         account_picture_url, is_sign_in));
  // The AccountSelectionViewAndroid may be destroyed.
  // AccountSelectionView::Delegate::OnAccountSelected() might delete this.
  // See https://crbug.com/1393650 for details.
}

void AccountSelectionViewAndroid::OnDismiss(JNIEnv* env, jint dismiss_reason) {
  delegate_->OnDismiss(static_cast<DismissReason>(dismiss_reason));
}

void AccountSelectionViewAndroid::OnLoginToIdP(
    JNIEnv* env,
    const JavaParamRef<jobject>& idp_login_url) {
  GURL login_url = *url::GURLAndroid::ToNativeGURL(env, idp_login_url);
  delegate_->OnLoginToIdP(login_url);
}

void AccountSelectionViewAndroid::OnMoreDetails(JNIEnv* env) {
  delegate_->OnMoreDetails();
}

bool AccountSelectionViewAndroid::MaybeCreateJavaObject() {
  if (delegate_->GetNativeView() == nullptr ||
      delegate_->GetNativeView()->GetWindowAndroid() == nullptr) {
    return false;  // No window attached (yet or anymore).
  }
  if (java_object_internal_) {
    return true;
  }
  java_object_internal_ = Java_AccountSelectionBridge_create(
      AttachCurrentThread(), reinterpret_cast<intptr_t>(this),
      delegate_->GetWebContents()->GetJavaWebContents(),
      delegate_->GetNativeView()->GetWindowAndroid()->GetJavaObject());
  return !!java_object_internal_;
}

// static
std::unique_ptr<AccountSelectionView> AccountSelectionView::Create(
    AccountSelectionView::Delegate* delegate) {
  return std::make_unique<AccountSelectionViewAndroid>(delegate);
}

// static
int AccountSelectionView::GetBrandIconMinimumSize() {
  return Java_AccountSelectionBridge_getBrandIconMinimumSize(
      base::android::AttachCurrentThread());
}

// static
int AccountSelectionView::GetBrandIconIdealSize() {
  return Java_AccountSelectionBridge_getBrandIconIdealSize(
      base::android::AttachCurrentThread());
}
