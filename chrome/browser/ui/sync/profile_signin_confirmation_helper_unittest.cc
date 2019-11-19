// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/sync/profile_signin_confirmation_helper.h"

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/test/base/testing_profile.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "components/history/core/browser/history_service.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_notifier_impl.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "extensions/buildflags/buildflags.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/login/users/scoped_test_user_manager.h"
#include "chrome/browser/chromeos/settings/scoped_cros_settings_test_helper.h"
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/common/extensions/extension_constants.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/permissions/permission_set.h"
#endif

using bookmarks::BookmarkModel;

namespace {

template<typename T>
void GetValueAndQuit(T* result, const base::Closure& quit, T actual) {
  *result = actual;
  quit.Run();
}

template <typename T>
T GetCallbackResult(
    const base::Callback<void(base::OnceCallback<void(T)>)>& callback) {
  T result = false;
  base::RunLoop loop;
  callback.Run(
      base::BindOnce(&GetValueAndQuit<T>, &result, loop.QuitClosure()));
  loop.Run();
  return result;
}

// A pref store that can have its read_error property changed for testing.
class TestingPrefStoreWithCustomReadError : public TestingPrefStore {
 public:
  TestingPrefStoreWithCustomReadError()
      : read_error_(PersistentPrefStore::PREF_READ_ERROR_NO_FILE) {
    // By default the profile is "new" (NO_FILE means that the profile
    // wasn't found on disk, so it was created).
  }
  PrefReadError GetReadError() const override { return read_error_; }
  bool IsInitializationComplete() const override { return true; }
  void set_read_error(PrefReadError read_error) {
    read_error_ = read_error;
  }
 private:
  ~TestingPrefStoreWithCustomReadError() override {}
  PrefReadError read_error_;
};

#if BUILDFLAG(ENABLE_EXTENSIONS)
#if defined(OS_WIN)
const base::FilePath::CharType kExtensionFilePath[] =
    FILE_PATH_LITERAL("c:\\foo");
#elif defined(OS_POSIX)
const base::FilePath::CharType kExtensionFilePath[] =
    FILE_PATH_LITERAL("/oo");
#endif

static scoped_refptr<extensions::Extension> CreateExtension(
    const std::string& name,
    const std::string& id,
    extensions::Manifest::Location location) {
  base::DictionaryValue manifest;
  manifest.SetString(extensions::manifest_keys::kVersion, "1.0.0.0");
  manifest.SetInteger(extensions::manifest_keys::kManifestVersion, 2);
  manifest.SetString(extensions::manifest_keys::kName, name);
  std::string error;
  scoped_refptr<extensions::Extension> extension =
    extensions::Extension::Create(
        base::FilePath(kExtensionFilePath).AppendASCII(name),
        location,
        manifest,
        extensions::Extension::NO_FLAGS,
        id,
        &error);
  return extension;
}
#endif

}  // namespace

class ProfileSigninConfirmationHelperTest : public testing::Test {
 public:
  ProfileSigninConfirmationHelperTest()
      : user_prefs_(NULL),
        model_(NULL) {
  }

  void SetUp() override {
    // Create the profile.
    TestingProfile::Builder builder;
    user_prefs_ = new TestingPrefStoreWithCustomReadError;
    sync_preferences::TestingPrefServiceSyncable* pref_service =
        new sync_preferences::TestingPrefServiceSyncable(
            new TestingPrefStore(), new TestingPrefStore(), user_prefs_,
            new TestingPrefStore(), new user_prefs::PrefRegistrySyncable(),
            new PrefNotifierImpl());
    RegisterUserProfilePrefs(pref_service->registry());
    builder.SetPrefService(
        base::WrapUnique<sync_preferences::PrefServiceSyncable>(pref_service));
    profile_ = builder.Build();

    // Initialize the services we check.
    profile_->CreateBookmarkModel(true);
    model_ = BookmarkModelFactory::GetForBrowserContext(profile_.get());
    bookmarks::test::WaitForBookmarkModelToLoad(model_);
    ASSERT_TRUE(profile_->CreateHistoryService(true, false));
#if BUILDFLAG(ENABLE_EXTENSIONS)
    extensions::TestExtensionSystem* system =
        static_cast<extensions::TestExtensionSystem*>(
            extensions::ExtensionSystem::Get(profile_.get()));
    base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
    system->CreateExtensionService(&command_line,
                                   base::FilePath(kExtensionFilePath),
                                   false);
#endif
  }

  void TearDown() override {
    // TestExtensionSystem uses DeleteSoon, so we need to delete the profile
    // and then run the message queue to clean up.
    profile_.reset();
    base::RunLoop().RunUntilIdle();
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  TestingPrefStoreWithCustomReadError* user_prefs_;
  BookmarkModel* model_;

#if defined OS_CHROMEOS
  chromeos::ScopedCrosSettingsTestHelper cros_settings_test_helper_;
  chromeos::ScopedTestUserManager test_user_manager_;
#endif
};

// http://crbug.com/393149
TEST_F(ProfileSigninConfirmationHelperTest, DISABLED_DoNotPromptForNewProfile) {
  // Profile is new and there's no profile data.
  profile_->SetIsNewProfile(true);
  EXPECT_FALSE(
      GetCallbackResult(
          base::Bind(
              &ui::CheckShouldPromptForNewProfile,
              profile_.get())));
}

TEST_F(ProfileSigninConfirmationHelperTest, PromptForNewProfile_Bookmarks) {
  ASSERT_TRUE(model_);

  // Profile is new but has bookmarks.
  profile_->SetIsNewProfile(true);
  model_->AddURL(model_->bookmark_bar_node(), 0,
                 base::string16(base::ASCIIToUTF16("foo")),
                 GURL("http://foo.com"));
  EXPECT_TRUE(
      GetCallbackResult(
          base::Bind(
              &ui::CheckShouldPromptForNewProfile,
              profile_.get())));
}

#if BUILDFLAG(ENABLE_EXTENSIONS)
TEST_F(ProfileSigninConfirmationHelperTest, PromptForNewProfile_Extensions) {
  extensions::ExtensionService* extensions =
      extensions::ExtensionSystem::Get(profile_.get())->extension_service();
  ASSERT_TRUE(extensions);

  // Profile is new but has synced extensions (The web store doesn't count).
  profile_->SetIsNewProfile(true);
  scoped_refptr<extensions::Extension> webstore =
      CreateExtension("web store",
                      extensions::kWebStoreAppId,
                      extensions::Manifest::COMPONENT);
  extensions::ExtensionPrefs::Get(profile_.get())
      ->AddGrantedPermissions(webstore->id(), extensions::PermissionSet());
  extensions->AddExtension(webstore.get());
  EXPECT_FALSE(GetCallbackResult(
      base::Bind(&ui::CheckShouldPromptForNewProfile, profile_.get())));

  scoped_refptr<extensions::Extension> extension =
      CreateExtension("foo", std::string(), extensions::Manifest::INTERNAL);
  extensions::ExtensionPrefs::Get(profile_.get())
      ->AddGrantedPermissions(extension->id(), extensions::PermissionSet());
  extensions->AddExtension(extension.get());
  EXPECT_TRUE(GetCallbackResult(
      base::Bind(&ui::CheckShouldPromptForNewProfile, profile_.get())));
}
#endif

// http://crbug.com/393149
TEST_F(ProfileSigninConfirmationHelperTest,
       DISABLED_PromptForNewProfile_History) {
  history::HistoryService* history = HistoryServiceFactory::GetForProfile(
      profile_.get(), ServiceAccessType::EXPLICIT_ACCESS);
  ASSERT_TRUE(history);

  // Profile is new but has more than $(kHistoryEntriesBeforeNewProfilePrompt)
  // history items.
  profile_->SetIsNewProfile(true);
  char buf[18];
  for (int i = 0; i < 10; i++) {
    base::snprintf(buf, base::size(buf), "http://foo.com/%d", i);
    history->AddPage(
        GURL(std::string(buf)), base::Time::Now(), NULL, 1,
        GURL(), history::RedirectList(), ui::PAGE_TRANSITION_LINK,
        history::SOURCE_BROWSED, false);
  }
  EXPECT_TRUE(
      GetCallbackResult(
          base::Bind(
              &ui::CheckShouldPromptForNewProfile,
              profile_.get())));
}

// http://crbug.com/393149
TEST_F(ProfileSigninConfirmationHelperTest,
       DISABLED_PromptForNewProfile_TypedURLs) {
  history::HistoryService* history = HistoryServiceFactory::GetForProfile(
      profile_.get(), ServiceAccessType::EXPLICIT_ACCESS);
  ASSERT_TRUE(history);

  // Profile is new but has a typed URL.
  profile_->SetIsNewProfile(true);
  history->AddPage(
      GURL("http://example.com"), base::Time::Now(), NULL, 1,
      GURL(), history::RedirectList(), ui::PAGE_TRANSITION_TYPED,
      history::SOURCE_BROWSED, false);
  EXPECT_TRUE(
      GetCallbackResult(
          base::Bind(
              &ui::CheckShouldPromptForNewProfile,
              profile_.get())));
}

TEST_F(ProfileSigninConfirmationHelperTest, PromptForNewProfile_Restarted) {
  // Browser has been shut down since profile was created.
  profile_->SetIsNewProfile(false);
  EXPECT_TRUE(
      GetCallbackResult(
          base::Bind(
              &ui::CheckShouldPromptForNewProfile,
              profile_.get())));
}
