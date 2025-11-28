// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/public/identity_manager/account_info.h"

#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/signin_constants.h"
#include "components/signin/public/identity_manager/tribool.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/gaia_id.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/jni_string.h"
#include "components/signin/public/android/jni_headers/AccountInfo_jni.h"
#include "components/signin/public/android/jni_headers/CoreAccountInfo_jni.h"
#include "ui/gfx/android/java_bitmap.h"
#include "ui/gfx/image/image_skia.h"
#endif

using signin::constants::kNoHostedDomainFound;

namespace {

// Updates |field| with |new_value| if non-empty and different; if |new_value|
// is equal to |default_value| then it won't override |field| unless it is not
// set. Returns whether |field| was changed.
bool UpdateField(std::string* field,
                 const std::string& new_value,
                 const char* default_value) {
  if (*field == new_value || new_value.empty()) {
    return false;
  }

  if (!field->empty() && default_value && new_value == default_value) {
    return false;
  }

  *field = new_value;
  return true;
}

// Updates |field| with |new_value| if different from the default value.
// Returns whether |field| was changed.
template <typename T>
bool UpdateField(T* field, T new_value, T default_value) {
  if (*field == new_value || new_value == default_value) {
    return false;
  }

  *field = new_value;
  return true;
}

// Updates |field| with |new_value| if non-empty. Returns whether |field| was
// changed.
bool UpdateField(GaiaId* field, const GaiaId& new_value) {
  if (*field == new_value || new_value.empty()) {
    return false;
  }

  *field = new_value;
  return true;
}

// Updates |field| with |new_value| if true. Returns whether |field| was
// changed.
bool UpdateField(bool* field, bool new_value) {
  return UpdateField<bool>(field, new_value, false);
}

// Updates |field| with |new_value| if true. Returns whether |field| was
// changed.
bool UpdateField(signin::Tribool* field, signin::Tribool new_value) {
  return UpdateField<signin::Tribool>(field, new_value,
                                      signin::Tribool::kUnknown);
}

}  // namespace

// This must be a string which can never be a valid picture URL.
const char kNoPictureURLFound[] = "NO_PICTURE_URL";

CoreAccountInfo::CoreAccountInfo() = default;

CoreAccountInfo::~CoreAccountInfo() = default;

CoreAccountInfo::CoreAccountInfo(const CoreAccountInfo& other) = default;

CoreAccountInfo::CoreAccountInfo(CoreAccountInfo&& other) noexcept = default;

CoreAccountInfo& CoreAccountInfo::operator=(const CoreAccountInfo& other) =
    default;

CoreAccountInfo& CoreAccountInfo::operator=(CoreAccountInfo&& other) noexcept =
    default;

bool CoreAccountInfo::IsEmpty() const {
  return account_id.empty() && email.empty() && gaia.empty();
}

AccountInfo::AccountInfo() = default;

AccountInfo::~AccountInfo() = default;

AccountInfo::AccountInfo(const AccountInfo& other) = default;

AccountInfo::AccountInfo(AccountInfo&& other) noexcept = default;

AccountInfo& AccountInfo::operator=(const AccountInfo& other) = default;

AccountInfo& AccountInfo::operator=(AccountInfo&& other) noexcept = default;

std::optional<std::string_view> AccountInfo::GetFullName() const {
  if (full_name.empty()) {
    return std::nullopt;
  }
  return full_name;
}

std::optional<std::string_view> AccountInfo::GetGivenName() const {
  if (given_name.empty()) {
    return std::nullopt;
  }
  return given_name;
}

std::optional<std::string_view> AccountInfo::GetHostedDomain() const {
  if (hosted_domain.empty()) {
    return std::nullopt;
  }
  if (hosted_domain == kNoHostedDomainFound) {
    return base::EmptyString();
  }
  return hosted_domain;
}

std::optional<std::string_view> AccountInfo::GetAvatarUrl() const {
  if (picture_url.empty()) {
    return std::nullopt;
  }
  if (picture_url == kNoPictureURLFound) {
    return base::EmptyString();
  }
  return picture_url;
}

std::optional<std::string_view>
AccountInfo::GetLastDownloadedAvatarUrlWithSize() const {
  if (last_downloaded_image_url_with_size.empty()) {
    return std::nullopt;
  }
  return last_downloaded_image_url_with_size;
}

std::optional<gfx::Image> AccountInfo::GetAvatarImage() const {
  if (account_image.IsEmpty()) {
    return std::nullopt;
  }
  return account_image;
}

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
signin_metrics::AccessPoint AccountInfo::GetLastAuthenticationAccessPoint()
    const {
  return access_point;
}
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

const AccountCapabilities& AccountInfo::GetAccountCapabilities() const {
  return capabilities;
}

signin::Tribool AccountInfo::IsChildAccount() const {
  return is_child_account;
}

std::optional<std::string_view> AccountInfo::GetLocale() const {
  if (locale.empty()) {
    return std::nullopt;
  }
  return locale;
}

bool AccountInfo::IsEmpty() const {
  return CoreAccountInfo::IsEmpty() && hosted_domain.empty() &&
         full_name.empty() && given_name.empty() && locale.empty() &&
         picture_url.empty();
}

bool AccountInfo::IsValid() const {
  return !account_id.empty() && !email.empty() && !gaia.empty() &&
         !hosted_domain.empty() && !full_name.empty() && !given_name.empty() &&
         !picture_url.empty();
}

bool AccountInfo::UpdateWith(const AccountInfo& other) {
  if (account_id != other.account_id) {
    // Only updates with a compatible AccountInfo.
    return false;
  }

  bool modified = false;
  modified |= UpdateField(&gaia, other.gaia);
  modified |= UpdateField(&email, other.email, nullptr);
  modified |= UpdateField(&full_name, other.full_name, nullptr);
  modified |= UpdateField(&given_name, other.given_name, nullptr);
  modified |=
      UpdateField(&hosted_domain, other.hosted_domain, kNoHostedDomainFound);
  modified |= UpdateField(&locale, other.locale, nullptr);
  modified |= UpdateField(&picture_url, other.picture_url, kNoPictureURLFound);
  modified |= UpdateField(&is_child_account, other.is_child_account);
  modified |= UpdateField(&access_point, other.access_point,
                          signin_metrics::AccessPoint::kUnknown);
  modified |= UpdateField(&is_under_advanced_protection,
                          other.is_under_advanced_protection);
  modified |= capabilities.UpdateWith(other.capabilities);

  return modified;
}

// static
signin::Tribool AccountInfo::IsManaged(const std::string& hosted_domain) {
  return hosted_domain.empty()
             ? signin::Tribool::kUnknown
             : signin::TriboolFromBool(hosted_domain != kNoHostedDomainFound);
}

bool AccountInfo::IsMemberOfFlexOrg() const {
  return capabilities.is_subject_to_enterprise_features() ==
             signin::Tribool::kTrue &&
         IsManaged(hosted_domain) != signin::Tribool::kTrue;
}

signin::Tribool AccountInfo::IsManaged() const {
  return IsManaged(hosted_domain);
}

signin::Tribool AccountInfo::CanApplyAccountLevelEnterprisePolicies() const {
  return IsManaged();
}

bool AccountInfo::IsEduAccount() const {
  return capabilities.can_use_edu_features() == signin::Tribool::kTrue &&
         IsManaged(hosted_domain) == signin::Tribool::kTrue;
}

bool AccountInfo::CanHaveEmailAddressDisplayed() const {
  return capabilities.can_have_email_address_displayed() ==
             signin::Tribool::kTrue ||
         capabilities.can_have_email_address_displayed() ==
             signin::Tribool::kUnknown;
}

AccountInfo::Builder::Builder(const GaiaId& gaia_id, std::string_view email) {
  CHECK(!gaia_id.empty());
  CHECK(!email.empty());
  account_info_.gaia = gaia_id;
  account_info_.email = std::string(email);
}

AccountInfo::Builder::Builder(const CoreAccountInfo& core_account_info) {
  CHECK(!core_account_info.gaia.empty());
  CHECK(!core_account_info.email.empty());
  account_info_.account_id = core_account_info.account_id;
  account_info_.gaia = core_account_info.gaia;
  account_info_.email = core_account_info.email;
  account_info_.is_under_advanced_protection =
      core_account_info.is_under_advanced_protection;
}

AccountInfo::Builder::Builder(const AccountInfo& account_info)
    : account_info_(account_info) {
  CHECK(!account_info.gaia.empty());
  CHECK(!account_info.email.empty());
}

AccountInfo::Builder::~Builder() = default;

AccountInfo AccountInfo::Builder::Build() {
  return std::move(account_info_);
}

AccountInfo::Builder& AccountInfo::Builder::SetAccountId(
    const CoreAccountId& account_id) {
  CHECK(!account_id.empty());
  account_info_.account_id = account_id;
  return *this;
}

AccountInfo::Builder& AccountInfo::Builder::SetIsUnderAdvancedProtection(
    bool is_under_advanced_protection) {
  account_info_.is_under_advanced_protection = is_under_advanced_protection;
  return *this;
}

AccountInfo::Builder& AccountInfo::Builder::SetFullName(
    std::string_view full_name_val) {
  CHECK(!full_name_val.empty());
  account_info_.full_name = std::string(full_name_val);
  return *this;
}

AccountInfo::Builder& AccountInfo::Builder::SetGivenName(
    std::string_view given_name_val) {
  CHECK(!given_name_val.empty());
  account_info_.given_name = std::string(given_name_val);
  return *this;
}

AccountInfo::Builder& AccountInfo::Builder::SetLastDownloadedAvatarUrlWithSize(
    std::string_view avatar_url_with_size) {
  CHECK(!avatar_url_with_size.empty());
  account_info_.last_downloaded_image_url_with_size =
      std::string(avatar_url_with_size);
  return *this;
}

AccountInfo::Builder& AccountInfo::Builder::SetAvatarImage(
    const gfx::Image& avatar_image) {
  CHECK(!avatar_image.IsEmpty());
  account_info_.account_image = avatar_image;
  return *this;
}

AccountInfo::Builder& AccountInfo::Builder::SetLocale(
    std::string_view locale_val) {
  CHECK(!locale_val.empty());
  account_info_.locale = std::string(locale_val);
  return *this;
}

AccountInfo::Builder& AccountInfo::Builder::SetHostedDomain(
    std::string_view hosted_domain_val) {
  account_info_.hosted_domain = hosted_domain_val.empty()
                                    ? kNoHostedDomainFound
                                    : std::string(hosted_domain_val);
  return *this;
}

AccountInfo::Builder& AccountInfo::Builder::SetAvatarUrl(
    std::string_view avatar_url) {
  account_info_.picture_url =
      avatar_url.empty() ? kNoPictureURLFound : std::string(avatar_url);
  return *this;
}

AccountInfo::Builder& AccountInfo::Builder::SetLastAuthenticationAccessPoint(
    signin_metrics::AccessPoint access_point_val) {
  account_info_.access_point = access_point_val;
  return *this;
}

AccountInfo::Builder& AccountInfo::Builder::SetIsChildAccount(
    signin::Tribool is_child_account_val) {
  account_info_.is_child_account = is_child_account_val;
  return *this;
}

AccountInfo::Builder& AccountInfo::Builder::UpdateAccountCapabilitiesWith(
    const AccountCapabilities& other) {
  account_info_.capabilities.UpdateWith(other);
  return *this;
}

AccountInfo::Builder::Builder() = default;

// static
AccountInfo::Builder AccountInfo::Builder::CreateWithPossiblyEmptyGaiaId(
    const GaiaId& gaia_id,
    std::string_view email) {
  CHECK(!email.empty());
  AccountInfo::Builder builder;
  builder.account_info_.gaia = gaia_id;
  builder.account_info_.email = email;
  return builder;
}

bool operator==(const CoreAccountInfo& l, const CoreAccountInfo& r) {
  return l.account_id == r.account_id && l.gaia == r.gaia &&
         gaia::AreEmailsSame(l.email, r.email) &&
         l.is_under_advanced_protection == r.is_under_advanced_protection;
}

std::ostream& operator<<(std::ostream& os, const CoreAccountInfo& account) {
  os << "account_id: " << account.account_id << ", gaia: " << account.gaia
     << ", email: " << account.email << ", adv_prot: " << std::boolalpha
     << account.is_under_advanced_protection;
  return os;
}

#if BUILDFLAG(IS_ANDROID)
base::android::ScopedJavaLocalRef<jobject> ConvertToJavaCoreAccountInfo(
    JNIEnv* env,
    const CoreAccountInfo& account_info) {
  CHECK(!account_info.IsEmpty());
  return signin::Java_CoreAccountInfo_Constructor(
      env, account_info.account_id, account_info.email, account_info.gaia);
}

base::android::ScopedJavaLocalRef<jobject> ConvertToJavaAccountInfo(
    JNIEnv* env,
    const AccountInfo& account_info) {
  CHECK(!account_info.IsEmpty());
  // Null domain means that the management status is unknown, which is
  // represented by `null` hostedDomain on the Java side.
  std::optional<std::string_view> maybe_hosted_domain =
      account_info.GetHostedDomain();
  base::android::ScopedJavaLocalRef<jstring> hosted_domain =
      maybe_hosted_domain.has_value()
          ? base::android::ConvertUTF8ToJavaString(
                env, maybe_hosted_domain->empty() ? kNoHostedDomainFound
                                                  : *maybe_hosted_domain)
          : nullptr;
  base::android::ScopedJavaLocalRef<jobject> account_image =
      account_info.account_image.IsEmpty()
          ? nullptr
          : gfx::ConvertToJavaBitmap(
                *account_info.account_image.AsImageSkia().bitmap());
  return signin::Java_AccountInfo_Constructor(
      env, account_info.account_id, account_info.email, account_info.gaia,
      account_info.full_name, account_info.given_name, hosted_domain,
      account_image,
      account_info.capabilities.ConvertToJavaAccountCapabilities(env));
}

CoreAccountInfo ConvertFromJavaCoreAccountInfo(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& j_core_account_info) {
  CHECK(j_core_account_info);
  CoreAccountInfo account;
  account.account_id =
      signin::Java_CoreAccountInfo_getId(env, j_core_account_info);
  account.gaia =
      signin::Java_CoreAccountInfo_getGaiaId(env, j_core_account_info);
  account.email =
      signin::Java_CoreAccountInfo_getEmail(env, j_core_account_info);
  return account;
}

AccountInfo ConvertFromJavaAccountInfo(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& j_account_info) {
  CHECK(j_account_info);
  // TODO(crbug.com/348373729): Marshal account image & capabilities from Java.
  AccountInfo::Builder builder(
      signin::Java_CoreAccountInfo_getGaiaId(env, j_account_info),
      signin::Java_CoreAccountInfo_getEmail(env, j_account_info));
  builder.SetAccountId(signin::Java_CoreAccountInfo_getId(env, j_account_info));
  if (std::string full_name =
          signin::Java_AccountInfo_getFullName(env, j_account_info);
      !full_name.empty()) {
    builder.SetFullName(full_name);
  }
  if (std::string given_name =
          signin::Java_AccountInfo_getGivenName(env, j_account_info);
      !given_name.empty()) {
    builder.SetFullName(given_name);
  }
  // Unknown hosted domain is represented by a null Java string which is
  // converted to an empty std::string. Do not call `SetHostedDomain()` in this
  // case.
  if (std::string hosted_domain = base::android::ConvertJavaStringToUTF8(
          signin::Java_AccountInfo_getRawHostedDomain(env, j_account_info));
      !hosted_domain.empty()) {
    builder.SetHostedDomain(hosted_domain);
  }
  return builder.Build();
}

#endif

#if BUILDFLAG(IS_ANDROID)
DEFINE_JNI(AccountInfo)
DEFINE_JNI(CoreAccountInfo)
#endif
