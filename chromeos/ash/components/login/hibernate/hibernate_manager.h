// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_LOGIN_HIBERNATE_HIBERNATE_MANAGER_H_
#define CHROMEOS_ASH_COMPONENTS_LOGIN_HIBERNATE_HIBERNATE_MANAGER_H_

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

  base::WeakPtr<HibernateManager> AsWeakPtr();

  // Resume from hibernate, in the form of an AuthOperation.
  void PrepareHibernateAndMaybeResumeAuthOp(
      std::unique_ptr<UserContext> user_context,
      AuthOperationCallback callback);

  // Resume from hibernate. On a successful resume from hibernation, this never
  // returns. On failure, or if no hibernate image is available to resume to,
  // calls the callback.
  void PrepareHibernateAndMaybeResume(std::unique_ptr<UserContext> user_context,
                                      HibernateResumeCallback callback);

  // Determines if hibernate is supported on this platform.
  static void InitializePlatformSupport();

  // Determines if the system has AESKL.
  static bool HasAESKL();

  // Determines if hibernate is supported.
  static bool IsHibernateSupported();

 private:
  void ResumeFromHibernateAuthOpCallback(
      AuthOperationCallback callback,
      std::unique_ptr<UserContext> user_context,
      bool resume_call_successful);

  base::WeakPtrFactory<HibernateManager> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_LOGIN_HIBERNATE_HIBERNATE_MANAGER_H_
