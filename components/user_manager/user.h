// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_MANAGER_USER_H_
#define COMPONENTS_USER_MANAGER_USER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/user_image/user_image.h"
#include "components/user_manager/user_manager_export.h"
#include "components/user_manager/user_type.h"

class PrefService;

namespace ash {
class FakeChromeUserManager;
class UserAddingScreenTest;
class UserSessionManager;
class UserImageManagerImpl;
}  // namespace ash

namespace chromeos {
class SupervisedUserManagerImpl;
}

namespace gfx {
class ImageSkia;
}

namespace policy {
class ProfilePolicyConnectorTest;
}

namespace user_manager {

class UserManagerImpl;
class FakeUserManager;

// A class representing information about a previously logged in user.
//   Each user has an |AccountId| containing canonical email (username),
// returned by |GetAccountId().GetUserEmail()| and may have a different
// displayed email (in the raw form as entered by user), returned by
// |displayed_email()|.
//   Displayed emails are for use in UI only, anywhere else users must be
// referred to by |GetAccountId()|. Internal details of AccountId should not
// be relied on unless you have special knowledge of the account type.
class USER_MANAGER_EXPORT User {
 public:
  // User OAuth token status according to the last check.
  // Please note that enum values 1 and 2 were used for OAuth1 status and are
  // deprecated now.
  typedef enum {
    OAUTH_TOKEN_STATUS_UNKNOWN = 0,
    OAUTH2_TOKEN_STATUS_INVALID = 3,
    OAUTH2_TOKEN_STATUS_VALID = 4,
  } OAuthTokenStatus;

  // Returns true if user type has gaia account.
  static bool TypeHasGaiaAccount(UserType user_type);

  // Returns true if user represents any type of the kiosk.
  static bool TypeIsKiosk(UserType user_type);

  User(const User&) = delete;
  User& operator=(const User&) = delete;

  ~User();

  std::string GetDisplayEmail() const;
  std::u16string GetDisplayName() const;
  std::u16string GetGivenName() const;
  const gfx::ImageSkia& GetImage() const;
  const AccountId& GetAccountId() const;

  // Returns the user type.
  UserType GetType() const { return type_; }

  // Will LOG(FATAL) unless overridden.
  void UpdateType(UserType new_type);

  // Returns true if user has gaia account. True for users of types
  // UserType::kRegular and UserType::kChild.
  bool HasGaiaAccount() const;

  // Returns true if user is child.
  bool IsChild() const;

  // The displayed (non-canonical) user email.
  std::string display_email() const;

  // True if the user is affiliated to the device. Returns false if the
  // affiliation is not known. Use IsAffiliatedAsync if it's possible the call
  // is done before affiliation is established.
  bool IsAffiliated() const;

  // Runs the callback immediately if the affiliation is known, otherwise later
  // when the affiliation is established.
  void IsAffiliatedAsync(base::OnceCallback<void(bool)> is_affiliated_callback);

  // True if the user is a device local account user.
  bool IsDeviceLocalAccount() const;

  // True if the user is a kiosk.
  bool IsKioskType() const;

  // Returns PrefService of the Profile corresponding this User.
  // If Profile and its PrefService is not yet ready, or it is already
  // destroyed, this API returns nullptr.
  PrefService* GetProfilePrefs() { return profile_prefs_.get(); }
  const PrefService* GetProfilePrefs() const { return profile_prefs_.get(); }

  // The displayed user name.
  std::u16string display_name() const { return display_name_; }

  // If the user has to use SAML to log in.
  bool using_saml() const { return using_saml_; }

  // Returns the account name part of the email. Use the display form of the
  // email if available and use_display_name == true. Otherwise use canonical.
  std::string GetAccountName(bool use_display_email) const;

  // True if the user's session can be locked (i.e. the user has a password with
  // which to unlock the session).
  // This depends on Profile preference, and if it's not yet ready, this
  // returns false as fallback.
  bool CanLock() const;

  int image_index() const { return image_index_; }
  bool has_image_bytes() const { return user_image_->has_image_bytes(); }
  // Returns bytes representation of static user image for WebUI.
  scoped_refptr<base::RefCountedBytes> image_bytes() const {
    return user_image_->image_bytes();
  }
  // Returns image format of the bytes representation of static user image
  // for WebUI.
  UserImage::ImageFormat image_format() const {
    return user_image_->image_format();
  }

  // Whether |user_image_| contains data in format that is considered safe to
  // decode in sensitive environment (on Login screen).
  bool image_is_safe_format() const { return user_image_->is_safe_format(); }

  // Returns the URL of user image, if there is any. Currently only the profile
  // image has a URL, for other images empty URL is returned.
  GURL image_url() const { return user_image_->url(); }

  // True if user image is a stub (while real image is being loaded from file).
  bool image_is_stub() const { return image_is_stub_; }

  // True if image is being loaded from file.
  bool image_is_loading() const { return image_is_loading_; }

  // OAuth token status for this user.
  OAuthTokenStatus oauth_token_status() const { return oauth_token_status_; }

  // Whether online authentication against GAIA should be enforced during the
  // user's next sign-in.
  bool force_online_signin() const { return force_online_signin_; }

  // Returns empty string when home dir hasn't been mounted yet.
  const std::string& username_hash() const;

  // True if current user is logged in.
  bool is_logged_in() const;

  // True if current user is active within the current session.
  bool is_active() const;

  // True if the user Profile is created.
  bool is_profile_created() const { return profile_is_created_; }

  // True if user has google account (not a guest or managed user).
  bool has_gaia_account() const;

  static User* CreatePublicAccountUserForTesting(const AccountId& account_id) {
    return CreatePublicAccountUser(account_id);
  }

  static User* CreatePublicAccountUserForTestingWithSAML(
      const AccountId& account_id) {
    return CreatePublicAccountUser(account_id, /* is_using_saml */ true);
  }

  static User* CreateRegularUserForTesting(const AccountId& account_id) {
    User* user = CreateRegularUser(account_id, UserType::kRegular);
    user->SetImage(std::make_unique<UserImage>(), 0);
    return user;
  }

  void AddProfileCreatedObserver(base::OnceClosure on_profile_created);

 protected:
  friend class UserManagerImpl;
  friend class chromeos::SupervisedUserManagerImpl;
  friend class ash::UserImageManagerImpl;
  friend class ash::UserSessionManager;

  // For testing:
  friend class FakeUserManager;
  friend class ash::FakeChromeUserManager;
  friend class ash::UserAddingScreenTest;
  friend class policy::ProfilePolicyConnectorTest;
  FRIEND_TEST_ALL_PREFIXES(UserTest, DeviceLocalAccountAffiliation);
  FRIEND_TEST_ALL_PREFIXES(UserTest, UserSessionInitialized);

  // Do not allow anyone else to create new User instances.
  static User* CreateRegularUser(const AccountId& account_id,
                                 const UserType user_type);
  static User* CreateGuestUser(const AccountId& guest_account_id);
  static User* CreateKioskAppUser(const AccountId& kiosk_app_account_id);
  static User* CreateWebKioskAppUser(const AccountId& web_kiosk_account_id);
  static User* CreateKioskIwaUser(const AccountId& kiosk_iwa_account_id);
  static User* CreatePublicAccountUser(const AccountId& account_id,
                                       bool is_using_saml = false);

  User(const AccountId& account_id, UserType type);

  const std::string* GetAccountLocale() const { return account_locale_.get(); }

  // Setters are private so only UserManager can call them.
  void SetAccountLocale(const std::string& resolved_account_locale);

  void SetImage(std::unique_ptr<UserImage> user_image, int image_index);

  void SetImageURL(const GURL& image_url);

  // Sets a stub image until the next |SetImage| call. |image_index| may be
  // one of |UserImage::Type::kExternal| or |UserImage::Type::kProfile|.
  // If |is_loading| is |true|, that means user image is being loaded from file.
  void SetStubImage(std::unique_ptr<UserImage> stub_user_image,
                    int image_index,
                    bool is_loading);

  void set_display_name(const std::u16string& display_name) {
    display_name_ = display_name;
  }

  void set_given_name(const std::u16string& given_name) {
    given_name_ = given_name;
  }

  void set_display_email(const std::string& display_email) {
    display_email_ = display_email;
  }

  void set_using_saml(const bool using_saml) { using_saml_ = using_saml; }

  const UserImage& user_image() const { return *user_image_; }

  void set_oauth_token_status(OAuthTokenStatus status) {
    oauth_token_status_ = status;
  }

  void set_force_online_signin(bool force_online_signin) {
    force_online_signin_ = force_online_signin;
  }

  void set_username_hash(const std::string& username_hash) {
    username_hash_ = username_hash;
  }

  void set_is_logged_in(bool is_logged_in) { is_logged_in_ = is_logged_in; }

  void set_is_active(bool is_active) { is_active_ = is_active; }

  void SetProfileIsCreated();

  void SetProfilePrefs(PrefService* prefs) { profile_prefs_ = prefs; }

  void SetAffiliated(bool is_affiliated);

 private:
  AccountId account_id_;
  UserType type_;
  std::u16string display_name_;
  std::u16string given_name_;
  // User email for display, which may include capitals and non-significant
  // periods. For example, "John.Steinbeck@gmail.com" is a display email, but
  // "johnsteinbeck@gmail.com" is the canonical form. Defaults to
  // account_id_.GetUserEmail().
  std::string display_email_;
  bool using_saml_ = false;
  std::unique_ptr<UserImage> user_image_;
  OAuthTokenStatus oauth_token_status_ = OAUTH_TOKEN_STATUS_UNKNOWN;
  bool force_online_signin_ = false;

  // This is set to chromeos locale if account data has been downloaded.
  // (Or failed to download, but at least one download attempt finished).
  // An empty string indicates error in data load, or in
  // translation of Account locale to chromeos locale.
  std::unique_ptr<std::string> account_locale_;

  // Used to identify homedir mount point.
  std::string username_hash_;

  // Either index of a default image for the user, |UserImage::Type::kExternal|
  // or |UserImage::Type::kProfile|.
  int image_index_ = UserImage::Type::kInvalid;

  // True if current user image is a stub set by a |SetStubImage| call.
  bool image_is_stub_ = false;

  // True if current user image is being loaded from file.
  bool image_is_loading_ = false;

  // True if user is currently logged in in current session.
  bool is_logged_in_ = false;

  // True if user is currently logged in and active in current session.
  bool is_active_ = false;

  // True if user Profile is created
  bool profile_is_created_ = false;

  // Owned by Profile.
  raw_ptr<PrefService> profile_prefs_ = nullptr;

  // True if the user is affiliated to the device.
  std::optional<bool> is_affiliated_;

  std::vector<base::OnceClosure> on_profile_created_observers_;
  std::vector<base::OnceCallback<void(bool is_affiliated)>>
      on_affiliation_set_callbacks_;
};

// List of known users.
using UserList = std::vector<raw_ptr<User, VectorExperimental>>;

}  // namespace user_manager

#endif  // COMPONENTS_USER_MANAGER_USER_H_
