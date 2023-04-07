// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_ACCOUNT_INFO_H_
#define COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_ACCOUNT_INFO_H_

#include <string>

#include "build/build_config.h"
#include "components/signin/public/identity_manager/account_capabilities.h"
#include "components/signin/public/identity_manager/tribool.h"
#include "google_apis/gaia/core_account_id.h"
#include "ui/gfx/image/image.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/scoped_java_ref.h"
#endif

// Value representing no hosted domain associated with an account.
extern const char kNoHostedDomainFound[];

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
  std::string gaia;
  std::string email;

  bool is_under_advanced_protection = false;

  // Returns true if all fields in the account info are empty.
  bool IsEmpty() const;
};

// Stores all the information known about an account. Part of the information
// may only become available asynchronously.
struct AccountInfo : public CoreAccountInfo {
  AccountInfo();
  ~AccountInfo();

  AccountInfo(const AccountInfo& other);
  AccountInfo(AccountInfo&& other) noexcept;

  AccountInfo& operator=(const AccountInfo& other);
  AccountInfo& operator=(AccountInfo&& other) noexcept;

  std::string full_name;
  std::string given_name;
  std::string hosted_domain;
  std::string locale;
  std::string picture_url;
  std::string last_downloaded_image_url_with_size;
  gfx::Image account_image;

  AccountCapabilities capabilities;
  signin::Tribool is_child_account = signin::Tribool::kUnknown;

  // Returns true if all fields in the account info are empty.
  bool IsEmpty() const;

  // Returns true if all non-optional fields in this account info are filled.
  // Note: IsValid() does not check if `is_child_account` or `capabilities` are
  // filled.
  bool IsValid() const;

  // Updates the empty fields of |this| with |other|. Returns whether at least
  // one field was updated.
  bool UpdateWith(const AccountInfo& other);

  // Helper functions returning whether the account is managed (hosted_domain
  // is different from kNoHostedDomainFound). Returns false for gmail.com
  // accounts and other non-managed accounts like yahoo.com. Returns false if
  // hosted_domain is still unknown (empty), this information will become
  // available asynchronously.
  static bool IsManaged(const std::string& hosted_domain);

  // Returns true if the account has no hosted domain but is a dasher account.
  bool IsMemberOfFlexOrg() const;

  bool IsManaged() const;
};

bool operator==(const CoreAccountInfo& l, const CoreAccountInfo& r);
bool operator!=(const CoreAccountInfo& l, const CoreAccountInfo& r);
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
// Constructs a Java CoreAccountInfo from the provided C++ CoreAccountInfo
base::android::ScopedJavaLocalRef<jobject> ConvertToJavaCoreAccountInfo(
    JNIEnv* env,
    const CoreAccountInfo& account_info);

// Constructs a Java AccountInfo from the provided C++ AccountInfo
base::android::ScopedJavaLocalRef<jobject> ConvertToJavaAccountInfo(
    JNIEnv* env,
    const AccountInfo& account_info);

// Constructs a Java CoreAccountId from the provided C++ CoreAccountId
base::android::ScopedJavaLocalRef<jobject> ConvertToJavaCoreAccountId(
    JNIEnv* env,
    const CoreAccountId& account_id);

// Constructs a C++ CoreAccountInfo from the provided Java CoreAccountInfo
CoreAccountInfo ConvertFromJavaCoreAccountInfo(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& j_core_account_info);

// Constructs a C++ CoreAccountId from the provided Java CoreAccountId
CoreAccountId ConvertFromJavaCoreAccountId(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& j_core_account_id);
#endif

#endif  // COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_ACCOUNT_INFO_H_
