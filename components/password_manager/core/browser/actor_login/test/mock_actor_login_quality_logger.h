// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ACTOR_LOGIN_TEST_MOCK_ACTOR_LOGIN_QUALITY_LOGGER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ACTOR_LOGIN_TEST_MOCK_ACTOR_LOGIN_QUALITY_LOGGER_H_

#include "components/optimization_guide/core/model_quality/model_quality_logs_uploader_service.h"
#include "components/optimization_guide/proto/features/actor_login.pb.h"
#include "components/password_manager/core/browser/actor_login/actor_login_quality_logger_interface.h"
#include "components/translate/core/browser/translate_manager.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace actor_login {
class MockActorLoginQualityLogger : public ActorLoginQualityLoggerInterface {
 public:
  MockActorLoginQualityLogger();
  ~MockActorLoginQualityLogger() override;

  MOCK_METHOD(void,
              SetDomainAndLanguage,
              (translate::TranslateManager*, const GURL&),
              (override));
  MOCK_METHOD(
      void,
      SetGetCredentialsDetails,
      (optimization_guide::proto::ActorLoginQuality_GetCredentialsDetails),
      (override));
  MOCK_METHOD(
      void,
      AddAttemptLoginDetails,
      (optimization_guide::proto::ActorLoginQuality_AttemptLoginDetails),
      (override));
  MOCK_METHOD(void,
              SetPermissionPicked,
              (optimization_guide::proto::ActorLoginQuality_PermissionOption
                   permission_option),
              (override));
  MOCK_METHOD(void,
              UploadFinalLog,
              (optimization_guide::ModelQualityLogsUploaderService *
               mqls_uploader),
              (const, override));

  base::WeakPtr<MockActorLoginQualityLogger> AsWeakPtr();

 private:
  base::WeakPtrFactory<MockActorLoginQualityLogger> weak_ptr_factory_{this};
};
}  // namespace actor_login

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ACTOR_LOGIN_TEST_MOCK_ACTOR_LOGIN_QUALITY_LOGGER_H_
