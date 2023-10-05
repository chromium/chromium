// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_LOGIN_HIBERNATE_HIBERNATE_MANAGER_H_
#define CHROMEOS_ASH_COMPONENTS_LOGIN_HIBERNATE_HIBERNATE_MANAGER_H_

#include <set>
#include <string>

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/login/auth/public/auth_callbacks.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"

namespace ash {

using HibernateResumeCallback =
    base::OnceCallback<void(std::unique_ptr<UserContext> user_context, bool)>;

// HibernateManager is used to initiate resume from hibernation.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_LOGIN_HIBERNATE)
    HibernateManager {
 public:
  HibernateManager();

  // Not copyable or movable.
  HibernateManager(const HibernateManager&) = delete;
  HibernateManager& operator=(const HibernateManager&) = delete;

  ~HibernateManager();

  // Returns the HibernateManager singleton.
  static HibernateManager* Get();

  // Determines if hibernate is supported on this platform.
  static void InitializePlatformSupport();

  // Determines if the system has AESKL.
  static bool HasAESKL();

  // Determines if hibernate is supported.
  static bool IsHibernateSupported();

  // Set auth info. During the login flow we save the account and auth session
  // id to pass to hiberman after the users profile has been created.
  void SetAuthInfo(const std::string& account_id,
                   const std::string& auth_session_id);

  // Once the user's profile has been created and preferences loaded we will try
  // to resume. The reason this is necessary is because if a user has overridden
  // their suspend-to-disk settings via chrome://flags it would still be too
  // early to learn this. So HibernateManager will inspect the preferences for
  // this unique situation.
  void MaybeResume(const std::set<std::string>& user_prefs);

 private:
  std::string account_id_;
  std::string auth_session_id_;

  void ResumeFromHibernateAuthOpCallback(
      AuthOperationCallback callback,
      std::unique_ptr<UserContext> user_context,
      bool resume_call_successful);
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_LOGIN_HIBERNATE_HIBERNATE_MANAGER_H_
