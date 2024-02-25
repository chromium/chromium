// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/geo/subkey_requester.h"

#include <memory>
#include <utility>

#include "base/cancelable_callback.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "third_party/libaddressinput/chromium/chrome_address_validator.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/address_data.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/source.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/storage.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "components/autofill/android/main_autofill_jni_headers/SubKeyRequester_jni.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace autofill {

namespace {

using ::i18n::addressinput::Source;
using ::i18n::addressinput::Storage;

class SubKeyRequest : public SubKeyRequester::Request {
 public:
  // The |delegate| and |address_validator| need to outlive this Request.
  SubKeyRequest(const std::string& region_code,
                const std::string& language,
                int timeout_seconds,
                AddressValidator* address_validator,
                SubKeyReceiverCallback on_subkeys_received)
      : region_code_(region_code),
        language_(language),
        address_validator_(address_validator),
        on_subkeys_received_(std::move(on_subkeys_received)),
        on_timeout_(base::BindOnce(&SubKeyRequest::OnRulesLoaded,
                                   base::Unretained(this))) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, on_timeout_.callback(), base::Seconds(timeout_seconds));
  }

  SubKeyRequest(const SubKeyRequest&) = delete;
  SubKeyRequest& operator=(const SubKeyRequest&) = delete;

  ~SubKeyRequest() override { on_timeout_.Cancel(); }

  void OnRulesLoaded() override {
    on_timeout_.Cancel();
    // Check if the timeout happened before the rules were loaded.
    if (has_responded_)
      return;
    has_responded_ = true;

    auto subkeys =
        address_validator_->GetRegionSubKeys(region_code_, language_);
    std::vector<std::string> subkeys_codes;
    std::vector<std::string> subkeys_names;
    for (const auto& [code, name] : subkeys) {
      subkeys_codes.push_back(code);
      subkeys_names.push_back(name);
    }
    std::move(on_subkeys_received_).Run(subkeys_codes, subkeys_names);
  }

 private:
  std::string region_code_;
  std::string language_;
  // Not owned. Never null. Outlive this object.
  raw_ptr<AddressValidator> address_validator_;

  SubKeyReceiverCallback on_subkeys_received_;

  bool has_responded_ = false;
  base::CancelableOnceClosure on_timeout_;
};

#if BUILDFLAG(IS_ANDROID)
void OnSubKeysReceived(base::android::ScopedJavaGlobalRef<jobject> jdelegate,
                       const std::vector<std::string>& subkeys_codes,
                       const std::vector<std::string>& subkeys_names) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_GetSubKeysRequestDelegate_onSubKeysReceived(
      env, jdelegate, base::android::ToJavaArrayOfStrings(env, subkeys_codes),
      base::android::ToJavaArrayOfStrings(env, subkeys_names));
}
#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace

SubKeyRequester::SubKeyRequester(std::unique_ptr<Source> source,
                                 std::unique_ptr<Storage> storage,
                                 const std::string& language)
    : address_validator_(std::move(source), std::move(storage), this),
      language_(language) {}

SubKeyRequester::~SubKeyRequester() = default;

void SubKeyRequester::StartRegionSubKeysRequest(const std::string& region_code,
                                                int timeout_seconds,
                                                SubKeyReceiverCallback cb) {
  DCHECK(timeout_seconds >= 0);

  std::unique_ptr<SubKeyRequest> request(
      std::make_unique<SubKeyRequest>(region_code, language_, timeout_seconds,
                                      &address_validator_, std::move(cb)));

  if (AreRulesLoadedForRegion(region_code)) {
    request->OnRulesLoaded();
  } else {
    // Setup the variables so that the subkeys request is sent, when the rules
    // are loaded.
    pending_subkey_region_code_ = region_code;
    pending_subkey_request_ = std::move(request);

    // Start loading the rules for that region. If the rules were already in the
    // process of being loaded, this call will do nothing.
    LoadRulesForRegion(region_code);
  }
}

bool SubKeyRequester::AreRulesLoadedForRegion(const std::string& region_code) {
  return address_validator_.AreRulesLoadedForRegion(region_code);
}

void SubKeyRequester::LoadRulesForRegion(const std::string& region_code) {
  address_validator_.LoadRules(region_code);
}

void SubKeyRequester::OnAddressValidationRulesLoaded(
    const std::string& region_code,
    bool success) {
  // The case for |success| == false is already handled. if |success| == false,
  // AddressValidator::GetRegionSubKeys will return an empty list of subkeys.
  // Therefore, here, we can ignore the value of |success|.
  // Check if there is any subkey request for that region code.
  if (pending_subkey_request_ &&
      !pending_subkey_region_code_.compare(region_code)) {
    pending_subkey_request_->OnRulesLoaded();
  }
  pending_subkey_region_code_.clear();
  pending_subkey_request_.reset();
}

void SubKeyRequester::CancelPendingGetSubKeys() {
  pending_subkey_region_code_.clear();
  pending_subkey_request_.reset();
}

#if BUILDFLAG(IS_ANDROID)
base::android::ScopedJavaLocalRef<jobject> SubKeyRequester::GetJavaObject() {
  if (!java_ref_) {
    java_ref_.Reset(
        Java_SubKeyRequester_Constructor(base::android::AttachCurrentThread(),
                                         reinterpret_cast<intptr_t>(this)));
  }
  return base::android::ScopedJavaLocalRef<jobject>(java_ref_);
}

void SubKeyRequester::LoadRulesForSubKeys(
    JNIEnv* env,
    const base::android::JavaParamRef<jstring>& jregion_code) {
  LoadRulesForRegion(base::android::ConvertJavaStringToUTF8(env, jregion_code));
}

void SubKeyRequester::StartRegionSubKeysRequest(
    JNIEnv* env,
    const base::android::JavaParamRef<jstring>& jregion_code,
    jint jtimeout_seconds,
    const base::android::JavaParamRef<jobject>& jdelegate) {
  const std::string region_code =
      base::android::ConvertJavaStringToUTF8(env, jregion_code);

  base::android::ScopedJavaGlobalRef<jobject> my_jdelegate;
  my_jdelegate.Reset(env, jdelegate);

  SubKeyReceiverCallback cb =
      base::BindOnce(&OnSubKeysReceived,
                     base::android::ScopedJavaGlobalRef<jobject>(my_jdelegate));

  StartRegionSubKeysRequest(region_code, jtimeout_seconds, std::move(cb));
}

void SubKeyRequester::CancelPendingGetSubKeys(JNIEnv* env) {
  CancelPendingGetSubKeys();
}
#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace autofill
