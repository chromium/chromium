// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_sharing/internal/partial_failure_sdk_delegate_wrapper.h"

#include "base/functional/callback.h"
#include "base/types/expected.h"
#include "components/data_sharing/public/data_sharing_sdk_delegate.h"
#include "components/data_sharing/public/group_data.h"
#include "components/data_sharing/public/protocol/data_sharing_sdk.pb.h"
#include "third_party/abseil-cpp/absl/status/status.h"

namespace data_sharing {

PartialFailureSDKDelegateWrapper::PartialFailureSDKDelegateWrapper(
    DataSharingSDKDelegate* sdk_delegate)
    : sdk_delegate_(sdk_delegate) {
  CHECK(sdk_delegate_);
}

PartialFailureSDKDelegateWrapper::~PartialFailureSDKDelegateWrapper() = default;

void PartialFailureSDKDelegateWrapper::Initialize(
    DataSharingNetworkLoader* data_sharing_network_loader) {
  sdk_delegate_->Initialize(data_sharing_network_loader);
}

void PartialFailureSDKDelegateWrapper::CreateGroup(
    const data_sharing_pb::CreateGroupParams& params,
    base::OnceCallback<
        void(const base::expected<data_sharing_pb::CreateGroupResult,
                                  absl::Status>&)> callback) {
  sdk_delegate_->CreateGroup(params, std::move(callback));
}

void PartialFailureSDKDelegateWrapper::ReadGroups(
    const data_sharing_pb::ReadGroupsParams& params,
    base::OnceCallback<void(
        const base::expected<data_sharing_pb::ReadGroupsResult, absl::Status>&)>
        callback) {
  // Request should contain at least one group.
  CHECK_GT(params.group_ids_size(), 0);
  // Only one ongoing ReadGroups() call is supported.
  CHECK(ongoing_read_groups_callback_.is_null());
  ongoing_read_groups_callback_ = std::move(callback);

  ongoing_read_groups_groups_count_ = params.group_ids_size();
  CHECK_EQ(params.group_ids_size(), params.group_params_size());
  for (int i = 0; i < params.group_ids_size(); ++i) {
    const std::string& group_id = params.group_ids(i);
    data_sharing_pb::ReadGroupsParams single_read_group_params;
    single_read_group_params.add_group_ids(group_id);
    *single_read_group_params.add_group_params() =
        params.group_params(i);
    sdk_delegate_->ReadGroups(
        single_read_group_params,
        base::BindOnce(
            &PartialFailureSDKDelegateWrapper::OnSingleReadGroupCompleted,
            weak_ptr_factory_.GetWeakPtr(), GroupId(group_id)));
  }
}

void PartialFailureSDKDelegateWrapper::OnSingleReadGroupCompleted(
    const GroupId& group_id,
    const base::expected<data_sharing_pb::ReadGroupsResult, absl::Status>&
        result) {
  finished_read_group_results_[group_id] = result;
  ongoing_read_groups_groups_count_--;
  if (ongoing_read_groups_groups_count_ == 0) {
    OnAllOngoingReadGroupsCompleted();
  }
}

void PartialFailureSDKDelegateWrapper::OnAllOngoingReadGroupsCompleted() {
  CHECK(ongoing_read_groups_callback_);
  // Determine if the whole ReadGroups() call should be reported as success or
  // failure. It makes sense to propagate individual group successful reads or
  // known permanent failures, but if everything failed with a transient error,
  // then the whole ReadGroups() call should fail with a transient error.
  bool has_successful_read_group_result = false;
  bool has_permanent_failure = false;
  for (const auto& [_, result] : finished_read_group_results_) {
    if (result.has_value()) {
      has_successful_read_group_result = true;
    }
    if (!result.has_value() &&
        (result.error().code() == absl::StatusCode::kNotFound ||
         result.error().code() == absl::StatusCode::kPermissionDenied)) {
      has_permanent_failure = true;
    }
  }

  if (!has_successful_read_group_result && !has_permanent_failure) {
    // Pick and report first error.
    // At least one group was requested (see CHECK() in ReadGroups()), so there
    // should be at least one result.
    CHECK_GT(finished_read_group_results_.size(), 0u);
    CHECK(!finished_read_group_results_.begin()->second.has_value());
    std::move(ongoing_read_groups_callback_)
        .Run(base::unexpected(
            finished_read_group_results_.begin()->second.error()));
    return;
  }

  data_sharing_pb::ReadGroupsResult read_groups_result;
  for (const auto& [group_id, result] : finished_read_group_results_) {
    if (result.has_value() && result.value().group_data_size() == 1) {
      // Need to check that group_data_size() == 1 to guard against protocol
      // violations.
      *read_groups_result.add_group_data() = result.value().group_data(0);
    } else {
      auto* failed_read_group_result =
          read_groups_result.add_failed_read_group_results();
      failed_read_group_result->set_group_id(group_id.value());
      if (result.has_value()) {
        // Protocol violation: see if block above. Report as transient error.
        failed_read_group_result->set_failure_reason(
            data_sharing_pb::FailedReadGroupResult::TRANSIENT_ERROR);
      } else if (result.error().code() == absl::StatusCode::kNotFound) {
        failed_read_group_result->set_failure_reason(
            data_sharing_pb::FailedReadGroupResult::GROUP_NOT_FOUND);
      } else if (result.error().code() == absl::StatusCode::kPermissionDenied) {
        failed_read_group_result->set_failure_reason(
            data_sharing_pb::FailedReadGroupResult::USER_NOT_MEMBER);
      } else {
        // The rest is not distinguished and reported as a transient error.
        failed_read_group_result->set_failure_reason(
            data_sharing_pb::FailedReadGroupResult::TRANSIENT_ERROR);
      }
    }
  }
  std::move(ongoing_read_groups_callback_).Run(read_groups_result);
}

void PartialFailureSDKDelegateWrapper::AddMember(
    const data_sharing_pb::AddMemberParams& params,
    base::OnceCallback<void(const absl::Status&)> callback) {
  sdk_delegate_->AddMember(params, std::move(callback));
}

void PartialFailureSDKDelegateWrapper::RemoveMember(
    const data_sharing_pb::RemoveMemberParams& params,
    base::OnceCallback<void(const absl::Status&)> callback) {
  sdk_delegate_->RemoveMember(params, std::move(callback));
}

void PartialFailureSDKDelegateWrapper::LeaveGroup(
    const data_sharing_pb::LeaveGroupParams& params,
    base::OnceCallback<void(const absl::Status&)> callback) {
  sdk_delegate_->LeaveGroup(params, std::move(callback));
}

void PartialFailureSDKDelegateWrapper::DeleteGroup(
    const data_sharing_pb::DeleteGroupParams& params,
    base::OnceCallback<void(const absl::Status&)> callback) {
  sdk_delegate_->DeleteGroup(params, std::move(callback));
}

void PartialFailureSDKDelegateWrapper::LookupGaiaIdByEmail(
    const data_sharing_pb::LookupGaiaIdByEmailParams& params,
    base::OnceCallback<
        void(const base::expected<data_sharing_pb::LookupGaiaIdByEmailResult,
                                  absl::Status>&)> callback) {
  sdk_delegate_->LookupGaiaIdByEmail(params, std::move(callback));
}

void PartialFailureSDKDelegateWrapper::Shutdown() {
  sdk_delegate_->Shutdown();
}

void PartialFailureSDKDelegateWrapper::AddAccessToken(
    const data_sharing_pb::AddAccessTokenParams& params,
    base::OnceCallback<
        void(const base::expected<data_sharing_pb::AddAccessTokenResult,
                                  absl::Status>&)> callback) {
  sdk_delegate_->AddAccessToken(params, std::move(callback));
}

}  // namespace data_sharing
