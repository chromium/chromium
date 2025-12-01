// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_ACCOUNT_INFO_H_
#define COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_ACCOUNT_INFO_H_

#include <optional>
#include <string>
#include <string_view>

#include "base/gtest_prod_util.h"
#include "build/build_config.h"
#include "components/signin/internal/identity_manager/account_info_util.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/identity_manager/account_capabilities.h"
#include "components/signin/public/identity_manager/tribool.h"
#include "google_apis/gaia/core_account_id.h"
#include "google_apis/gaia/gaia_id.h"
#include "ui/gfx/image/image.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/scoped_java_ref.h"
#endif

// Value representing no picture URL associated with an account.
extern const char kNoPictureURLFound[];

// Stores the basic information about an account that is always known
// about the account (from the moment it is added to the system until
// it is removed). It will infrequently, if ever, change.
struct CoreAccountInfo {
  CoreAccountInfo();
  ~CoreAccountInfo();

  CoreAccountInfo(const CoreAccountInfo& other);
  CoreAccountInfo(CoreAccountInfo&& other) noexcept;

  CoreAccountInfo& operator=(const CoreAccountInfo& other);
  CoreAccountInfo& operator=(CoreAccountInfo&& other) noexcept;

  CoreAccountId account_id;
  GaiaId gaia;

  // Displaying the `email` in display fields (e.g. Android View) can be
  // restricted. Please verify displayability using
  // `AccountInfo::CanHaveEmailAddressDisplayed()`.
  std::string email;

  bool is_under_advanced_protection = false;

  // Returns true if all fields in the account info are empty.
  bool IsEmpty() const;
};

// Stores all the information known about an account. Part of the information
// may only become available asynchronously, which is indicated by optional
// return values.
struct AccountInfo : public CoreAccountInfo {
  class Builder;

  AccountInfo();
  ~AccountInfo();

  AccountInfo(const AccountInfo& other);
  AccountInfo(AccountInfo&& other) noexcept;

  AccountInfo& operator=(const AccountInfo& other);
  AccountInfo& operator=(AccountInfo&& other) noexcept;

  // Returns the full name of the account.
  // Returns std::nullopt if the value is unknown yet.
  std::optional<std::string_view> GetFullName() const;

  // Returns the given name of the account.
  // Returns std::nullopt if the value is unknown yet.
  std::optional<std::string_view> GetGivenName() const;

  // Returns the hosted domain of the account. Might be empty if an account
  // doesn't have a hosted domain.
  // Returns std::nullopt if the value is unknown yet.
  std::optional<std::string_view> GetHostedDomain() const;

  // Returns the URL of the account avatar. Might be empty if the account
  // doesn't have a usable avatar.
  // Returns std::nullopt if the value is unknown yet.
  std::optional<std::string_view> GetAvatarUrl() const;

  // Returns the last downloaded account avatar URL with size.
  // Returns std::nullopt if the avatar image hasn't been downloaded yet.
  std::optional<std::string_view> GetLastDownloadedAvatarUrlWithSize() const;

  // Returns the account avatar image.
  // Returns std::nullopt if the avatar image hasn't been downloaded yet.
  std::optional<gfx::Image> GetAvatarImage() const;

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  // Returns access point used to add the account, which is also updated on
  // reauth. The access point is not updated when signing in to Chrome, only
  // when the token is updated or refreshed.
  signin_metrics::AccessPoint GetLastAuthenticationAccessPoint() const;
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

  // Returns the account capabilities.
  const AccountCapabilities& GetAccountCapabilities() const;

  // Returns whether this is a child account.
  signin::Tribool IsChildAccount() const;

  // Returns the locale of the account.
  // Returns std::nullopt if the value is unknown yet.
  std::optional<std::string_view> GetLocale() const;

  // Returns true if all fields in the account info are empty.
  bool IsEmpty() const;

  // Returns true if all non-optional fields in this account info are filled.
  // Note: IsValid() does not check if `access_point`, `is_child_account`,
  // `capabilities` or `locale` are filled.
  // Deprecated: check availability of specific fields instead.
  bool IsValid() const;

  // Updates the empty fields of |this| with |other|. Returns whether at least
  // one field was updated.
  bool UpdateWith(const AccountInfo& other);

  // Returns `kTrue` the given `hosted_domain` is managed (different from
  // kNoHostedDomainFound). Returns `kFalse` for gmail.com and other non-managed
  // domains like yahoo.com. Returns `kUnknown` if `hosted_domain` is still
  // unknown (empty).
  static signin::Tribool IsManaged(const std::string& hosted_domain);

  // Returns true if the account has no hosted domain but is a dasher account.
  bool IsMemberOfFlexOrg() const;

  // Returns `kTrue` if the account is managed (`hosted_domain` is non empty
  // different from kNoHostedDomainFound). Returns `kFalse` for gmail.com
  // accounts and other non-managed accounts like yahoo.com. Returns `kUnknown`
  // if `hosted_domain` is still unknown (empty).
  signin::Tribool IsManaged() const;

  // Returns `kTrue` if the account is managed and can apply account level
  // enterprise policies. Returns `kFalse` if the account is not managed or if
  // the account is managed but cannot apply account level enterprise policies.
  // Returns `kUnknown` the value is unknown.
  signin::Tribool CanApplyAccountLevelEnterprisePolicies() const;

  bool IsEduAccount() const;

  // Returns true if the account email can be used in display fields.
  // If `capabilities.can_have_email_address_displayed()` is unknown at the time
  // this function is called, the email address will be considered displayable.
  bool CanHaveEmailAddressDisplayed() const;

  // The following struct members are going to be moved to the private section
  // soon, do not use them directly.
  // TODO(crbug.com/458409080): move all struct members to the private section.

  // Mandatory fields for `IsValid()` to return true:
  // Deprecated: Use GetFullName() instead.
  std::string full_name;
  // Deprecated: Use GetGivenName() instead.
  std::string given_name;
  // Deprecated: Use GetHostedDomain() instead.
  std::string hosted_domain;
  // Deprecated: Use GetAvatarUrl() instead.
  std::string picture_url;

  // Deprecated: Use GetLastDownloadedAvatarUrlWithSize() instead.
  std::string last_downloaded_image_url_with_size;
  // Deprecated: Use GetAvatarImage() instead.
  gfx::Image account_image;

  // Deprecated: Use GetLastAuthenticationAccessPoint() instead.
  // The value is set consistently only on DICE platforms.
  signin_metrics::AccessPoint access_point =
      signin_metrics::AccessPoint::kUnknown;

  // Deprecated: Use GetAccountCapabilities() instead.
  AccountCapabilities capabilities;
  // Deprecated: Use IsChildAccount() instead.
  signin::Tribool is_child_account = signin::Tribool::kUnknown;
  // Deprecated: Use GetLocale() instead.
  std::string locale;

 private:
  friend class Builder;
};

// Builder class for constructing AccountInfo objects.
//
// Setter methods do not allow setting unknown values. To keep an AccountInfo
// class member in an unknown state, do not call a corresponding setter.
class AccountInfo::Builder {
 public:
  // `gaia_id` and `email` must be non-empty.
  Builder(const GaiaId& gaia_id, std::string_view email);

  // Builders that allow augmenting an already existing object with extra
  // values. Mostly useful for tests.
  // Source objects must have non-empty `gaia_id` and `email`.
  explicit Builder(const CoreAccountInfo& core_account_info);
  explicit Builder(const AccountInfo& account_info);

  Builder(const Builder&) = delete;
  Builder& operator=(const Builder&) = delete;

  Builder(Builder&& other) noexcept;
  Builder& operator=(Builder&& other) noexcept;

  ~Builder();

  // `Builder` will be invalid after calling this.
  AccountInfo Build();

  // Setters for CoreAccountInfo members.
  Builder& SetAccountId(const CoreAccountId& account_id);
  Builder& SetIsUnderAdvancedProtection(bool is_under_advanced_protection);

  // The following AccountInfo class members are never supposed to contain empty
  // known values. Thus, these setters do not allow setting empty values.
  //
  // To keep a value unknown, do not call a corresponding setter.
  Builder& SetFullName(std::string_view full_name);
  Builder& SetGivenName(std::string_view given_name);
  Builder& SetLastDownloadedAvatarUrlWithSize(
      std::string_view avatar_url_with_size);
  Builder& SetAvatarImage(const gfx::Image& avatar_image);
  Builder& SetLocale(std::string_view locale);

  // The following AccountInfo class members have well-defined empty values.
  //
  // To set an empty value, call a setter with an empty value.
  //
  // To keep a value unknown, do not call a corresponding setter.
  Builder& SetHostedDomain(std::string_view hosted_domain);
  Builder& SetAvatarUrl(std::string_view avatar_url);

  Builder& SetLastAuthenticationAccessPoint(
      signin_metrics::AccessPoint access_point);
  Builder& SetIsChildAccount(signin::Tribool is_child_account);

  Builder& UpdateAccountCapabilitiesWith(const AccountCapabilities& other);

 private:
  FRIEND_TEST_ALL_PREFIXES(AccountInfoTest, CreateWithPossiblyEmptyGaiaId);
  friend std::optional<AccountInfo> signin::DeserializeAccountInfo(
      const base::Value::Dict& dict);
  // Default constructor is only available to support ongoing migrations.
  // TODO(crbug.com/40268200): remove this after the migration is done.
  Builder();
  // Factory function that permits creating `AccountInfo` with an empty Gaia ID.
  // It exists only to support an ongoing migration and shouldn't be used for
  // other purposes.
  // TODO(crbug.com/40268200): remove this after the migration is done.
  static AccountInfo::Builder CreateWithPossiblyEmptyGaiaId(
      const GaiaId& gaia_id,
      std::string_view email);

  AccountInfo account_info_;
};

bool operator==(const CoreAccountInfo& l, const CoreAccountInfo& r);
std::ostream& operator<<(std::ostream& os, const CoreAccountInfo& account);

// Comparing `AccountInfo`s is likely a mistake. You should compare either
// `CoreAccountId` or `CoreAccountInfo` instead:
//
//   AccountInfo l, r;
//   // if (l == r) {
//   if (l.account_id == r.account_id) {}
//
bool operator==(const AccountInfo& l, const AccountInfo& r) = delete;
bool operator!=(const AccountInfo& l, const AccountInfo& r) = delete;

#if BUILDFLAG(IS_ANDROID)
// Constructs a Java CoreAccountInfo from the provided C++ CoreAccountInfo.
base::android::ScopedJavaLocalRef<jobject> ConvertToJavaCoreAccountInfo(
    JNIEnv* env,
    const CoreAccountInfo& account_info);

// Constructs a Java AccountInfo from the provided C++ AccountInfo.
base::android::ScopedJavaLocalRef<jobject> ConvertToJavaAccountInfo(
    JNIEnv* env,
    const AccountInfo& account_info);

// Constructs a C++ CoreAccountInfo from the provided Java CoreAccountInfo.
CoreAccountInfo ConvertFromJavaCoreAccountInfo(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& j_core_account_info);

// Constructs a C++ AccountInfo from the provided Java AccountInfo.
AccountInfo ConvertFromJavaAccountInfo(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& j_account_info);

namespace jni_zero {
template <>
inline CoreAccountInfo FromJniType<CoreAccountInfo>(
    JNIEnv* env,
    const JavaRef<jobject>& j_core_account_info) {
  return ConvertFromJavaCoreAccountInfo(env, j_core_account_info);
}

template <>
inline ScopedJavaLocalRef<jobject> ToJniType(
    JNIEnv* env,
    const CoreAccountInfo& core_account_info) {
  return ConvertToJavaCoreAccountInfo(env, core_account_info);
}

template <>
inline AccountInfo FromJniType<AccountInfo>(
    JNIEnv* env,
    const JavaRef<jobject>& j_account_info) {
  return ConvertFromJavaAccountInfo(env, j_account_info);
}

template <>
inline ScopedJavaLocalRef<jobject> ToJniType(JNIEnv* env,
                                             const AccountInfo& account_info) {
  return ConvertToJavaAccountInfo(env, account_info);
}
}  // namespace jni_zero
#endif

#endif  // COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_ACCOUNT_INFO_H_
