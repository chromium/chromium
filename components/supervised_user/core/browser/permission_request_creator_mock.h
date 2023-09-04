// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUPERVISED_USER_CORE_BROWSER_PERMISSION_REQUEST_CREATOR_MOCK_H_
#define COMPONENTS_SUPERVISED_USER_CORE_BROWSER_PERMISSION_REQUEST_CREATOR_MOCK_H_

#include <vector>

#include "base/memory/raw_ref.h"
#include "components/supervised_user/core/browser/permission_request_creator.h"
#include "url/gurl.h"

namespace supervised_user {
class SupervisedUserSettingsService;

class PermissionRequestCreatorMock : public PermissionRequestCreator {
 public:
  explicit PermissionRequestCreatorMock(
      SupervisedUserSettingsService& settings_service);

  PermissionRequestCreatorMock(const PermissionRequestCreatorMock&) = delete;
  PermissionRequestCreatorMock& operator=(const PermissionRequestCreatorMock&) =
      delete;

  ~PermissionRequestCreatorMock() override;

  // PermissionRequestCreator:
  bool IsEnabled() const override;
  void CreateURLAccessRequest(const GURL& url_requested,
                              SuccessCallback callback) override;

  // Sets whether the next call to create requests will succeed.
  void SetPermissionResult(bool result);
  void SetEnabled();

  // Delays approvals for incoming requests until
  // |PermissionRequestCreatorMock::HandleDelayedRequests| is called.
  void DelayHandlingForNextRequests();
  void HandleDelayedRequests();

  // Getter methods.
  const std::vector<GURL>& url_requests() const { return url_requests_; }

 private:
  void CreateURLAccessRequestImpl(const GURL& url_requested);

  bool result_ = false;
  bool enabled_ = false;
  bool delay_handling_ = false;
  int last_url_request_handled_index_ = 0;

  const raw_ref<SupervisedUserSettingsService> settings_service_;
  std::vector<GURL> url_requests_;
};

}  // namespace supervised_user

#endif  // COMPONENTS_SUPERVISED_USER_CORE_BROWSER_PERMISSION_REQUEST_CREATOR_MOCK_H_
