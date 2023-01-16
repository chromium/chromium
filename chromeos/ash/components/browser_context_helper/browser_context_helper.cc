// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"

#include "base/check.h"
#include "base/logging.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_types.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/browser_context.h"

namespace ash {
namespace {

// Chrome OS profile directories have custom prefix.
// Profile path format: [user_data_dir]/u-[$hash]
// Ex.: /home/chronos/u-0123456789
constexpr char kBrowserContextDirPrefix[] = "u-";

BrowserContextHelper* g_instance = nullptr;

bool ShouldAddBrowserContextDirPrefix(base::StringPiece user_id_hash) {
  // Do not add profile dir prefix for legacy profile dir and test
  // user profile. The reason of not adding prefix for test user profile
  // is to keep the promise that TestingProfile::kTestUserProfileDir and
  // chrome::kTestUserProfileDir are always in sync. Otherwise,
  // TestingProfile::kTestUserProfileDir needs to be dynamically calculated
  // based on whether multi profile is enabled or not.
  return user_id_hash != BrowserContextHelper::kLegacyBrowserContextDirName &&
         user_id_hash != BrowserContextHelper::kTestUserBrowserContextDirName;
}

}  // namespace

// static
const char BrowserContextHelper::kLegacyBrowserContextDirName[] = "user";

// static
const char BrowserContextHelper::kTestUserBrowserContextDirName[] = "test-user";

BrowserContextHelper::BrowserContextHelper(std::unique_ptr<Delegate> delegate)
    : delegate_(std::move(delegate)) {
  DCHECK(!g_instance);
  g_instance = this;
}

BrowserContextHelper::~BrowserContextHelper() {
  DCHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

// static
BrowserContextHelper* BrowserContextHelper::Get() {
  DCHECK(g_instance);
  return g_instance;
}

// static
std::string BrowserContextHelper::GetUserIdHashFromBrowserContext(
    content::BrowserContext* browser_context) {
  if (!browser_context)
    return std::string();

  const std::string dir = browser_context->GetPath().BaseName().value();

  // Don't strip prefix if the dir is not supposed to be prefixed.
  if (!ShouldAddBrowserContextDirPrefix(dir))
    return dir;

  if (!base::StartsWith(dir, kBrowserContextDirPrefix,
                        base::CompareCase::SENSITIVE)) {
    // This happens when creating a TestingProfile in browser_tests.
    return std::string();
  }

  return dir.substr(base::StringPiece(kBrowserContextDirPrefix).length());
}

content::BrowserContext* BrowserContextHelper::GetBrowserContextByAccountId(
    const AccountId& account_id) {
  const auto* user = user_manager::UserManager::Get()->FindUser(account_id);
  if (!user) {
    LOG(WARNING) << "Unable to retrieve user for account_id: " << account_id;
    return nullptr;
  }

  return GetBrowserContextByUser(user);
}

content::BrowserContext* BrowserContextHelper::GetBrowserContextByUser(
    const user_manager::User* user) {
  DCHECK(user);

  if (!user->is_profile_created()) {
    return nullptr;
  }

  content::BrowserContext* browser_context = delegate_->GetBrowserContextByPath(
      GetBrowserContextPathByUserIdHash(user->username_hash()));

  // GetBrowserContextByPath() returns a new instance of ProfileImpl,
  // but actually its off-the-record profile should be used.
  // TODO(hidehiko): Replace this by user->GetType() == USER_TYPE_GUEST.
  if (user_manager::UserManager::Get()->IsLoggedInAsGuest()) {
    browser_context =
        delegate_->GetOrCreatePrimaryOTRBrowserContext(browser_context);
  }

  return browser_context;
}

const user_manager::User* BrowserContextHelper::GetUserByBrowserContext(
    content::BrowserContext* browser_context) {
  if (!IsUserBrowserContext(browser_context)) {
    return nullptr;
  }

  const std::string hash = GetUserIdHashFromBrowserContext(browser_context);

  // Finds the matching user in logged-in user list since only a logged-in
  // user would have a profile.
  auto* user_manager = user_manager::UserManager::Get();
  for (const auto* user : user_manager->GetLoggedInUsers()) {
    if (user->username_hash() == hash) {
      return user;
    }
  }
  return nullptr;
}

// static
std::string BrowserContextHelper::GetUserBrowserContextDirName(
    base::StringPiece user_id_hash) {
  CHECK(!user_id_hash.empty());
  return ShouldAddBrowserContextDirPrefix(user_id_hash)
             ? base::StrCat({kBrowserContextDirPrefix, user_id_hash})
             : std::string(user_id_hash);
}

base::FilePath BrowserContextHelper::GetBrowserContextPathByUserIdHash(
    base::StringPiece user_id_hash) {
  // Fails if Chrome runs with "--login-manager", but not "--login-profile", and
  // needs to restart. This might happen if you test Chrome OS on Linux and
  // you start a guest session or Chrome crashes. Be sure to add
  //   "--login-profile=user@example.com-hash"
  // to the command line flags.
  DCHECK(!user_id_hash.empty())
      << "user_id_hash is empty, probably need to add "
         "--login-profile=user@example.com-hash to command line parameters";
  return delegate_->GetUserDataDir()->Append(
      GetUserBrowserContextDirName(user_id_hash));
}

base::FilePath BrowserContextHelper::GetSigninBrowserContextPath() const {
  return delegate_->GetUserDataDir()->Append(kSigninBrowserContextBaseName);
}

content::BrowserContext* BrowserContextHelper::GetSigninBrowserContext() {
  content::BrowserContext* browser_context =
      delegate_->GetBrowserContextByPath(GetSigninBrowserContextPath());
  if (!browser_context) {
    return nullptr;
  }
  return delegate_->GetOrCreatePrimaryOTRBrowserContext(browser_context);
}

content::BrowserContext*
BrowserContextHelper::DeprecatedGetOrCreateSigninBrowserContext() {
  content::BrowserContext* browser_context =
      delegate_->DeprecatedGetBrowserContext(GetSigninBrowserContextPath());
  if (!browser_context) {
    return nullptr;
  }
  return delegate_->GetOrCreatePrimaryOTRBrowserContext(browser_context);
}

base::FilePath BrowserContextHelper::GetLockScreenAppBrowserContextPath()
    const {
  return delegate_->GetUserDataDir()->Append(
      kLockScreenAppBrowserContextBaseName);
}

base::FilePath BrowserContextHelper::GetLockScreenBrowserContextPath() const {
  return delegate_->GetUserDataDir()->Append(kLockScreenBrowserContextBaseName);
}

content::BrowserContext* BrowserContextHelper::GetLockScreenBrowserContext() {
  content::BrowserContext* browser_context =
      delegate_->GetBrowserContextByPath(GetLockScreenBrowserContextPath());
  if (!browser_context) {
    return nullptr;
  }
  return delegate_->GetOrCreatePrimaryOTRBrowserContext(browser_context);
}

}  // namespace ash
