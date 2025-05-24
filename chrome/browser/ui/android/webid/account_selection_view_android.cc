// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/webid/account_selection_view_android.h"

#include <memory>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/containers/flat_map.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/webid/account_selection_view.h"
#include "chrome/browser/ui/webid/identity_ui_utils.h"
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

// The size of the circle cropped avatar on Android, not including the offset
// from badging.
constexpr int kCircleCroppedBadgedAvatarSize = 40;

ScopedJavaLocalRef<jobject> ConvertToJavaAccount(
    JNIEnv* env,
    content::IdentityRequestAccount* account,
    bool is_multi_idp,
    ScopedJavaLocalRef<jobject> identity_provider) {
  ScopedJavaLocalRef<jobject> decoded_picture = nullptr;
  if (!account->decoded_picture.IsEmpty()) {
    decoded_picture =
        gfx::ConvertToJavaBitmap(*account->decoded_picture.ToSkBitmap());
  }
  ScopedJavaLocalRef<jobject> circle_cropped_badged_picture = nullptr;
  if (is_multi_idp) {
    circle_cropped_badged_picture = gfx::ConvertToJavaBitmap(
        gfx::Image(webid::ComputeAccountCircleCroppedPicture(
                       *account, /*avatar_size=*/kCircleCroppedBadgedAvatarSize,
                       std::make_optional<gfx::ImageSkia>(
                           account->identity_provider->idp_metadata
                               .brand_decoded_icon.AsImageSkia())))
            .AsBitmap());
  }
  return Java_Account_Constructor(
      env, account->id, account->display_identifier, account->display_name,
      account->given_name,
      is_multi_idp ? std::make_optional<std::string>(
                         account->identity_provider->idp_for_display)
                   : std::nullopt,
      // TODO(crbug.com/398001374): Pass the circle cropped image here to avoid
      // duplication of code on Android.
      decoded_picture, circle_cropped_badged_picture,
      account->login_state == Account::LoginState::kSignIn,
      account->browser_trusted_login_state == Account::LoginState::kSignIn,
      account->is_filtered_out, identity_provider);
}

ScopedJavaLocalRef<jobject> ConvertToJavaIdentityProviderMetadata(
    JNIEnv* env,
    const content::IdentityProviderMetadata& metadata,
    blink::mojom::RpMode rp_mode) {
  ScopedJavaLocalRef<jobject> decoded_picture = nullptr;
  if (!metadata.brand_decoded_icon.IsEmpty()) {
    decoded_picture =
        gfx::ConvertToJavaBitmap(*metadata.brand_decoded_icon.ToSkBitmap());
  }
  return Java_IdentityProviderMetadata_Constructor(
      env, ui::OptionalSkColorToJavaColor(metadata.brand_text_color),
      ui::OptionalSkColorToJavaColor(metadata.brand_background_color),
      decoded_picture, metadata.config_url, metadata.idp_login_url,
      // We only support the add account feature on active mode. In both modes,
      // we still show this button in the filtered out accounts case.
      rp_mode == blink::mojom::RpMode::kPassive
          ? metadata.has_filtered_out_account
          : metadata.supports_add_account || metadata.has_filtered_out_account);
}

ScopedJavaLocalRef<jobject> ConvertToJavaIdentityCredentialTokenError(
    JNIEnv* env,
    const std::optional<TokenError>& error) {
  return Java_IdentityCredentialTokenError_Constructor(
      env, error ? error->code : "", error ? error->url : GURL());
}

ScopedJavaLocalRef<jobject> ConvertToJavaClientIdMetadata(
    JNIEnv* env,
    const content::ClientMetadata& metadata) {
  ScopedJavaLocalRef<jobject> brand_icon_bitmap = nullptr;
  if (!metadata.brand_decoded_icon.IsEmpty()) {
    brand_icon_bitmap =
        gfx::ConvertToJavaBitmap(*metadata.brand_decoded_icon.ToSkBitmap());
  }
  return Java_ClientIdMetadata_Constructor(env, metadata.terms_of_service_url,
                                           metadata.privacy_policy_url,
                                           brand_icon_bitmap);
}

ScopedJavaLocalRef<jobjectArray> ConvertToJavaAccounts(
    JNIEnv* env,
    const std::vector<IdentityRequestAccountPtr>& accounts,
    const base::flat_map<IdentityProviderDataPtr, ScopedJavaLocalRef<jobject>>&
        identity_providers_map) {
  ScopedJavaLocalRef<jclass> account_clazz = base::android::GetClass(
      env, "org/chromium/chrome/browser/ui/android/webid/data/Account");
  ScopedJavaLocalRef<jobjectArray> array(
      env, env->NewObjectArray(accounts.size(), account_clazz.obj(), nullptr));

  base::android::CheckException(env);

  bool is_multi_idp = identity_providers_map.size() > 1u;
  for (size_t i = 0; i < accounts.size(); ++i) {
    ScopedJavaLocalRef<jobject> item = ConvertToJavaAccount(
        env, accounts[i].get(), is_multi_idp,
        identity_providers_map.at(accounts[i]->identity_provider));
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
    content::IdentityProviderData* idp_data,
    blink::mojom::RpMode rp_mode) {
  return Java_IdentityProviderData_Constructor(
      env, idp_data->idp_for_display,
      ConvertToJavaIdentityProviderMetadata(env, idp_data->idp_metadata,
                                            rp_mode),
      ConvertToJavaClientIdMetadata(env, idp_data->client_metadata),
      static_cast<jint>(idp_data->rp_context),
      ConvertFieldsToJavaArray(env, idp_data->disclosure_fields),
      idp_data->has_login_status_mismatch);
}

base::flat_map<IdentityProviderDataPtr, ScopedJavaLocalRef<jobject>>
ConvertToJavaIdentityProviderDataMap(
    JNIEnv* env,
    const std::vector<IdentityProviderDataPtr>& identity_providers,
    blink::mojom::RpMode rp_mode) {
  base::flat_map<IdentityProviderDataPtr, ScopedJavaLocalRef<jobject>> map;

  for (const auto& identity_provider : identity_providers) {
    map[identity_provider] = ConvertToJavaIdentityProviderData(
        env, identity_provider.get(), rp_mode);
  }
  return map;
}

ScopedJavaLocalRef<jobjectArray> ConvertToJavaIdentityProvidersList(
    JNIEnv* env,
    base::flat_map<IdentityProviderDataPtr, ScopedJavaLocalRef<jobject>>
        identity_providers_map) {
  ScopedJavaLocalRef<jclass> identity_provider_clazz = base::android::GetClass(
      env,
      "org/chromium/chrome/browser/ui/android/webid/data/IdentityProviderData");
  ScopedJavaLocalRef<jobjectArray> array(
      env, env->NewObjectArray(identity_providers_map.size(),
                               identity_provider_clazz.obj(), nullptr));

  base::android::CheckException(env);
  size_t i = 0;
  for (const auto& iter : identity_providers_map) {
    env->SetObjectArrayElement(array.obj(), i++, iter.second.obj());
  }
  return array;
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
    const content::RelyingPartyData& rp_data,
    const std::vector<IdentityProviderDataPtr>& idp_list,
    const std::vector<IdentityRequestAccountPtr>& accounts,
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
  // TODO(crbug.com/40945672): render filtered out accounts differently on
  // Android.
  JNIEnv* env = AttachCurrentThread();

  base::flat_map<IdentityProviderDataPtr, ScopedJavaLocalRef<jobject>>
      identity_providers_map =
          ConvertToJavaIdentityProviderDataMap(env, idp_list, rp_mode);

  ScopedJavaLocalRef<jobjectArray> accounts_obj =
      ConvertToJavaAccounts(env, accounts, identity_providers_map);

  ScopedJavaLocalRef<jobjectArray> new_accounts_obj =
      ConvertToJavaAccounts(env, new_accounts, identity_providers_map);

  ScopedJavaLocalRef<jobjectArray> identity_providers_list =
      ConvertToJavaIdentityProvidersList(env, identity_providers_map);

  return Java_AccountSelectionBridge_showAccounts(
      env, java_object_internal_, rp_data.rp_for_display, accounts_obj,
      identity_providers_list, new_accounts_obj);
}

bool AccountSelectionViewAndroid::ShowFailureDialog(
    const content::RelyingPartyData& rp_data,
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
      ConvertToJavaIdentityProviderMetadata(env, idp_metadata, rp_mode);
  // TODO(crbug.com/382086282): Pass RelyingPartyData to Java.
  return Java_AccountSelectionBridge_showFailureDialog(
      env, java_object_internal_, rp_data.rp_for_display, idp_for_display,
      idp_metadata_obj, static_cast<jint>(rp_context));
}

bool AccountSelectionViewAndroid::ShowErrorDialog(
    const content::RelyingPartyData& rp_data,
    const std::string& idp_for_display,
    blink::mojom::RpContext rp_context,
    blink::mojom::RpMode rp_mode,
    const content::IdentityProviderMetadata& idp_metadata,
    const std::optional<TokenError>& error) {
  if (!MaybeCreateJavaObject(rp_mode)) {
    // It's possible that the constructor cannot access the bottom sheet clank
    // component. That case may be temporary but we can't let users in a
    // waiting state so report that AccountSelectionView is dismissed instead.
    delegate_->OnDismiss(DismissReason::kOther);
    return false;
  }
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> idp_metadata_obj =
      ConvertToJavaIdentityProviderMetadata(env, idp_metadata, rp_mode);
  // TODO(crbug.com/382086282): Pass RelyingPartyData to Java.
  return Java_AccountSelectionBridge_showErrorDialog(
      env, java_object_internal_, rp_data.rp_for_display, idp_for_display,
      idp_metadata_obj, static_cast<jint>(rp_context),
      ConvertToJavaIdentityCredentialTokenError(env, error));
}

bool AccountSelectionViewAndroid::ShowLoadingDialog(
    const content::RelyingPartyData& rp_data,
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
  // TODO(crbug.com/382086282): Pass RelyingPartyData to Java.
  return Java_AccountSelectionBridge_showLoadingDialog(
      env, java_object_internal_, rp_data.rp_for_display, idp_for_display,
      static_cast<jint>(rp_context));
}

bool AccountSelectionViewAndroid::ShowVerifyingDialog(
    const content::RelyingPartyData& rp_data,
    const IdentityProviderDataPtr& idp_data,
    const IdentityRequestAccountPtr& account,
    Account::SignInMode sign_in_mode,
    blink::mojom::RpMode rp_mode) {
  if (!MaybeCreateJavaObject(rp_mode)) {
    // It's possible that the constructor cannot access the bottom sheet clank
    // component.
    return false;
  }
  JNIEnv* env = AttachCurrentThread();

  ScopedJavaLocalRef<jobject> idp_obj =
      ConvertToJavaIdentityProviderData(env, idp_data.get(), rp_mode);

  ScopedJavaLocalRef<jobject> account_obj =
      ConvertToJavaAccount(env, account.get(),
                           /*is_multi_idp=*/false, idp_obj);

  return Java_AccountSelectionBridge_showVerifyingDialog(
      env, java_object_internal_, account_obj,
      sign_in_mode == Account::SignInMode::kAuto);
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
  // Since this is triggered only after the CCT is opened, leaving it out of the
  // metrics to focus on cases where a UI cannot be displayed.
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
    const std::string& account_id,
    bool is_sign_in) {
  delegate_->OnAccountSelected(
      idp_config_url, account_id,
      is_sign_in ? Account::LoginState::kSignIn : Account::LoginState::kSignUp);
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
