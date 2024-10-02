// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIGNIN_MANAGED_USER_PROFILE_NOTICE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SIGNIN_MANAGED_USER_PROFILE_NOTICE_HANDLER_H_

#include <optional>
#include <string>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/timer/timer.h"
#include "base/values.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/webui/signin/managed_user_profile_notice_ui.h"
#include "chrome/browser/ui/webui/signin/signin_utils.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "google_apis/gaia/core_account_id.h"

class Browser;
class Profile;
struct AccountInfo;

namespace base {
class FilePath;
}

// WebUI message handler for the managed user notice screen for profiles in the
// profile creation flow.
class ManagedUserProfileNoticeHandler
    : public content::WebUIMessageHandler,
      public ProfileAttributesStorage::Observer,
      public BrowserListObserver,
      public signin::IdentityManager::Observer {
 public:
  enum State {
    kDisclosure = 0,
    kProcessing = 1,
    kSuccess = 2,
    kTimeout = 3,
    kError = 4,
    kValueProposition = 5,
    kUserDataHandling = 6,
  };
  ManagedUserProfileNoticeHandler(
      Browser* browser,
      ManagedUserProfileNoticeUI::ScreenType type,
      std::unique_ptr<signin::EnterpriseProfileCreationDialogParams>
          create_param);
  ~ManagedUserProfileNoticeHandler() override;

  ManagedUserProfileNoticeHandler(const ManagedUserProfileNoticeHandler&) =
      delete;
  ManagedUserProfileNoticeHandler& operator=(
      const ManagedUserProfileNoticeHandler&) = delete;

  // content::WebUIMessageHandler:
  void RegisterMessages() override;
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

  // ProfileAttributesStorage::Observer:
  void OnProfileAvatarChanged(const base::FilePath& profile_path) override;
  void OnProfileHighResAvatarLoaded(
      const base::FilePath& profile_path) override;
  void OnProfileHostedDomainChanged(
      const base::FilePath& profile_path) override;

  // BrowserListObserver:
  void OnBrowserRemoved(Browser* browser) override;

  // signin::IdentityManager::Observer:
  void OnExtendedAccountInfoUpdated(const AccountInfo& info) override;

  // Access to construction parameters for tests.
  ManagedUserProfileNoticeUI::ScreenType GetTypeForTesting();
  void CallProceedCallbackForTesting(signin::SigninChoice choice);
  void set_web_ui_for_test(content::WebUI* web_ui) { set_web_ui(web_ui); }

 private:
  FRIEND_TEST_ALL_PREFIXES(
      ManagedUserProfileNoticeHandlerTest,
      GetManagedAccountTitleWithEmailInterceptionEnforcedAtMachineLevel);
  FRIEND_TEST_ALL_PREFIXES(
      ManagedUserProfileNoticeHandlerTest,
      GetManagedAccountTitleWithEmailInterceptionEnforcedByExistingProfile);
  FRIEND_TEST_ALL_PREFIXES(
      ManagedUserProfileNoticeHandlerTest,
      GetManagedAccountTitleWithEmailInterceptionEnforcedByInterceptedAccount);

  void HandleInitialized(const base::Value::List& args);
  // Handles the web ui message sent when the html content is done being laid
  // out and it's time to resize the native view hosting it to fit. |args| is
  // a single integer value for the height the native view should resize to.
  void HandleInitializedWithSize(const base::Value::List& args);
  void HandleProceed(const base::Value::List& args);
  void HandleCancel(const base::Value::List& args);

  void OnLongProcessingTime();

  // Sends an updated profile info (avatar and strings) to the WebUI.
  // `profile_path` is the path of the profile being updated, this function does
  // nothing if the profile path does not match the current profile.
  void UpdateProfileInfo(const base::FilePath& profile_path);

  // Returns a string stating the management status.
  static std::string GetManagedAccountTitleWithEmail(
      Profile* profile,
      ProfileAttributesEntry* entry,
      const std::string& account_domain_name,
      const std::u16string& email);

  // Computes the profile info (avatar and strings) to be sent to the WebUI.
  base::Value::Dict GetProfileInfoValue();

  // Returns the ProfilesAttributesEntry associated with the current profile.
  ProfileAttributesEntry* GetProfileEntry() const;

  std::string GetPictureUrl();
  void OnUserChoiceHandled(signin::SigninChoiceOperationResult result);

  base::FilePath profile_path_;
  base::ScopedObservation<ProfileAttributesStorage,
                          ProfileAttributesStorage::Observer>
      observed_profile_{this};

  base::ScopedObservation<signin::IdentityManager,
                          signin::IdentityManager::Observer>
      observed_account_{this};

  base::OneShotTimer processing_timer_;

  raw_ptr<Browser> browser_ = nullptr;
  const ManagedUserProfileNoticeUI::ScreenType type_;
  const bool profile_creation_required_by_policy_;
#if !BUILDFLAG(IS_CHROMEOS)
  const bool show_link_data_option_;
#endif
  const std::u16string email_;
  const std::string domain_name_;
  const CoreAccountId account_id_;
  signin::SigninChoiceWithConfirmationCallback
      process_user_choice_with_confirmation_callback_;
  base::OnceClosure done_callback_;
  base::OnceClosure retry_callback_;
  bool canceling_ = false;
  base::WeakPtrFactory<ManagedUserProfileNoticeHandler> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_SIGNIN_MANAGED_USER_PROFILE_NOTICE_HANDLER_H_
