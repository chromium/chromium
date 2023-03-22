// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/glanceables/glanceables_tasks_client_impl.h"

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "base/check.h"
#include "base/functional/callback_forward.h"
#include "google_apis/common/request_sender.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/tasks/tasks_api_requests.h"
#include "google_apis/tasks/tasks_api_response_types.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

namespace ash {
namespace {

using ::google_apis::tasks::ListTaskListsRequest;
using ::google_apis::tasks::ListTasksRequest;
using ::google_apis::tasks::TaskLists;
using ::google_apis::tasks::Tasks;

// TODO(b/269750741): Update the traffic annotation tag once all "[TBD]" items
// are ready.
constexpr net::NetworkTrafficAnnotationTag kTrafficAnnotationTag =
    net::DefineNetworkTrafficAnnotation("glanceables_tasks_integration", R"(
        semantics {
          sender: "Glanceables keyed service"
          description: "Provide ChromeOS users quick access to their "
                       "task lists without opening the app or website"
          trigger: "[TBD] Depends on UI surface and pre-fetching strategy"
          internal {
            contacts {
              email: "chromeos-launcher@google.com"
            }
          }
          user_data {
            type: ACCESS_TOKEN
          }
          data: "The request is authenticated with an OAuth2 access token "
                "identifying the Google account"
          destination: GOOGLE_OWNED_SERVICE
          last_reviewed: "2023-03-14"
        }
        policy {
          cookies_allowed: NO
          setting: "[TBD] This feature cannot be disabled in settings"
          policy_exception_justification: "WIP, guarded by `GlanceablesV2` flag"
        }
    )");

}  // namespace

GlanceablesTasksClientImpl::GlanceablesTasksClientImpl(
    const GlanceablesTasksClientImpl::CreateRequestSenderCallback&
        create_request_sender_callback)
    : create_request_sender_callback_(create_request_sender_callback) {}

GlanceablesTasksClientImpl::~GlanceablesTasksClientImpl() = default;

base::OnceClosure GlanceablesTasksClientImpl::GetTaskLists(
    ListTaskListsRequest::Callback callback) {
  EnsureRequestSenderExists();
  return request_sender_->StartRequestWithAuthRetry(
      std::make_unique<ListTaskListsRequest>(request_sender_.get(),
                                             std::move(callback)));
}

base::OnceClosure GlanceablesTasksClientImpl::GetTasks(
    ListTasksRequest::Callback callback,
    const std::string& task_list_id) {
  DCHECK(!task_list_id.empty());
  EnsureRequestSenderExists();
  return request_sender_->StartRequestWithAuthRetry(
      std::make_unique<ListTasksRequest>(request_sender_.get(),
                                         std::move(callback), task_list_id));
}

void GlanceablesTasksClientImpl::EnsureRequestSenderExists() {
  if (request_sender_) {
    return;
  }
  DCHECK(create_request_sender_callback_);
  request_sender_ = std::move(create_request_sender_callback_)
                        .Run({GaiaConstants::kTasksReadOnlyOAuth2Scope},
                             kTrafficAnnotationTag);
  DCHECK(request_sender_);
}

}  // namespace ash
