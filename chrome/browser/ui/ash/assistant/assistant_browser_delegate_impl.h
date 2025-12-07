// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_ASSISTANT_ASSISTANT_BROWSER_DELEGATE_IMPL_H_
#define CHROME_BROWSER_UI_ASH_ASSISTANT_ASSISTANT_BROWSER_DELEGATE_IMPL_H_

#include <memory>
#include <vector>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/types/expected.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chromeos/ash/services/assistant/public/cpp/assistant_browser_delegate.h"
#include "components/session_manager/core/session_manager_observer.h"

class Profile;

// Class to handle all Assistant in-browser-process functionalities.
class AssistantBrowserDelegateImpl
    : public ash::assistant::AssistantBrowserDelegate,
      public session_manager::SessionManagerObserver {
 public:
  AssistantBrowserDelegateImpl();
  AssistantBrowserDelegateImpl(const AssistantBrowserDelegateImpl&) = delete;
  AssistantBrowserDelegateImpl& operator=(const AssistantBrowserDelegateImpl&) =
      delete;
  ~AssistantBrowserDelegateImpl() override;

  // chromeos::assistant::AssistantBrowserDelegate overrides:
  void OpenUrl(GURL url) override;
  base::expected<bool, AssistantBrowserDelegate::Error>
  IsNewEntryPointEligibleForPrimaryProfile() override;
  void OpenNewEntryPoint() override;
  std::optional<std::string> GetNewEntryPointName() override;

  void OverrideEntryPointIdForTesting(const std::string& test_entry_point_id);

  void SetGoogleChromeBuildForTesting();

 private:
  // Gets `web_app::WebAppRegistrar` for querying information about new entry
  // point. Use a pointer instead of a reference as `base::expected` is
  // incompatible with a reference.
  base::expected<const web_app::WebAppRegistrar*,
                 ash::assistant::AssistantBrowserDelegate::Error>
  GetWebAppRegistrarForNewEntryPoint();

  // Resolves new entry point if a device or a profile is eligible. Note that
  // it's guaranteed that the value is non-nullptr if provided.
  base::expected<const web_app::WebApp*,
                 ash::assistant::AssistantBrowserDelegate::Error>
  ResolveNewEntryPointIfEligible();

  void OnExternalManagersSynchronized();

  // session_manager::SessionManagerObserver:
  void OnUserSessionStarted(bool is_primary_user) override;
  // Initializes new entry point for a passed primary profile. Note that
  // Assistant new entry point is eligible only for a primary profile.
  void InitializeNewEntryPointFor(Profile* primary_profile);

  // Stores a profile for Assistant new entry point. Note that
  // `AssistantBrowserDelegateImpl::profile_` is only initialized when Assistant
  // is allowed.
  raw_ptr<Profile> profile_for_new_entry_point_ = nullptr;

  std::string entry_point_id_for_testing_;

  bool is_google_chrome_override_for_testing_ = false;

  base::WeakPtrFactory<AssistantBrowserDelegateImpl> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_ASH_ASSISTANT_ASSISTANT_BROWSER_DELEGATE_IMPL_H_
