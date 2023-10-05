// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_ORGANIZATION_TAB_ORGANIZATION_REQUEST_H_
#define CHROME_BROWSER_UI_TABS_ORGANIZATION_TAB_ORGANIZATION_REQUEST_H_

#include <vector>

#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/token.h"
#include "chrome/browser/ui/tabs/organization/tab_data.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

struct TabOrganizationResponse {
  struct Organization {
    explicit Organization(std::u16string label_,
                          std::vector<TabData::TabID> tab_ids_);
    Organization(const Organization& organization);
    Organization(Organization&& organization);
    ~Organization();

    const std::u16string label;
    const std::vector<TabData::TabID> tab_ids;
  };

  explicit TabOrganizationResponse(std::vector<Organization> organizations_);
  ~TabOrganizationResponse();

  const std::vector<Organization> organizations;
};

class TabOrganizationRequest {
 public:
  enum class State { NOT_STARTED, STARTED, COMPLETED, FAILED, CANCELED };

  using OnResponseCallback =
      base::OnceCallback<void(const TabOrganizationResponse* response)>;

  using BackendCompletionCallback = base::OnceCallback<void(
      std::unique_ptr<TabOrganizationResponse> response)>;
  using BackendFailureCallback = base::OnceCallback<void()>;
  using BackendStartRequest =
      base::OnceCallback<void(const TabOrganizationRequest* request,
                              BackendCompletionCallback on_completion,
                              BackendFailureCallback on_failure)>;
  using BackendCancelRequest =
      base::OnceCallback<void(const TabOrganizationRequest* request)>;

  using TabDatas = std::vector<std::unique_ptr<TabData>>;

  explicit TabOrganizationRequest(
      BackendStartRequest backend_start_request_lambda = base::DoNothing(),
      BackendCancelRequest backend_cancel_request_lambda = base::DoNothing());
  ~TabOrganizationRequest();

  State state() const { return state_; }
  const TabDatas& tab_datas() const { return tab_datas_; }
  const absl::optional<TabData::TabID> base_tab_id() const {
    return base_tab_id_;
  }
  const TabOrganizationResponse* response() const {
    return response_ ? response_.get() : nullptr;
  }

  void SetResponseCallback(OnResponseCallback callback);
  TabData* AddTabData(std::unique_ptr<TabData> tab_data);

  void StartRequest();
  void FailRequest();
  void CancelRequest();
  void CompleteRequestForTesting(
      std::unique_ptr<TabOrganizationResponse> response) {
    CompleteRequest(std::move(response));
  }

 private:
  void CompleteRequest(std::unique_ptr<TabOrganizationResponse> response);

  State state_ = State::NOT_STARTED;
  TabDatas tab_datas_;
  absl::optional<TabData::TabID> base_tab_id_ = absl::nullopt;
  std::unique_ptr<TabOrganizationResponse> response_;
  OnResponseCallback response_callback_;

  BackendStartRequest backend_start_request_lambda_;
  BackendCancelRequest backend_cancel_request_lambda_;
};

#endif  // CHROME_BROWSER_UI_TABS_ORGANIZATION_TAB_ORGANIZATION_REQUEST_H_
