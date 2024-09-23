// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_ORGANIZATION_TAB_ORGANIZATION_REQUEST_H_
#define CHROME_BROWSER_UI_TABS_ORGANIZATION_TAB_ORGANIZATION_REQUEST_H_

#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/token.h"
#include "chrome/browser/ui/tabs/organization/tab_data.h"
#include "chrome/browser/ui/tabs/organization/tab_organization.h"
#include "components/tab_groups/tab_group_id.h"

class TabOrganizationSession;

struct GroupData {
  explicit GroupData(tab_groups::TabGroupId id_,
                     std::u16string label_,
                     std::vector<std::unique_ptr<TabData>> tabs_);
  ~GroupData();

  const tab_groups::TabGroupId id;
  const std::u16string label;
  const std::vector<std::unique_ptr<TabData>> tabs;
};

struct TabOrganizationResponse {
  using LogResultsCallback =
      base::OnceCallback<void(const TabOrganizationSession* session)>;

  struct Organization {
    explicit Organization(
        std::u16string label_,
        std::vector<TabData::TabID> tab_ids_,
        std::optional<tab_groups::TabGroupId> group_id_ = std::nullopt,
        std::optional<TabOrganization::ID> organization_id_ = std::nullopt);
    Organization(const Organization& organization);
    Organization(Organization&& organization);
    ~Organization();

    const std::u16string label;
    const std::vector<TabData::TabID> tab_ids;
    std::optional<tab_groups::TabGroupId> group_id;
    std::optional<TabOrganization::ID> organization_id;
  };

  explicit TabOrganizationResponse(
      std::vector<Organization> organizations_,
      std::u16string feedback_id_ = u"",
      LogResultsCallback log_results_callback_ = base::DoNothing());
  ~TabOrganizationResponse();

  int GetTabCount();

  std::vector<Organization> organizations;
  const std::u16string feedback_id;
  LogResultsCallback log_results_callback;
};

class TabOrganizationRequest {
 public:
  enum class State { NOT_STARTED, STARTED, COMPLETED, FAILED, CANCELED };

  using OnResponseCallback =
      base::OnceCallback<void(TabOrganizationResponse* response)>;

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
  using GroupDatas = std::vector<std::unique_ptr<GroupData>>;

  explicit TabOrganizationRequest(
      BackendStartRequest backend_start_request_lambda = base::DoNothing(),
      BackendCancelRequest backend_cancel_request_lambda = base::DoNothing());
  ~TabOrganizationRequest();

  State state() const { return state_; }
  const TabDatas& tab_datas() const { return tab_datas_; }
  const GroupDatas& group_datas() const { return group_datas_; }
  const std::optional<TabData::TabID> base_tab_id() const {
    return base_tab_id_;
  }
  const TabOrganizationResponse* response() const {
    return response_ ? response_.get() : nullptr;
  }

  void SetBaseTabID(TabData::TabID base_tab_id) { base_tab_id_ = base_tab_id; }

  void SetResponseCallback(OnResponseCallback callback);
  TabData* AddTabData(std::unique_ptr<TabData> tab_data);
  void AddGroupData(tab_groups::TabGroupId id,
                    std::u16string label,
                    std::vector<std::unique_ptr<TabData>> tabs);

  void StartRequest();
  void FailRequest();
  void CancelRequest();
  void CompleteRequestForTesting(
      std::unique_ptr<TabOrganizationResponse> response) {
    CompleteRequest(std::move(response));
  }
  void LogResults(const TabOrganizationSession* session);

 private:
  void CompleteRequest(std::unique_ptr<TabOrganizationResponse> response);

  State state_ = State::NOT_STARTED;
  TabDatas tab_datas_;
  GroupDatas group_datas_;
  std::optional<TabData::TabID> base_tab_id_ = std::nullopt;

  // Time measurements for the request, used to log latency metrics.
  std::optional<base::Time> request_start_time_ = std::nullopt;
  std::optional<base::Time> request_end_time_ = std::nullopt;

  std::unique_ptr<TabOrganizationResponse> response_;
  OnResponseCallback response_callback_;

  BackendStartRequest backend_start_request_lambda_;
  BackendCancelRequest backend_cancel_request_lambda_;

  base::WeakPtrFactory<TabOrganizationRequest> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_TABS_ORGANIZATION_TAB_ORGANIZATION_REQUEST_H_
