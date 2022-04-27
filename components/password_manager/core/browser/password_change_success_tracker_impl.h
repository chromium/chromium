// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_CHANGE_SUCCESS_TRACKER_IMPL_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_CHANGE_SUCCESS_TRACKER_IMPL_H_

#include "components/password_manager/core/browser/password_change_success_tracker.h"

#include "base/memory/raw_ptr.h"
#include "url/gurl.h"

class PrefService;

namespace password_manager {

class PasswordChangeSuccessTrackerImpl
    : public password_manager::PasswordChangeSuccessTracker {
 public:
  // Current record version for flows that are persisted in preferences.
  static constexpr int kTrackerVersion = 1;

  explicit PasswordChangeSuccessTrackerImpl(PrefService* pref_service);

  ~PasswordChangeSuccessTrackerImpl() override;

  void OnChangePasswordFlowStarted(const GURL& url,
                                   const std::string& username,
                                   StartEvent event_type,
                                   EntryPoint entry_point) override;

  void OnManualChangePasswordFlowStarted(const GURL& url,
                                         const std::string& username,
                                         EntryPoint entry_point) override;

  void OnChangePasswordFlowModified(const GURL& url,
                                    StartEvent new_event_type) override;

  void OnChangePasswordFlowModified(const GURL& url,
                                    const std::string& username,
                                    StartEvent new_event_type) override;

  void OnChangePasswordFlowCompleted(const GURL& url,
                                     const std::string& username,
                                     EndEvent event_type) override;

 private:
  // Pointer to the preference service used for persisting events.
  raw_ptr<PrefService> pref_service_;
};

}  // namespace password_manager
#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_CHANGE_SUCCESS_TRACKER_IMPL_H_
