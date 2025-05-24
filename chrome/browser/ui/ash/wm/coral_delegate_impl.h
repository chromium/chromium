// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_WM_CORAL_DELEGATE_IMPL_H_
#define CHROME_BROWSER_UI_ASH_WM_CORAL_DELEGATE_IMPL_H_

#include "ash/public/cpp/coral_delegate.h"
#include "base/memory/raw_ref.h"
#include "base/scoped_observation.h"
#include "base/timer/timer.h"
#include "components/signin/public/identity_manager/identity_manager.h"

class ApplicationLocaleStorage;
class DesksTemplatesAppLaunchHandler;

namespace variations {
class VariationsService;
}  // namespace variations

class CoralDelegateImpl : public ash::CoralDelegate,
                          public signin::IdentityManager::Observer {
 public:
  // `application_locale_storage` and `variations_service` must be non-null and
  // must outlive `this`.
  CoralDelegateImpl(const ApplicationLocaleStorage* application_locale_storage,
                    const variations::VariationsService* variations_service);
  CoralDelegateImpl(const CoralDelegateImpl&) = delete;
  CoralDelegateImpl& operator=(const CoralDelegateImpl&) = delete;
  ~CoralDelegateImpl() override;

  void OnPostLoginLaunchComplete(const base::Token& group_id);

  // ash::CoralDelegate:
  void LaunchPostLoginGroup(coral::mojom::GroupPtr group) override;
  void MoveTabsInGroupToNewDesk(const std::vector<coral::mojom::Tab>& tabs,
                                size_t src_desk_index) override;
  int GetChromeDefaultRestoreId() override;
  void OpenFeedbackDialog(const std::string& group_description,
                          ash::ScannerDelegate::SendFeedbackCallback
                              send_feedback_callback) override;
  void CheckGenAIAgeAvailability(GenAIInquiryCallback callback) override;
  bool GetGenAILocationAvailability() override;
  std::string GetSystemLanguage() override;

  // signin::IdentityManager::Observer:
  void OnIdentityManagerShutdown(
      signin::IdentityManager* identity_manager) override;
  void OnRefreshTokensLoaded() override;

 private:
  void HandleGenerativeAiInquiryTimeout();

  const raw_ref<const ApplicationLocaleStorage> application_locale_storage_;
  const raw_ref<const variations::VariationsService> variations_service_;

  // Handles launching apps and creating browsers for post login groups.
  std::map<base::Token, std::unique_ptr<DesksTemplatesAppLaunchHandler>>
      app_launch_handlers_;

  GenAIInquiryCallback gen_ai_age_inquiry_callback_;

  base::OneShotTimer gen_ai_age_inquiry_timeout_;

  base::ScopedObservation<signin::IdentityManager,
                          signin::IdentityManager::Observer>
      identity_manager_observation_{this};

  base::WeakPtrFactory<CoralDelegateImpl> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_ASH_WM_CORAL_DELEGATE_IMPL_H_
