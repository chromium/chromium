// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/starboard/media/cdm/starboard_decryptor_cast.h"

#include <algorithm>
#include <memory>
#include <string>
#include <string_view>

#include "base/functional/callback.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "chromecast/starboard/media/cdm/starboard_drm_key_tracker.h"
#include "chromecast/starboard/media/cdm/starboard_drm_wrapper.h"
#include "chromecast/starboard/media/media/mock_starboard_api_wrapper.h"
#include "chromecast/starboard/media/media/starboard_api_wrapper.h"
#include "media/base/cdm_callback_promise.h"
#include "media/base/provision_fetcher.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace chromecast {
namespace media {
namespace {

using ::media::CdmKeyInformation;
using ::media::CdmKeysInfo;
using ::media::CdmMessageType;
using ::media::ProvisionFetcher;
using ::testing::_;
using ::testing::DoAll;
using ::testing::ElementsAreArray;
using ::testing::Eq;
using ::testing::Field;
using ::testing::Mock;
using ::testing::MockFunction;
using ::testing::NotNull;
using ::testing::Pointee;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::StartsWith;
using ::testing::StrEq;
using ::testing::WithArg;

constexpr char kProvisionServerUrl[] =
    "https://www.googleapis.com/"
    "certificateprovisioning/v1/devicecertificates/create"
    "?key=";

// Checks that a const void* arg matches a string.
//
// str is an std::string (or std::string_view), arg is a const void*. Only the
// first str.length() characters will be checked.
MATCHER_P(StrEqWhenCast, str, "") {
  const char* c_str_arg = static_cast<const char*>(arg);
  if (!c_str_arg) {
    *result_listener << "the argument is null";
    return false;
  }

  if (strlen(c_str_arg) < str.length()) {
    *result_listener << "the argument is too short";
    return false;
  }

  return ExplainMatchResult(StrEq(str), std::string(c_str_arg, str.length()),
                            result_listener);
}

// Checks that a GURL arg's string starts with the given string.
//
// str is a string (or c string), arg is a GURL.
MATCHER_P(GurlStartsWith, str, "") {
  return ExplainMatchResult(StartsWith(str), arg.spec(), result_listener);
}

// Runs any currently-queued tasks.
void RunQueuedTasks() {
  base::RunLoop run_loop;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();
}

class MockProvisionFetcher : public ProvisionFetcher {
 public:
  MockProvisionFetcher() = default;
  ~MockProvisionFetcher() override = default;

  MOCK_METHOD(void,
              Retrieve,
              (const GURL& default_url,
               const std::string& request_data,
               ResponseCB response_cb),
              (override));
};

// A test fixture is used to manage the global mock state and to handle the
// lifetime of the SingleThreadTaskEnvironment.
class StarboardDecryptorCastTest : public ::testing::Test {
 public:
  std::unique_ptr<ProvisionFetcher> CreateProvisionFetcher() {
    CHECK(provision_fetcher_) << "provision_fetcher_ must be set by a test "
                                 "before CreateProvisionFetcher is called";
    return std::move(provision_fetcher_);
  }

 protected:
  StarboardDecryptorCastTest()
      : starboard_(std::make_unique<MockStarboardApiWrapper>()) {
    ON_CALL(*starboard_, EnsureInitialized).WillByDefault(Return(true));
    ON_CALL(*starboard_, DrmIsServerCertificateUpdatable)
        .WillByDefault(Return(true));
  }

  ~StarboardDecryptorCastTest() override = default;

  // This should be destructed last.
  base::test::SingleThreadTaskEnvironment task_environment_;
  // This will be passed to the StarboardDecryptorCast, and all calls to
  // Starboard will go through it. Thus, we can mock out those calls.
  std::unique_ptr<MockStarboardApiWrapper> starboard_;
  // This will be returned when CreateProvisionFetcher is called. It can be set
  // by tests to mock the provision fetcher.
  std::unique_ptr<MockProvisionFetcher> provision_fetcher_;
  // Since SbDrmSystem is just an opaque blob to the StarboardDecryptorCast, we
  // will simply use an int to represent it.
  int fake_drm_system_ = 1;

  // Mock functions passed to CastCdm::Initialize.
  MockFunction<void(const std::string& session_id,
                    CdmMessageType message_type,
                    const std::vector<uint8_t>& message)>
      session_message_cb_;
  MockFunction<void(const std::string& session_id,
                    ::media::CdmSessionClosedReason reason)>
      session_closed_cb_;
  MockFunction<void(const std::string& session_id,
                    bool has_additional_usable_key,
                    CdmKeysInfo keys_info)>
      session_keys_change_cb_;
  MockFunction<void(const std::string& session_id, base::Time new_expiry_time)>
      session_expiration_update_cb_;
};

TEST_F(StarboardDecryptorCastTest,
       SendsProvisionRequestToWidevineProvisioningServer) {
  const std::string license_request_data_str = "license request data";
  base::span<const uint8_t> license_request_data =
      base::as_byte_span(license_request_data_str);
  const std::string provision_request_data = "provision_request data";
  const base::span<const uint8_t> provision_request_span =
      base::as_byte_span(provision_request_data);
  const std::string provision_response_data = "provision_response data";
  const std::string session_id = "some_session";
  const std::string error_message = "";
  // This is generated by StarboardDrmWrapper; we capture the ticket in a
  // callback below.
  int ticket = StarboardDrmWrapper::kInvalidTicket;

  // This will get populated by decryptor.
  const StarboardDrmSystemCallbackHandler* decryptor_provided_callbacks =
      nullptr;

  EXPECT_CALL(*starboard_, CreateDrmSystem("com.widevine.alpha", _))
      .WillOnce(DoAll(SaveArg<1>(&decryptor_provided_callbacks),
                      Return(&fake_drm_system_)));
  EXPECT_CALL(*starboard_, DrmGenerateSessionUpdateRequest(&fake_drm_system_, _,
                                                           StrEq("cenc"), _, _))
      .WillOnce(SaveArg<1>(&ticket));
  EXPECT_CALL(*starboard_,
              DrmUpdateSession(&fake_drm_system_, _,
                               StrEqWhenCast(provision_response_data),
                               provision_response_data.size(),
                               StrEqWhenCast(session_id), session_id.size()))
      .Times(1);

  StarboardDrmWrapper::SetSingletonForTesting(starboard_.get());
  auto decryptor = base::MakeRefCounted<StarboardDecryptorCast>(
      /*create_provision_fetcher_cb=*/base::BindRepeating(
          &StarboardDecryptorCastTest::CreateProvisionFetcher,
          base::Unretained(this)),
      /*media_resource_tracker=*/nullptr);
  std::string actual_session_id;
  bool resolved_promise = false;
  decryptor->Initialize(
      base::BindLambdaForTesting(session_message_cb_.AsStdFunction()),
      base::BindLambdaForTesting(session_closed_cb_.AsStdFunction()),
      base::BindLambdaForTesting(session_keys_change_cb_.AsStdFunction()),
      base::BindLambdaForTesting(
          session_expiration_update_cb_.AsStdFunction()));

  ASSERT_THAT(decryptor_provided_callbacks, NotNull());

  // This provision fetcher will be returned when
  // create_provision_fetcher_cb is called.
  provision_fetcher_ = std::make_unique<MockProvisionFetcher>();
  EXPECT_CALL(*provision_fetcher_, Retrieve(GurlStartsWith(kProvisionServerUrl),
                                            StrEq(provision_request_data), _))
      .WillOnce(
          WithArg<2>([provision_response_data](
                         base::OnceCallback<void(bool, const std::string&)>
                             response_callback) {
            std::move(response_callback).Run(true, provision_response_data);
          }));

  // First trigger the session creation.
  decryptor->CreateSessionAndGenerateRequest(
      ::media::CdmSessionType::kTemporary, ::media::EmeInitDataType::CENC,
      /*init_data=*/{1, 2, 3, 4},
      std::make_unique<::media::CdmCallbackPromise<std::string>>(
          /*resolve_cb=*/base::BindOnce(
              +[](bool* b, std::string* out_session_id,
                  const std::string& session_id) {
                CHECK(b != nullptr);
                CHECK(out_session_id != nullptr);
                *b = true;
                *out_session_id = session_id;
              },
              &resolved_promise, &actual_session_id),
          /*reject_cb=*/base::BindOnce(
              +[](::media::CdmPromise::Exception exception_code,
                  uint32_t system_code, const std::string& error_message) {
                LOG(ERROR) << "Rejected promise with system code "
                           << system_code << " and error message "
                           << error_message;
              })));

  // Now simulate the DRM system requesting provisioning from cast. In
  // production scenarios, the DRM system first sends a license request for the
  // given ticket, and then sends an individualization (provisioning) request
  // with the invalid ticket ID (signifying that the request was triggered by
  // the SbDrmSystem itself).
  EXPECT_NE(ticket, StarboardDrmWrapper::kInvalidTicket);
  ASSERT_THAT(decryptor_provided_callbacks->update_request_fn, NotNull());
  decryptor_provided_callbacks->update_request_fn(
      &fake_drm_system_, decryptor_provided_callbacks->context, ticket,
      kStarboardDrmStatusSuccess, kStarboardDrmSessionRequestTypeLicenseRequest,
      error_message, session_id,
      std::vector<uint8_t>(license_request_data.begin(),
                           license_request_data.end()),
      "");
  decryptor_provided_callbacks->update_request_fn(
      &fake_drm_system_, decryptor_provided_callbacks->context,
      StarboardDrmWrapper::kInvalidTicket, kStarboardDrmStatusSuccess,
      kStarboardDrmSessionRequestTypeIndividualizationRequest, error_message,
      session_id,
      std::vector<uint8_t>(provision_request_span.begin(),
                           provision_request_span.end()),
      "");

  // The functions in decryptor_provided_callbacks post tasks to
  // task_environment_.
  RunQueuedTasks();
}

TEST_F(StarboardDecryptorCastTest,
       DoesNotResolveProvisionPromiseUntilResponseIsReceived) {
  const std::string provision_request_data_str = "request data";
  base::span<const uint8_t> provision_request_data =
      base::as_byte_span(provision_request_data_str);
  const std::string provision_response_data = "response data";
  const std::string session_id = "some_session";
  const ::media::EmeInitDataType init_type = ::media::EmeInitDataType::CENC;
  const std::string init_data_str = "init_data";
  base::span<const uint8_t> init_data = base::as_byte_span(init_data_str);

  // This will get populated by decryptor.
  const StarboardDrmSystemCallbackHandler* decryptor_provided_callbacks =
      nullptr;

  EXPECT_CALL(*starboard_, CreateDrmSystem("com.widevine.alpha", _))
      .WillOnce(DoAll(SaveArg<1>(&decryptor_provided_callbacks),
                      Return(&fake_drm_system_)));

  int update_session_ticket = -1;
  EXPECT_CALL(*starboard_,
              DrmUpdateSession(&fake_drm_system_, _,
                               StrEqWhenCast(provision_response_data),
                               provision_response_data.size(),
                               StrEqWhenCast(session_id), session_id.size()))
      .WillOnce(SaveArg<1>(&update_session_ticket));

  int update_session_request_ticket = -1;
  EXPECT_CALL(*starboard_,
              DrmGenerateSessionUpdateRequest(
                  &fake_drm_system_, _, StrEq("cenc"),
                  StrEqWhenCast(init_data_str), init_data_str.size()))
      .WillOnce(SaveArg<1>(&update_session_request_ticket));

  StarboardDrmWrapper::SetSingletonForTesting(starboard_.get());
  auto decryptor = base::MakeRefCounted<StarboardDecryptorCast>(
      /*create_provision_fetcher_cb=*/base::BindRepeating(
          &StarboardDecryptorCastTest::CreateProvisionFetcher,
          base::Unretained(this)),
      /*media_resource_tracker=*/nullptr);
  decryptor->Initialize(
      base::BindLambdaForTesting(session_message_cb_.AsStdFunction()),
      base::BindLambdaForTesting(session_closed_cb_.AsStdFunction()),
      base::BindLambdaForTesting(session_keys_change_cb_.AsStdFunction()),
      base::BindLambdaForTesting(
          session_expiration_update_cb_.AsStdFunction()));

  ASSERT_THAT(decryptor_provided_callbacks, NotNull());

  base::OnceCallback<void(bool, const std::string&)> provision_callback;
  // This provision fetcher will be returned when
  // create_provision_fetcher_cb is called.
  provision_fetcher_ = std::make_unique<MockProvisionFetcher>();
  EXPECT_CALL(*provision_fetcher_,
              Retrieve(GurlStartsWith(kProvisionServerUrl),
                       StrEq(provision_request_data_str), _))
      .WillOnce(WithArg<2>(
          [&provision_callback](
              base::OnceCallback<void(bool, const std::string&)> cb) {
            provision_callback = std::move(cb);
          }));

  // These will be set to if the promise passed to
  // CreateSessionAndGenerateRequest is resolved successfully.
  bool resolved_promise = false;
  std::string actual_session_id;

  // Trigger the session creation.
  decryptor->CreateSessionAndGenerateRequest(
      ::media::CdmSessionType::kTemporary, init_type,
      std::vector<uint8_t>(init_data.begin(), init_data.end()),
      std::make_unique<::media::CdmCallbackPromise<std::string>>(
          /*resolve_cb=*/base::BindOnce(
              +[](bool* b, std::string* out_session_id,
                  const std::string& session_id) {
                CHECK(b != nullptr);
                CHECK(out_session_id != nullptr);
                *b = true;
                *out_session_id = session_id;
              },
              &resolved_promise, &actual_session_id),
          /*reject_cb=*/base::BindOnce(
              +[](::media::CdmPromise::Exception exception_code,
                  uint32_t system_code, const std::string& error_message) {
                LOG(ERROR) << "Rejected promise with system code "
                           << system_code << " and error message "
                           << error_message;
              })));

  // Trigger the provision request.

  const std::string error_message = "";
  ASSERT_THAT(decryptor_provided_callbacks->update_request_fn, NotNull());
  decryptor_provided_callbacks->update_request_fn(
      &fake_drm_system_, decryptor_provided_callbacks->context,
      update_session_request_ticket, kStarboardDrmStatusSuccess,
      kStarboardDrmSessionRequestTypeIndividualizationRequest, error_message,
      session_id,
      std::vector<uint8_t>(provision_request_data.begin(),
                           provision_request_data.end()),
      "");

  // The functions in decryptor_provided_callbacks post tasks to
  // task_environment_.
  RunQueuedTasks();

  // Ensure that the session creation promise has not been resolved, since we
  // did not receive a provisioning response yet.
  EXPECT_FALSE(resolved_promise);

  // Simulate a provisioning response. The promise should be resolved now.
  ASSERT_FALSE(provision_callback.is_null());
  std::move(provision_callback).Run(true, provision_response_data);

  // Simulate starboard responding to the session update.
  decryptor_provided_callbacks->session_updated_fn(
      &fake_drm_system_, decryptor_provided_callbacks->context,
      update_session_ticket, kStarboardDrmStatusSuccess, error_message,
      session_id);

  RunQueuedTasks();
  EXPECT_TRUE(resolved_promise);
}

TEST_F(StarboardDecryptorCastTest, SendsSessionUpdateToStarboard) {
  const std::string session_id = "session_id";
  constexpr std::string_view key = "some_key";
  base::span<const uint8_t> key_span = base::as_byte_span(key);

  // This will get populated by decryptor.
  const StarboardDrmSystemCallbackHandler* decryptor_provided_callbacks =
      nullptr;

  EXPECT_CALL(*starboard_, CreateDrmSystem("com.widevine.alpha", _))
      .WillOnce(DoAll(SaveArg<1>(&decryptor_provided_callbacks),
                      Return(&fake_drm_system_)));

  // This will be set when drm_update_session_fn is called.
  int ticket = -1;
  EXPECT_CALL(
      *starboard_,
      DrmUpdateSession(&fake_drm_system_, _, StrEqWhenCast(key), key.size(),
                       StrEqWhenCast(session_id), session_id.size()))
      .WillOnce(SaveArg<1>(&ticket));

  StarboardDrmWrapper::SetSingletonForTesting(starboard_.get());
  auto decryptor = base::MakeRefCounted<StarboardDecryptorCast>(
      /*create_provision_fetcher_cb=*/base::BindRepeating(
          &StarboardDecryptorCastTest::CreateProvisionFetcher,
          base::Unretained(this)),
      /*media_resource_tracker=*/nullptr);
  decryptor->Initialize(
      base::BindLambdaForTesting(session_message_cb_.AsStdFunction()),
      base::BindLambdaForTesting(session_closed_cb_.AsStdFunction()),
      base::BindLambdaForTesting(session_keys_change_cb_.AsStdFunction()),
      base::BindLambdaForTesting(
          session_expiration_update_cb_.AsStdFunction()));

  ASSERT_THAT(decryptor_provided_callbacks, NotNull());

  // This will be set to true if the promise passed to UpdateSession is resolved
  // successfully.
  bool resolved_promise = false;

  // Trigger the session update.
  decryptor->UpdateSession(
      session_id, std::vector<uint8_t>(key_span.begin(), key_span.end()),
      std::make_unique<::media::CdmCallbackPromise<>>(
          /*resolve_cb=*/base::BindOnce(
              +[](bool* b) {
                CHECK(b != nullptr);
                *b = true;
              },
              &resolved_promise),
          /*reject_cb=*/base::BindOnce(
              +[](::media::CdmPromise::Exception exception_code,
                  uint32_t system_code, const std::string& error_message) {
                LOG(ERROR) << "Rejected promise with system code "
                           << system_code << " and error message "
                           << error_message;
              })));

  // Simulate a successful session update.
  ASSERT_THAT(decryptor_provided_callbacks->session_updated_fn, NotNull());
  decryptor_provided_callbacks->session_updated_fn(
      &fake_drm_system_, decryptor_provided_callbacks->context, ticket,
      kStarboardDrmStatusSuccess, /*error_message=*/"", session_id);

  // The functions in decryptor_provided_callbacks post tasks to
  // task_environment_.
  RunQueuedTasks();
  EXPECT_TRUE(resolved_promise);
}

TEST_F(StarboardDecryptorCastTest, CallsKeyChangeCallbackOnKeyUpdate) {
  StarboardDrmKeyTracker::GetInstance().ClearStateForTesting();

  const std::vector<uint8_t> init_data = {0, 1, 2, 3};
  constexpr std::string_view key_id = "123456";
  const auto key_status = CdmKeyInformation::KeyStatus::USABLE;
  const uint32_t system_code = 0;
  const std::string session_id = "some_session_id";
  const std::string license_request_data_str = "license_data";
  base::span<const uint8_t> license_request_data =
      base::as_byte_span(license_request_data_str);

  // The starboard representation of key_id.
  StarboardDrmKeyId starboard_key_id;
  // Safely copy the data from key_id into starboard_key_id.identifier.
  base::span<uint8_t>(starboard_key_id.identifier)
      .first<key_id.size()>()
      .copy_from(base::as_byte_span(key_id));
  starboard_key_id.identifier_size = key_id.size();
  // The starboard representation of key_status.
  const StarboardDrmKeyStatus starboard_key_status =
      kStarboardDrmKeyStatusUsable;

  // This will get populated by decryptor.
  const StarboardDrmSystemCallbackHandler* decryptor_provided_callbacks =
      nullptr;

  int actual_create_ticket = StarboardDrmWrapper::kInvalidTicket;
  EXPECT_CALL(*starboard_, CreateDrmSystem("com.widevine.alpha", _))
      .WillOnce(DoAll(SaveArg<1>(&decryptor_provided_callbacks),
                      Return(&fake_drm_system_)));
  EXPECT_CALL(*starboard_, DrmGenerateSessionUpdateRequest(&fake_drm_system_, _,
                                                           StrEq("cenc"), _, _))
      .WillOnce(SaveArg<1>(&actual_create_ticket));

  EXPECT_CALL(
      session_keys_change_cb_,
      Call(StrEq(session_id), true,
           ElementsAre(Pointee(AllOf(
               Field(&CdmKeyInformation::key_id, ElementsAreArray(key_id)),
               Field(&CdmKeyInformation::status, Eq(key_status)),
               Field(&CdmKeyInformation::system_code, Eq(system_code)))))))
      .Times(1);

  StarboardDrmWrapper::SetSingletonForTesting(starboard_.get());
  auto decryptor = base::MakeRefCounted<StarboardDecryptorCast>(
      /*create_provision_fetcher_cb=*/base::BindRepeating(
          &StarboardDecryptorCastTest::CreateProvisionFetcher,
          base::Unretained(this)),
      /*media_resource_tracker=*/nullptr);
  decryptor->Initialize(
      base::BindLambdaForTesting(session_message_cb_.AsStdFunction()),
      base::BindLambdaForTesting(session_closed_cb_.AsStdFunction()),
      base::BindLambdaForTesting(session_keys_change_cb_.AsStdFunction()),
      base::BindLambdaForTesting(
          session_expiration_update_cb_.AsStdFunction()));

  // Trigger the session creation so that a session ID exists.
  std::string actual_session_id;

  decryptor->CreateSessionAndGenerateRequest(
      ::media::CdmSessionType::kTemporary, ::media::EmeInitDataType::CENC,
      init_data,
      std::make_unique<::media::CdmCallbackPromise<std::string>>(
          /*resolve_cb=*/base::BindOnce(
              +[](std::string* out_session_id, const std::string& session_id) {
                CHECK(out_session_id != nullptr);
                *out_session_id = session_id;
              },
              &actual_session_id),
          /*reject_cb=*/base::BindOnce(
              +[](::media::CdmPromise::Exception exception_code,
                  uint32_t system_code, const std::string& error_message) {
                LOG(ERROR) << "Rejected promise with system code "
                           << system_code << " and error message "
                           << error_message;
              })));

  ASSERT_THAT(decryptor_provided_callbacks, NotNull());
  ASSERT_THAT(decryptor_provided_callbacks->update_request_fn, NotNull());
  ASSERT_THAT(decryptor_provided_callbacks->key_statuses_changed_fn, NotNull());

  // Simulate the session creation.
  EXPECT_NE(actual_create_ticket, StarboardDrmWrapper::kInvalidTicket);
  decryptor_provided_callbacks->update_request_fn(
      &fake_drm_system_, decryptor_provided_callbacks->context,
      actual_create_ticket, kStarboardDrmStatusSuccess,
      kStarboardDrmSessionRequestTypeLicenseRequest, "", session_id,
      std::vector<uint8_t>(license_request_data.begin(),
                           license_request_data.end()),
      "");

  // Notify the decryptor that the key status changed. This should trigger the
  // expected call to session_keys_change_cb_ above.
  decryptor_provided_callbacks->key_statuses_changed_fn(
      &fake_drm_system_, decryptor_provided_callbacks->context, session_id,
      {starboard_key_id}, {starboard_key_status});

  // The functions in decryptor_provided_callbacks post tasks to
  // task_environment_.
  RunQueuedTasks();

  // Verify that StarboardDrmKeyTracker was updated.
  const std::string key_str(reinterpret_cast<const char*>(key_id.data()),
                            key_id.size());
  EXPECT_TRUE(StarboardDrmKeyTracker::GetInstance().HasKey(key_str));

  // Verify that the key is removed when the decryptor is destroyed.
  decryptor = nullptr;
  EXPECT_FALSE(StarboardDrmKeyTracker::GetInstance().HasKey(key_str));
  EXPECT_EQ(actual_session_id, session_id);
}

TEST_F(StarboardDecryptorCastTest,
       RemovesKeyFromStarboardDrmKeyTrackerWhenKeyIsReleased) {
  StarboardDrmKeyTracker::GetInstance().ClearStateForTesting();

  const std::string session_id = "some_session";
  constexpr std::string_view key_id = "abcdef";
  const auto key_status = CdmKeyInformation::KeyStatus::USABLE;
  const auto key_released_status = CdmKeyInformation::KeyStatus::RELEASED;
  const uint32_t system_code = 0;
  const std::string license_request_data_str = "license_request_data";
  base::span<const uint8_t> license_request_data =
      base::as_byte_span(license_request_data_str);

  // The starboard representation of key_id.
  StarboardDrmKeyId starboard_key_id;
  // Safely copy the data from key_id into starboard_key_id.identifier.
  base::span<uint8_t>(starboard_key_id.identifier)
      .first<key_id.size()>()
      .copy_from(base::as_byte_span(key_id));
  starboard_key_id.identifier_size = key_id.size();

  // The starboard representation of key_status.
  const StarboardDrmKeyStatus starboard_key_status =
      kStarboardDrmKeyStatusUsable;
  const StarboardDrmKeyStatus starboard_key_released_status =
      kStarboardDrmKeyStatusReleased;

  // This will get populated by decryptor.
  const StarboardDrmSystemCallbackHandler* decryptor_provided_callbacks =
      nullptr;

  int ticket = StarboardDrmWrapper::kInvalidTicket;
  EXPECT_CALL(*starboard_, CreateDrmSystem("com.widevine.alpha", _))
      .WillOnce(DoAll(SaveArg<1>(&decryptor_provided_callbacks),
                      Return(&fake_drm_system_)));
  EXPECT_CALL(*starboard_, DrmGenerateSessionUpdateRequest(&fake_drm_system_, _,
                                                           StrEq("cenc"), _, _))
      .WillOnce(SaveArg<1>(&ticket));

  EXPECT_CALL(
      session_keys_change_cb_,
      Call(StrEq(session_id), true,
           ElementsAre(Pointee(AllOf(
               Field(&CdmKeyInformation::key_id,
                     ElementsAreArray(base::as_byte_span(key_id))),
               Field(&CdmKeyInformation::status, Eq(key_status)),
               Field(&CdmKeyInformation::system_code, Eq(system_code)))))))
      .Times(1);
  EXPECT_CALL(
      session_keys_change_cb_,
      Call(StrEq(session_id), false,
           ElementsAre(Pointee(AllOf(
               Field(&CdmKeyInformation::key_id,
                     ElementsAreArray(base::as_byte_span(key_id))),
               Field(&CdmKeyInformation::status, Eq(key_released_status)),
               Field(&CdmKeyInformation::system_code, Eq(system_code)))))))
      .Times(1);

  StarboardDrmWrapper::SetSingletonForTesting(starboard_.get());
  auto decryptor = base::MakeRefCounted<StarboardDecryptorCast>(
      /*create_provision_fetcher_cb=*/base::BindRepeating(
          &StarboardDecryptorCastTest::CreateProvisionFetcher,
          base::Unretained(this)),
      /*media_resource_tracker=*/nullptr);
  decryptor->Initialize(
      base::BindLambdaForTesting(session_message_cb_.AsStdFunction()),
      base::BindLambdaForTesting(session_closed_cb_.AsStdFunction()),
      base::BindLambdaForTesting(session_keys_change_cb_.AsStdFunction()),
      base::BindLambdaForTesting(
          session_expiration_update_cb_.AsStdFunction()));

  // Trigger the session creation so that a session ID exists.
  std::string actual_session_id;
  decryptor->CreateSessionAndGenerateRequest(
      ::media::CdmSessionType::kTemporary, ::media::EmeInitDataType::CENC,
      /*init_data=*/{1, 2, 3, 4},
      std::make_unique<::media::CdmCallbackPromise<std::string>>(
          /*resolve_cb=*/base::BindOnce(
              +[](std::string* out_session_id, const std::string& session_id) {
                CHECK(out_session_id != nullptr);
                *out_session_id = session_id;
              },
              &actual_session_id),
          /*reject_cb=*/base::BindOnce(
              +[](::media::CdmPromise::Exception exception_code,
                  uint32_t system_code, const std::string& error_message) {
                LOG(ERROR) << "Rejected promise with system code "
                           << system_code << " and error message "
                           << error_message;
              })));

  ASSERT_THAT(decryptor_provided_callbacks, NotNull());
  ASSERT_THAT(decryptor_provided_callbacks->key_statuses_changed_fn, NotNull());
  // Simulate the session creation.
  EXPECT_NE(ticket, StarboardDrmWrapper::kInvalidTicket);
  decryptor_provided_callbacks->update_request_fn(
      &fake_drm_system_, decryptor_provided_callbacks->context, ticket,
      kStarboardDrmStatusSuccess, kStarboardDrmSessionRequestTypeLicenseRequest,
      "", session_id,
      std::vector<uint8_t>(license_request_data.begin(),
                           license_request_data.end()),
      "");
  // Notify the decryptor that the key status changed. This should trigger the
  // expected call to session_keys_change_cb_ above.
  decryptor_provided_callbacks->key_statuses_changed_fn(
      &fake_drm_system_, decryptor_provided_callbacks->context, session_id,
      {starboard_key_id}, {starboard_key_status});

  // The functions in decryptor_provided_callbacks post tasks to
  // task_environment_.
  RunQueuedTasks();

  // Verify that StarboardDrmKeyTracker was updated.
  EXPECT_TRUE(
      StarboardDrmKeyTracker::GetInstance().HasKey(std::string(key_id)));

  // Verify that the key is removed when the status changes to removed.
  decryptor_provided_callbacks->key_statuses_changed_fn(
      &fake_drm_system_, decryptor_provided_callbacks->context, session_id,
      {starboard_key_id}, {starboard_key_released_status});

  // The functions in decryptor_provided_callbacks post tasks to
  // task_environment_.
  RunQueuedTasks();

  EXPECT_FALSE(
      StarboardDrmKeyTracker::GetInstance().HasKey(std::string(key_id)));
  EXPECT_EQ(actual_session_id, session_id);
}

TEST_F(StarboardDecryptorCastTest, CreatesSessionAndGeneratesLicenseRequest) {
  constexpr char kLicenseUrl[] = "www.example.com";
  const std::string session_id = "session_id";
  const std::string content = "license_request_content";
  const base::span<const uint8_t> content_span = base::as_byte_span(content);
  const ::media::EmeInitDataType init_type = ::media::EmeInitDataType::CENC;
  const std::string init_data_str = "init_data";
  const base::span<const uint8_t> init_data = base::as_byte_span(init_data_str);

  // This will get populated by decryptor.
  const StarboardDrmSystemCallbackHandler* decryptor_provided_callbacks =
      nullptr;

  EXPECT_CALL(*starboard_, CreateDrmSystem("com.widevine.alpha", _))
      .WillOnce(DoAll(SaveArg<1>(&decryptor_provided_callbacks),
                      Return(&fake_drm_system_)));
  // This will be set when drm_generate_session_update_request_fn is called.
  int ticket = StarboardDrmWrapper::kInvalidTicket;
  EXPECT_CALL(*starboard_,
              DrmGenerateSessionUpdateRequest(
                  &fake_drm_system_, _, StrEq("cenc"),
                  StrEqWhenCast(init_data_str), init_data_str.size()))
      .WillOnce(SaveArg<1>(&ticket));

  EXPECT_CALL(
      session_message_cb_,
      Call(StrEq(session_id), CdmMessageType::LICENSE_REQUEST,
           ElementsAreArray(reinterpret_cast<const uint8_t*>(content.c_str()),
                            content.size())))
      .Times(1);

  StarboardDrmWrapper::SetSingletonForTesting(starboard_.get());
  auto decryptor = base::MakeRefCounted<StarboardDecryptorCast>(
      /*create_provision_fetcher_cb=*/base::BindRepeating(
          &StarboardDecryptorCastTest::CreateProvisionFetcher,
          base::Unretained(this)),
      /*media_resource_tracker=*/nullptr);
  decryptor->Initialize(
      base::BindLambdaForTesting(session_message_cb_.AsStdFunction()),
      base::BindLambdaForTesting(session_closed_cb_.AsStdFunction()),
      base::BindLambdaForTesting(session_keys_change_cb_.AsStdFunction()),
      base::BindLambdaForTesting(
          session_expiration_update_cb_.AsStdFunction()));

  ASSERT_THAT(decryptor_provided_callbacks, NotNull());

  // These will be set to if the promise passed to
  // CreateSessionAndGenerateRequest is resolved successfully.
  bool resolved_promise = false;
  std::string actual_session_id;

  // Trigger the session creation.
  decryptor->CreateSessionAndGenerateRequest(
      ::media::CdmSessionType::kTemporary, init_type,
      std::vector<uint8_t>(init_data.begin(), init_data.end()),
      std::make_unique<::media::CdmCallbackPromise<std::string>>(
          /*resolve_cb=*/base::BindOnce(
              +[](bool* b, std::string* out_session_id,
                  const std::string& session_id) {
                CHECK(b != nullptr);
                CHECK(out_session_id != nullptr);
                *b = true;
                *out_session_id = session_id;
              },
              &resolved_promise, &actual_session_id),
          /*reject_cb=*/base::BindOnce(
              +[](::media::CdmPromise::Exception exception_code,
                  uint32_t system_code, const std::string& error_message) {
                LOG(ERROR) << "Rejected promise with system code "
                           << system_code << " and error message "
                           << error_message;
              })));

  // Simulate starboard's response to drm_generate_session_update_request_fn.
  ASSERT_THAT(decryptor_provided_callbacks->update_request_fn, NotNull());
  EXPECT_NE(ticket, StarboardDrmWrapper::kInvalidTicket);
  decryptor_provided_callbacks->update_request_fn(
      &fake_drm_system_, decryptor_provided_callbacks->context, ticket,
      kStarboardDrmStatusSuccess, kStarboardDrmSessionRequestTypeLicenseRequest,
      /*error_message=*/"", session_id,
      std::vector<uint8_t>(content_span.begin(), content_span.end()),
      kLicenseUrl);

  // The functions in decryptor_provided_callbacks post tasks to
  // task_environment_.
  RunQueuedTasks();
  EXPECT_TRUE(resolved_promise);
  EXPECT_THAT(actual_session_id, StrEq(session_id));
}

TEST_F(StarboardDecryptorCastTest, CreatesSessionAndGeneratesLicenseRenewal) {
  constexpr char kLicenseUrl[] = "www.example.com";
  const std::string session_id = "session_id";
  const std::string content = "license_request_content";
  const base::span<const uint8_t> content_span = base::as_byte_span(content);
  const ::media::EmeInitDataType init_type = ::media::EmeInitDataType::CENC;
  const std::string init_data_str = "init_data";
  const base::span<const uint8_t> init_data = base::as_byte_span(init_data_str);

  // This will get populated by decryptor.
  const StarboardDrmSystemCallbackHandler* decryptor_provided_callbacks =
      nullptr;

  EXPECT_CALL(*starboard_, CreateDrmSystem("com.widevine.alpha", _))
      .WillOnce(DoAll(SaveArg<1>(&decryptor_provided_callbacks),
                      Return(&fake_drm_system_)));

  // This will be set when drm_generate_session_update_request_fn is called.
  int ticket = StarboardDrmWrapper::kInvalidTicket;
  EXPECT_CALL(*starboard_,
              DrmGenerateSessionUpdateRequest(
                  &fake_drm_system_, _, StrEq("cenc"),
                  StrEqWhenCast(init_data_str), init_data_str.size()))
      .WillOnce(SaveArg<1>(&ticket));

  EXPECT_CALL(
      session_message_cb_,
      Call(StrEq(session_id), CdmMessageType::LICENSE_RENEWAL,
           ElementsAreArray(reinterpret_cast<const uint8_t*>(content.c_str()),
                            content.size())))
      .Times(1);

  StarboardDrmWrapper::SetSingletonForTesting(starboard_.get());
  auto decryptor = base::MakeRefCounted<StarboardDecryptorCast>(
      /*create_provision_fetcher_cb=*/base::BindRepeating(
          &StarboardDecryptorCastTest::CreateProvisionFetcher,
          base::Unretained(this)),
      /*media_resource_tracker=*/nullptr);
  decryptor->Initialize(
      base::BindLambdaForTesting(session_message_cb_.AsStdFunction()),
      base::BindLambdaForTesting(session_closed_cb_.AsStdFunction()),
      base::BindLambdaForTesting(session_keys_change_cb_.AsStdFunction()),
      base::BindLambdaForTesting(
          session_expiration_update_cb_.AsStdFunction()));

  ASSERT_THAT(decryptor_provided_callbacks, NotNull());

  // These will be set to if the promise passed to
  // CreateSessionAndGenerateRequest is resolved successfully.
  bool resolved_promise = false;
  std::string actual_session_id;

  // Trigger the session creation.
  decryptor->CreateSessionAndGenerateRequest(
      ::media::CdmSessionType::kTemporary, init_type,
      std::vector<uint8_t>(init_data.begin(), init_data.end()),
      std::make_unique<::media::CdmCallbackPromise<std::string>>(
          /*resolve_cb=*/base::BindOnce(
              +[](bool* b, std::string* out_session_id,
                  const std::string& session_id) {
                CHECK(b != nullptr);
                CHECK(out_session_id != nullptr);
                *b = true;
                *out_session_id = session_id;
              },
              &resolved_promise, &actual_session_id),
          /*reject_cb=*/base::BindOnce(
              +[](::media::CdmPromise::Exception exception_code,
                  uint32_t system_code, const std::string& error_message) {
                LOG(ERROR) << "Rejected promise with system code "
                           << system_code << " and error message "
                           << error_message;
              })));

  // Simulate starboard's response to drm_generate_session_update_request_fn.
  ASSERT_THAT(decryptor_provided_callbacks->update_request_fn, NotNull());
  EXPECT_NE(ticket, StarboardDrmWrapper::kInvalidTicket);
  decryptor_provided_callbacks->update_request_fn(
      &fake_drm_system_, decryptor_provided_callbacks->context, ticket,
      kStarboardDrmStatusSuccess, kStarboardDrmSessionRequestTypeLicenseRenewal,
      /*error_message=*/"", session_id,
      std::vector<uint8_t>(content_span.begin(), content_span.end()),
      kLicenseUrl);

  // The functions in decryptor_provided_callbacks post tasks to
  // task_environment_.
  RunQueuedTasks();
  EXPECT_TRUE(resolved_promise);
  EXPECT_THAT(actual_session_id, StrEq(session_id));
}

TEST_F(StarboardDecryptorCastTest, ForwardsCloseSessionToStarboard) {
  const std::string session_id = "session_id";
  const std::vector<uint8_t> init_data = {0, 1, 2, 3};

  // This will get populated by decryptor.
  const StarboardDrmSystemCallbackHandler* decryptor_provided_callbacks =
      nullptr;

  EXPECT_CALL(*starboard_, CreateDrmSystem("com.widevine.alpha", _))
      .WillOnce(DoAll(SaveArg<1>(&decryptor_provided_callbacks),
                      Return(&fake_drm_system_)));
  int actual_create_ticket = std::numeric_limits<int>::min();
  EXPECT_CALL(*starboard_,
              DrmGenerateSessionUpdateRequest(&fake_drm_system_, _, _, _, _))
      .WillOnce(SaveArg<1>(&actual_create_ticket));
  EXPECT_CALL(
      *starboard_,
      DrmCloseSession(&fake_drm_system_, session_id.c_str(), session_id.size()))
      .Times(1);

  StarboardDrmWrapper::SetSingletonForTesting(starboard_.get());
  auto decryptor = base::MakeRefCounted<StarboardDecryptorCast>(
      /*create_provision_fetcher_cb=*/base::BindRepeating(
          &StarboardDecryptorCastTest::CreateProvisionFetcher,
          base::Unretained(this)),
      /*media_resource_tracker=*/nullptr);
  decryptor->Initialize(
      base::BindLambdaForTesting(session_message_cb_.AsStdFunction()),
      base::BindLambdaForTesting(session_closed_cb_.AsStdFunction()),
      base::BindLambdaForTesting(session_keys_change_cb_.AsStdFunction()),
      base::BindLambdaForTesting(
          session_expiration_update_cb_.AsStdFunction()));

  ASSERT_THAT(decryptor_provided_callbacks, NotNull());
  ASSERT_THAT(decryptor_provided_callbacks->update_request_fn, NotNull());
  ASSERT_THAT(decryptor_provided_callbacks->session_closed_fn, NotNull());

  decryptor->CreateSessionAndGenerateRequest(
      ::media::CdmSessionType::kTemporary, ::media::EmeInitDataType::CENC,
      init_data,
      std::make_unique<::media::CdmCallbackPromise<std::string>>(
          /*resolve_cb=*/base::BindOnce(
              +[](const std::string& /*session_id*/) {}),
          /*reject_cb=*/base::BindOnce(
              +[](::media::CdmPromise::Exception exception_code,
                  uint32_t system_code, const std::string& error_message) {
                LOG(ERROR) << "Rejected promise with system code "
                           << system_code << " and error message "
                           << error_message;
              })));

  // Simulate a response for the session being created, so that the decryptor
  // tracks the session ID.
  decryptor_provided_callbacks->update_request_fn(
      &fake_drm_system_, decryptor_provided_callbacks->context,
      actual_create_ticket, StarboardDrmStatus::kStarboardDrmStatusSuccess,
      StarboardDrmSessionRequestType::
          kStarboardDrmSessionRequestTypeLicenseRequest,
      "", session_id, {1}, "");

  base::RunLoop create_session_loop;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, create_session_loop.QuitClosure());
  create_session_loop.Run();

  // This will be set to true if the promise passed to CloseSession is resolved
  // successfully.
  bool resolved_promise = false;

  decryptor->CloseSession(
      session_id,
      std::make_unique<::media::CdmCallbackPromise<>>(
          /*resolve_cb=*/base::BindOnce(
              +[](bool* b) {
                CHECK(b != nullptr);
                *b = true;
              },
              &resolved_promise),
          /*reject_cb=*/base::BindOnce(
              +[](::media::CdmPromise::Exception exception_code,
                  uint32_t system_code, const std::string& error_message) {
                LOG(ERROR) << "Rejected promise with system code "
                           << system_code << " and error message "
                           << error_message;
              })));

  // Simulate starboard's response to drm_close_session_fn.
  decryptor_provided_callbacks->session_closed_fn(
      &fake_drm_system_, decryptor_provided_callbacks->context, session_id);

  // The functions in decryptor_provided_callbacks post tasks to
  // task_environment_.
  RunQueuedTasks();
  EXPECT_TRUE(resolved_promise);
}

TEST_F(StarboardDecryptorCastTest, DoesNotDestroySbDrmSystemOnDestruction) {
  EXPECT_CALL(*starboard_, CreateDrmSystem("com.widevine.alpha", _))
      .WillOnce(Return(&fake_drm_system_));
  EXPECT_CALL(*starboard_, DrmDestroySystem).Times(0);

  StarboardDrmWrapper::SetSingletonForTesting(starboard_.get());
  auto decryptor = base::MakeRefCounted<StarboardDecryptorCast>(
      /*create_provision_fetcher_cb=*/base::BindRepeating(
          &StarboardDecryptorCastTest::CreateProvisionFetcher,
          base::Unretained(this)),
      /*media_resource_tracker=*/nullptr);
  decryptor->Initialize(
      base::BindLambdaForTesting(session_message_cb_.AsStdFunction()),
      base::BindLambdaForTesting(session_closed_cb_.AsStdFunction()),
      base::BindLambdaForTesting(session_keys_change_cb_.AsStdFunction()),
      base::BindLambdaForTesting(
          session_expiration_update_cb_.AsStdFunction()));
}

}  // namespace
}  // namespace media
}  // namespace chromecast
