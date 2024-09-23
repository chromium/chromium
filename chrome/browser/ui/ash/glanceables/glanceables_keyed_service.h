// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_GLANCEABLES_GLANCEABLES_KEYED_SERVICE_H_
#define CHROME_BROWSER_UI_ASH_GLANCEABLES_GLANCEABLES_KEYED_SERVICE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "components/account_id/account_id.h"
#include "components/keyed_service/core/keyed_service.h"

class Profile;

namespace google_apis {
class RequestSender;
}  // namespace google_apis

namespace net {
struct NetworkTrafficAnnotationTag;
}  // namespace net

namespace signin {
class IdentityManager;
}

namespace ash {

namespace api {
class TasksClientImpl;
}  // namespace api

class GlanceablesClassroomClientImpl;

// Browser context keyed service that owns implementations of interfaces from
// ash/ needed to communicate with different Google services as part of
// Glanceables project.
//
// As of March 2023, this service is created only for primary profiles (see
// `chrome/browser/ash/login/session/user_session_initializer.cc`) and does not
// support multi-user sign-in.
// TODO(b/269750741): Confirm timelines and revisit whether multi-user sign-in
// support is needed.
class GlanceablesKeyedService : public KeyedService {
 public:
  explicit GlanceablesKeyedService(Profile* profile);
  GlanceablesKeyedService(const GlanceablesKeyedService&) = delete;
  GlanceablesKeyedService& operator=(const GlanceablesKeyedService&) = delete;
  ~GlanceablesKeyedService() override;

  // KeyedService:
  void Shutdown() override;

 private:
  // Helper method that creates a `google_apis::RequestSender` instance.
  // `scopes` - OAuth 2 scopes needed for a client.
  // `traffic_annotation_tag` - describes requests issued by a client (for more
  // details see docs/network_traffic_annotations.md and
  // chrome/browser/privacy/traffic_annotation.proto).
  std::unique_ptr<google_apis::RequestSender> CreateRequestSenderForClient(
      const std::vector<std::string>& scopes,
      const net::NetworkTrafficAnnotationTag& traffic_annotation_tag) const;

  // The profile for which this keyed service was created.
  const raw_ptr<Profile> profile_;

  // Identity manager associated with `profile_`.
  raw_ptr<signin::IdentityManager> identity_manager_;

  // Account id associated with the primary profile.
  const AccountId account_id_;

  // Instance of the `GlanceablesClassroomClient` interface implementation.
  std::unique_ptr<GlanceablesClassroomClientImpl> classroom_client_;

  // Instance of the `api::TasksClient` interface implementation.
  std::unique_ptr<api::TasksClientImpl> tasks_client_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_ASH_GLANCEABLES_GLANCEABLES_KEYED_SERVICE_H_
