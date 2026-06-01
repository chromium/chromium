// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/network/autofill_ai/personal_context_access_manager_impl.h"

#include <memory>
#include <string>
#include <vector>

#include "base/test/gmock_callback_support.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type_names.h"
#include "components/personal_context/core/personal_context_enablement_service.h"
#include "components/personal_context/core/personal_context_service.h"
#include "components/personal_context/core/personal_context_types.h"
#include "components/personal_context/proto/features/ambient_autofill.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

namespace {

using ::base::test::RunOnceCallback;
using ::testing::_;
using ::testing::DoAll;
using ::testing::WithArg;

class MockPersonalContextService
    : public personal_context::PersonalContextService {
 public:
  MockPersonalContextService() = default;
  ~MockPersonalContextService() override = default;

  MOCK_METHOD(void,
              FetchContext,
              (personal_context::proto::ContextMemoryFeature feature,
               const google::protobuf::MessageLite& request_metadata,
               const personal_context::ContextMemoryRequestOptions& options,
               personal_context::FetchContextCallback callback),
              (override));
};

class MockPersonalContextEnablementService
    : public personal_context::PersonalContextEnablementService {
 public:
  MockPersonalContextEnablementService() = default;
  ~MockPersonalContextEnablementService() override = default;

  MOCK_METHOD(void, AddObserver, (Observer * observer), (override));
  MOCK_METHOD(void, RemoveObserver, (Observer * observer), (override));
  MOCK_METHOD(personal_context::PersonalContextEnablementState,
              GetEnablementState,
              (),
              (override));
};

class PersonalContextAccessManagerImplTest : public testing::Test {
 public:
  PersonalContextAccessManagerImplTest() = default;
  ~PersonalContextAccessManagerImplTest() override = default;

  void SetUp() override {
    access_manager_ = std::make_unique<PersonalContextAccessManagerImpl>(
        &mock_personal_context_service_,
        &mock_personal_context_enablement_service_);
  }

  PersonalContextAccessManagerImpl& access_manager() {
    return *access_manager_;
  }

  MockPersonalContextService& mock_personal_context_service() {
    return mock_personal_context_service_;
  }

 private:
  base::test::TaskEnvironment task_environment_;
  MockPersonalContextService mock_personal_context_service_;
  MockPersonalContextEnablementService
      mock_personal_context_enablement_service_;
  std::unique_ptr<PersonalContextAccessManagerImpl> access_manager_;
};

TEST_F(PersonalContextAccessManagerImplTest,
       FetchAmbientAutofillContextSuccess) {
  const std::vector<EntityType> requested_types = {
      EntityType(EntityTypeName::kOrder)};

  personal_context::proto::ContextMemoryAmbientAutofillResponse
      expected_response;
  personal_context::proto::Entity* entity = expected_response.add_entities();
  entity->mutable_order()->set_order_id("12345");
  entity->mutable_order()->set_merchant_name("Amazon");

  personal_context::proto::Any any_response;
  expected_response.SerializeToString(any_response.mutable_value());

  EXPECT_CALL(
      mock_personal_context_service(),
      FetchContext(
          personal_context::proto::CONTEXT_MEMORY_FEATURE_AMBIENT_AUTOFILL, _,
          _, _))
      .WillOnce(DoAll(
          WithArg<1>([](const google::protobuf::MessageLite& request_metadata) {
            const auto& request =
                static_cast<const personal_context::proto::
                                ContextMemoryAmbientAutofillRequest&>(
                    request_metadata);
            ASSERT_EQ(request.requested_types_size(), 1);
            EXPECT_EQ(request.requested_types(0),
                      personal_context::proto::EntityType::ORDER);
          }),
          RunOnceCallback<3>(personal_context::FetchContextResult(
              base::ok(std::move(any_response))))));

  base::test::TestFuture<
      base::expected<std::string, personal_context::ContextMemoryError>>
      future;

  access_manager().FetchAmbientAutofillContext(requested_types,
                                               future.GetCallback());

  auto result = future.Take();
  ASSERT_TRUE(result.has_value());

  personal_context::proto::ContextMemoryAmbientAutofillResponse response;
  ASSERT_TRUE(response.ParseFromString(result.value()));
  EXPECT_EQ(response.entities_size(), 1);
  EXPECT_EQ(response.entities(0).order().order_id(), "12345");
  EXPECT_EQ(response.entities(0).order().merchant_name(), "Amazon");
}

TEST_F(PersonalContextAccessManagerImplTest,
       FetchAmbientAutofillContextFailure) {
  const std::vector<EntityType> requested_types = {
      EntityType(EntityTypeName::kOrder)};

  personal_context::ContextMemoryError expected_error =
      personal_context::ContextMemoryError::FromExecutionError(
          personal_context::ContextMemoryError::ExecutionError::
              kGenericFailure);

  EXPECT_CALL(
      mock_personal_context_service(),
      FetchContext(
          personal_context::proto::CONTEXT_MEMORY_FEATURE_AMBIENT_AUTOFILL, _,
          _, _))
      .WillOnce(RunOnceCallback<3>(personal_context::FetchContextResult(
          base::unexpected(expected_error))));

  base::test::TestFuture<
      base::expected<std::string, personal_context::ContextMemoryError>>
      future;

  access_manager().FetchAmbientAutofillContext(requested_types,
                                               future.GetCallback());

  auto result = future.Take();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().error(), expected_error.error());
}

}  // namespace

}  // namespace autofill
