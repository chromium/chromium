// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_manager/user.h"

#include <stddef.h>

#include "base/callback.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/known_user.h"
#include "components/user_manager/user_manager.h"
#include "google_apis/gaia/gaia_auth_util.h"

namespace user_manager {

namespace {

// Must be in sync with histogram enum UserTypeChanged in enums.xml.
// The values must never be changed (only new ones can be added) as they
// are stored in UMA logs.
enum class UserTypeChangeHistogram {
  UNKNOWN_FATAL = 0,
  REGULAR_TO_CHILD = 1,
  CHILD_TO_REGULAR = 2,
  COUNT,  // Not a value, just a count of other values.
};
void UMAUserTypeChanged(const UserTypeChangeHistogram value) {
  UMA_HISTOGRAM_ENUMERATION("UserManager.UserTypeChanged", value,
                            UserTypeChangeHistogram::COUNT);
}

// Returns account name portion of an email.
std::string GetUserName(const std::string& email) {
  std::string::size_type i = email.find('@');
  if (i == 0 || i == std::string::npos) {
    return email;
  }
  return email.substr(0, i);
}

}  // namespace

// static
bool User::TypeHasGaiaAccount(UserType user_type) {
  return user_type == USER_TYPE_REGULAR ||
         user_type == USER_TYPE_CHILD;
}

// Also used for regular supervised users.
class RegularUser : public User {
 public:
  RegularUser(const AccountId& account_id, const UserType user_type);
  ~RegularUser() override;

  // Overridden from User:
  UserType GetType() const override;
  void UpdateType(UserType user_type) override;
  bool CanSyncImage() const override;

 private:
  bool is_child_;

  DISALLOW_COPY_AND_ASSIGN(RegularUser);
};

class ActiveDirectoryUser : public RegularUser {
 public:
  explicit ActiveDirectoryUser(const AccountId& account_id);
  ~ActiveDirectoryUser() override;
  // Overridden from User:
  UserType GetType() const override;
  bool CanSyncImage() const override;
};

class GuestUser : public User {
 public:
  explicit GuestUser(const AccountId& guest_account_id);
  ~GuestUser() override;

  // Overridden from User:
  UserType GetType() const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(GuestUser);
};

class DeviceLocalAccountUserBase : public User {
 public:
  // User:
  bool IsAffiliated() const override;

 protected:
  explicit DeviceLocalAccountUserBase(const AccountId& account_id);
  ~DeviceLocalAccountUserBase() override;
  // User:
  void SetAffiliation(bool) override;
  bool IsDeviceLocalAccount() const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(DeviceLocalAccountUserBase);
};

class KioskAppUser : public DeviceLocalAccountUserBase {
 public:
  explicit KioskAppUser(const AccountId& kiosk_app_account_id);
  ~KioskAppUser() override;

  // Overridden from User:
  UserType GetType() const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(KioskAppUser);
};

class ArcKioskAppUser : public DeviceLocalAccountUserBase {
 public:
  explicit ArcKioskAppUser(const AccountId& arc_kiosk_account_id);
  ~ArcKioskAppUser() override;

  // Overridden from User:
  UserType GetType() const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ArcKioskAppUser);
};

class WebKioskAppUser : public DeviceLocalAccountUserBase {
 public:
  explicit WebKioskAppUser(const AccountId& web_kiosk_account_id);
  ~WebKioskAppUser() override;

  // Overridden from User:
  UserType GetType() const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(WebKioskAppUser);
};

class SupervisedUser : public User {
 public:
  explicit SupervisedUser(const AccountId& account_id);
  ~SupervisedUser() override;

  // Overridden from User:
  UserType GetType() const override;
  std::string display_email() const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(SupervisedUser);
};

class PublicAccountUser : public DeviceLocalAccountUserBase {
 public:
  explicit PublicAccountUser(const AccountId& account_id);
  ~PublicAccountUser() override;

  // Overridden from User:
  UserType GetType() const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(PublicAccountUser);
};

User::User(const AccountId& account_id)
    : account_id_(account_id), user_image_(new UserImage) {}

User::~User() {}

std::string User::GetDisplayEmail() const {
  return display_email();
}

base::string16 User::GetDisplayName() const {
  // Fallback to the email account name in case display name haven't been set.
  return display_name_.empty() ? base::UTF8ToUTF16(GetAccountName(true))
                               : display_name_;
}

base::string16 User::GetGivenName() const {
  return given_name_;
}

const gfx::ImageSkia& User::GetImage() const {
  return user_image_->image();
}

const AccountId& User::GetAccountId() const {
  return account_id_;
}

void User::UpdateType(UserType user_type) {
  UMAUserTypeChanged(UserTypeChangeHistogram::UNKNOWN_FATAL);
  LOG(FATAL) << "Unsupported user type change " << GetType() << "=>"
             << user_type;
}

bool User::HasGaiaAccount() const {
  return TypeHasGaiaAccount(GetType());
}

bool User::IsActiveDirectoryUser() const {
  return GetType() == user_manager::USER_TYPE_ACTIVE_DIRECTORY;
}

bool User::IsSupervised() const {
  UserType type = GetType();
  return  type == USER_TYPE_SUPERVISED ||
          type == USER_TYPE_CHILD;
}

bool User::IsChild() const {
  return GetType() == USER_TYPE_CHILD;
}

std::string User::GetAccountName(bool use_display_email) const {
  if (use_display_email && !display_email_.empty())
    return GetUserName(display_email_);
  else
    return GetUserName(account_id_.GetUserEmail());
}

bool User::HasDefaultImage() const {
  return UserManager::Get()->IsValidDefaultUserImageId(image_index_);
}

bool User::CanSyncImage() const {
  return false;
}

std::string User::display_email() const {
  return display_email_;
}

bool User::can_lock() const {
  return can_lock_;
}

std::string User::username_hash() const {
  return username_hash_;
}

bool User::is_logged_in() const {
  return is_logged_in_;
}

bool User::is_active() const {
  return is_active_;
}

bool User::has_gaia_account() const {
  static_assert(user_manager::NUM_USER_TYPES == 10,
                "NUM_USER_TYPES should equal 10");
  switch (GetType()) {
    case user_manager::USER_TYPE_REGULAR:
    case user_manager::USER_TYPE_CHILD:
      return true;
    case user_manager::USER_TYPE_GUEST:
    case user_manager::USER_TYPE_PUBLIC_ACCOUNT:
    case user_manager::USER_TYPE_SUPERVISED:
    case user_manager::USER_TYPE_KIOSK_APP:
    case user_manager::USER_TYPE_ARC_KIOSK_APP:
    case user_manager::USER_TYPE_ACTIVE_DIRECTORY:
    case user_manager::USER_TYPE_WEB_KIOSK_APP:
      return false;
    default:
      NOTREACHED();
  }
  return false;
}

void User::AddProfileCreatedObserver(base::OnceClosure on_profile_created) {
  if (profile_is_created_)
    std::move(on_profile_created).Run();
  else
    on_profile_created_observers_.push_back(std::move(on_profile_created));
}

bool User::IsAffiliated() const {
  return is_affiliated_;
}

void User::SetProfileIsCreated() {
  profile_is_created_ = true;
  for (auto& callback : on_profile_created_observers_)
    std::move(callback).Run();
  on_profile_created_observers_.clear();
}

void User::SetAffiliation(bool is_affiliated) {
  is_affiliated_ = is_affiliated;
}

bool User::IsDeviceLocalAccount() const {
  return false;
}

bool User::IsKioskType() const {
  UserType type = GetType();
  return type == USER_TYPE_KIOSK_APP || type == USER_TYPE_ARC_KIOSK_APP ||
         type == USER_TYPE_WEB_KIOSK_APP;
}

User* User::CreateRegularUser(const AccountId& account_id,
                              const UserType user_type) {
  if (account_id.GetAccountType() == AccountType::ACTIVE_DIRECTORY)
    return new ActiveDirectoryUser(account_id);
  return new RegularUser(account_id, user_type);
}

User* User::CreateGuestUser(const AccountId& guest_account_id) {
  return new GuestUser(guest_account_id);
}

User* User::CreateKioskAppUser(const AccountId& kiosk_app_account_id) {
  return new KioskAppUser(kiosk_app_account_id);
}

User* User::CreateArcKioskAppUser(const AccountId& arc_kiosk_account_id) {
  return new ArcKioskAppUser(arc_kiosk_account_id);
}

User* User::CreateWebKioskAppUser(const AccountId& web_kiosk_account_id) {
  return new WebKioskAppUser(web_kiosk_account_id);
}

User* User::CreateSupervisedUser(const AccountId& account_id) {
  return new SupervisedUser(account_id);
}

User* User::CreatePublicAccountUser(const AccountId& account_id,
                                    bool is_using_saml) {
  User* user = new PublicAccountUser(account_id);
  user->set_using_saml(is_using_saml);
  return user;
}

void User::SetAccountLocale(const std::string& resolved_account_locale) {
  account_locale_.reset(new std::string(resolved_account_locale));
}

void User::SetImage(std::unique_ptr<UserImage> user_image, int image_index) {
  user_image_ = std::move(user_image);
  image_index_ = image_index;
  image_is_stub_ = false;
  image_is_loading_ = false;
  DCHECK(HasDefaultImage() || user_image_->has_image_bytes());
}

void User::SetImageURL(const GURL& image_url) {
  user_image_->set_url(image_url);
}

void User::SetStubImage(std::unique_ptr<UserImage> stub_user_image,
                        int image_index,
                        bool is_loading) {
  user_image_ = std::move(stub_user_image);
  image_index_ = image_index;
  image_is_stub_ = true;
  image_is_loading_ = is_loading;
}

UserType ActiveDirectoryUser::GetType() const {
  return user_manager::USER_TYPE_ACTIVE_DIRECTORY;
}

bool ActiveDirectoryUser::CanSyncImage() const {
  return false;
}

RegularUser::RegularUser(const AccountId& account_id, const UserType user_type)
    : User(account_id), is_child_(user_type == USER_TYPE_CHILD) {
  if (user_type != USER_TYPE_CHILD && user_type != USER_TYPE_REGULAR &&
      user_type != USER_TYPE_ACTIVE_DIRECTORY) {
    LOG(FATAL) << "Invalid user type " << user_type;
  }

  set_can_lock(true);
  set_display_email(account_id.GetUserEmail());
}

ActiveDirectoryUser::ActiveDirectoryUser(const AccountId& account_id)
    : RegularUser(account_id, user_manager::USER_TYPE_ACTIVE_DIRECTORY) {}

RegularUser::~RegularUser() {
}

ActiveDirectoryUser::~ActiveDirectoryUser() {}

UserType RegularUser::GetType() const {
  return is_child_ ? user_manager::USER_TYPE_CHILD :
                     user_manager::USER_TYPE_REGULAR;
}

void RegularUser::UpdateType(UserType user_type) {
  const UserType current_type = GetType();
  // Can only change between regular and child.
  if ((user_type == user_manager::USER_TYPE_CHILD ||
       user_type == user_manager::USER_TYPE_REGULAR) &&
      (current_type == user_manager::USER_TYPE_CHILD ||
       current_type == user_manager::USER_TYPE_REGULAR)) {
    // We want all the other type changes to crash, that is why this check is
    // not at the top level.
    if (user_type == current_type)
      return;
    const bool old_is_child = is_child_;
    is_child_ = user_type == user_manager::USER_TYPE_CHILD;

    // Clear information about profile policy requirements to enforce setting it
    // again for the new account type.
    user_manager::known_user::ClearProfileRequiresPolicy(GetAccountId());

    LOG(WARNING) << "User type has changed: " << current_type
                 << " (is_child=" << old_is_child << ") => " << user_type
                 << " (is_child=" << is_child_ << ")";
    UMAUserTypeChanged(is_child_ ? UserTypeChangeHistogram::REGULAR_TO_CHILD
                                 : UserTypeChangeHistogram::CHILD_TO_REGULAR);
    return;
  }
  // Fail with LOG(FATAL).
  User::UpdateType(user_type);
}

bool RegularUser::CanSyncImage() const {
  return true;
}

GuestUser::GuestUser(const AccountId& guest_account_id)
    : User(guest_account_id) {
  set_display_email(std::string());
}

GuestUser::~GuestUser() {
}

UserType GuestUser::GetType() const {
  return user_manager::USER_TYPE_GUEST;
}

DeviceLocalAccountUserBase::DeviceLocalAccountUserBase(
    const AccountId& account_id) : User(account_id) {
}

DeviceLocalAccountUserBase::~DeviceLocalAccountUserBase() {
}

bool DeviceLocalAccountUserBase::IsAffiliated() const {
  return true;
}

void DeviceLocalAccountUserBase::SetAffiliation(bool) {
  // Device local accounts are always affiliated. No affiliation modification
  // must happen.
  NOTREACHED();
}

bool DeviceLocalAccountUserBase::IsDeviceLocalAccount() const {
  return true;
}

KioskAppUser::KioskAppUser(const AccountId& kiosk_app_account_id)
    : DeviceLocalAccountUserBase(kiosk_app_account_id) {
  set_display_email(kiosk_app_account_id.GetUserEmail());
}

KioskAppUser::~KioskAppUser() {
}

UserType KioskAppUser::GetType() const {
  return user_manager::USER_TYPE_KIOSK_APP;
}

ArcKioskAppUser::ArcKioskAppUser(const AccountId& arc_kiosk_account_id)
    : DeviceLocalAccountUserBase(arc_kiosk_account_id) {
  set_display_email(arc_kiosk_account_id.GetUserEmail());
}

ArcKioskAppUser::~ArcKioskAppUser() {
}

UserType ArcKioskAppUser::GetType() const {
  return user_manager::USER_TYPE_ARC_KIOSK_APP;
}

WebKioskAppUser::WebKioskAppUser(const AccountId& web_kiosk_account_id)
    : DeviceLocalAccountUserBase(web_kiosk_account_id) {
  set_display_email(web_kiosk_account_id.GetUserEmail());
}

WebKioskAppUser::~WebKioskAppUser() {}

UserType WebKioskAppUser::GetType() const {
  return user_manager::USER_TYPE_WEB_KIOSK_APP;
}

SupervisedUser::SupervisedUser(const AccountId& account_id) : User(account_id) {
  set_can_lock(true);
}

SupervisedUser::~SupervisedUser() {
}

UserType SupervisedUser::GetType() const {
  return user_manager::USER_TYPE_SUPERVISED;
}

std::string SupervisedUser::display_email() const {
  return base::UTF16ToUTF8(display_name());
}

PublicAccountUser::PublicAccountUser(const AccountId& account_id)
    : DeviceLocalAccountUserBase(account_id) {
  // Public accounts do not have a real email address, so they do not set
  // |display_email_|.
}

PublicAccountUser::~PublicAccountUser() {
}

UserType PublicAccountUser::GetType() const {
  return user_manager::USER_TYPE_PUBLIC_ACCOUNT;
}

}  // namespace user_manager
