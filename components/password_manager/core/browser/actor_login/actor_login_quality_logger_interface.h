// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ACTOR_LOGIN_ACTOR_LOGIN_QUALITY_LOGGER_INTERFACE_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ACTOR_LOGIN_ACTOR_LOGIN_QUALITY_LOGGER_INTERFACE_H_

#include "components/optimization_guide/proto/features/actor_login.pb.h"
#include "components/password_manager/core/browser/actor_login/actor_login_types.h"
#include "components/translate/core/browser/translate_manager.h"

namespace optimization_guide {
class ModelQualityLogsUploaderService;
}  // namespace optimization_guide

namespace actor_login {

// Manages Model Logging Quality and uploads logs to the server.
// Each log corresponds to a single filling, which means there would be
// at most one GetCredentials request, and at most two AttemptLogin requests.
// The second AttemptLogin request can happen if the first one failed with
// kErrorDeviceReauthRequired.
class ActorLoginQualityLoggerInterface {
 public:
  virtual ~ActorLoginQualityLoggerInterface() = default;

  virtual void SetDomainAndLanguage(
      translate::TranslateManager* translate_manager,
      const GURL& url) = 0;
  virtual void SetGetCredentialsDetails(
      optimization_guide::proto::ActorLoginQuality_GetCredentialsDetails
          get_credentials_details) = 0;

  virtual void AddAttemptLoginDetails(
      optimization_guide::proto::ActorLoginQuality_AttemptLoginDetails
          attempt_login_details) = 0;

  virtual void SetPermissionPicked(
      optimization_guide::proto::ActorLoginQuality_PermissionOption
          permission_option) = 0;

  // To be called when the trajectory is finished and the final log should
  // be uploaded to the server.
  virtual void UploadFinalLog(
      optimization_guide::ModelQualityLogsUploaderService* mqls_uploader)
      const = 0;
};
}  // namespace actor_login

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ACTOR_LOGIN_ACTOR_LOGIN_QUALITY_LOGGER_INTERFACE_H_
