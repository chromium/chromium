// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"

#include "base/check.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
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

// static
const char BrowserContextHelper::kLegacyBrowserContextDirName[] = "user";

// static
const char BrowserContextHelper::kTestUserBrowserContextDirName[] = "test-user";

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

}  // namespace ash
