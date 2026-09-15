// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ACTOR_LOGIN_ACTOR_LOGIN_QUALITY_LOGGER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ACTOR_LOGIN_ACTOR_LOGIN_QUALITY_LOGGER_H_

#include <memory>

#include "components/optimization_guide/core/model_quality/model_quality_log_entry.h"
#include "components/optimization_guide/proto/features/actor_login.pb.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "components/password_manager/core/browser/actor_login/actor_login_quality_logger_interface.h"
#include "components/translate/core/browser/translate_manager.h"

namespace optimization_guide {
class ModelQualityLogsUploaderService;
}  // namespace optimization_guide

namespace variations {
class VariationsService;
}  // namespace variations

// Implementation of actor_login::ActorLoginQualityLoggerInterface
class ActorLoginQualityLogger
    : public actor_login::ActorLoginQualityLoggerInterface {
 public:
  // `mqls_uploader` is the service the log is uploaded to, or null if quality
  // logging is disabled, in which case the log is collected but dropped. It is
  // only used to create the log entry: binding it here rather than looking it
  // up when the log is uploaded means the log survives the tab the login was
  // attempted in.
  ActorLoginQualityLogger(
      variations::VariationsService* variations_service,
      optimization_guide::ModelQualityLogsUploaderService* mqls_uploader);
  ActorLoginQualityLogger(const ActorLoginQualityLogger&) = delete;
  ActorLoginQualityLogger& operator=(const ActorLoginQualityLogger&) = delete;

  // actor_login::ActorLoginQualityLoggerInterface:
  void SetDomainAndLanguage(translate::TranslateManager* translate_manager,
                            const GURL& url) override;
  void SetGetCredentialsDetails(
      optimization_guide::proto::ActorLoginQuality_GetCredentialsDetails
          get_credentials_details) override;

  void SetFederatedGetCredentialsDetails(
      optimization_guide::proto::
          ActorLoginQuality_FederatedGetCredentialsDetails
              federated_get_credentials_details) override;
  void AddAttemptLoginDetails(
      optimization_guide::proto::ActorLoginQuality_AttemptLoginDetails
          attempt_login_details) override;
  void SetPermissionPicked(
      optimization_guide::proto::ActorLoginQuality_PermissionOption
          permission_option) override;

#if defined(UNIT_TEST)
  const optimization_guide::proto::ActorLoginQuality& get_log_data() {
    return quality();
  }
#endif  // defined(UNIT_TEST)

 private:
  friend class base::RefCounted<actor_login::ActorLoginQualityLoggerInterface>;

  // Destroying `log_entry_` uploads it. The logger is destroyed once the last
  // participant of the login flow drops its reference, which is when the
  // trajectory is finished.
  ~ActorLoginQualityLogger() override;

  // The quality message of the log this class fills in.
  optimization_guide::proto::ActorLoginQuality& quality();

  // The log this class fills in. Uploaded when it is destroyed, unless it was
  // created without an uploader.
  const std::unique_ptr<optimization_guide::ModelQualityLogEntry> log_entry_;
};

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ACTOR_LOGIN_ACTOR_LOGIN_QUALITY_LOGGER_H_
