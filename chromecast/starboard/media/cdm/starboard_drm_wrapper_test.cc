// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/starboard/media/cdm/starboard_drm_wrapper.h"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/test/task_environment.h"
#include "chromecast/starboard/media/cdm/mock_starboard_drm_wrapper_client.h"
#include "chromecast/starboard/media/media/mock_starboard_api_wrapper.h"
#include "chromecast/starboard/media/media/starboard_api_wrapper.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromecast {
namespace media {

// This class exists for the purpose of accessing resources_ and simulating
// AtExitManager logic.
class StarboardDrmWrapperTestPeer {
 public:
  static size_t GetNumResources() {
    return StarboardDrmWrapper::GetInstance().resources_.size();
  }

  static void AttemptDestroySbDrmSystem() {
    // This code is normally not run in tests, because it relies on
    // AtExitManager.
    StarboardDrmWrapper::GetInstance().MaybeDestroySbDrmSystem();
  }
};

namespace {

using ::testing::_;
using ::testing::DoAll;
using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::Eq;
using ::testing::Field;
using ::testing::IsEmpty;
using ::testing::NotNull;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::StrEq;
using ::testing::WithoutArgs;

// Checks that a const void* arg matches a string.
//
// str is an std::string, arg is a const void*. Only the first str.length()
// characters will be checked.
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

// Checks that a StarboardDrmKeyId holds data matching a string.
MATCHER_P(StarboardDrmKeyIdMatches, str, "") {
  const StarboardDrmKeyId& key = arg;
  CHECK_LE(str.size(), std::size(key.identifier))
      << "The expected key must not be longer than "
      << std::size(key.identifier) << " characters.";

  if (key.identifier_size != static_cast<int>(str.size())) {
    *result_listener << " size mismatch";
    return false;
  }

  base::span<const uint8_t> key_view = key.identifier;
  for (size_t i = 0; i < str.size(); ++i) {
    if (key_view[i] != str[i]) {
      *result_listener << " character mismatch at index " << i;
      return false;
    }
  }
  return true;
}

// A test fixture is used to handle creation of a task environment and set up
// the mocks to handle calls that are irrelevant to tests.
//
// We do not call StarboardDrmWrapper::SetSingletonForTesting in the constructor
// since a mock should not be passed to production code until all expectations
// have been set on it.
class StarboardDrmWrapperTest : public ::testing::Test {
 protected:
  StarboardDrmWrapperTest() {
    ON_CALL(starboard_, EnsureInitialized).WillByDefault(Return(true));
  }

  // This should be destructed last.
  base::test::SingleThreadTaskEnvironment task_environment_;
  // This will be passed to the StarboardDecryptorCast, and all calls to
  // Starboard will go through it. Thus, we can mock out those calls.
  MockStarboardApiWrapper starboard_;
  // This is optional so that we can delay construction until after
  // StarboardDrmWrapper::SetSingletonForTesting has been called.
  std::optional<MockStarboardDrmWrapperClient> client_;
  // Since SbDrmSystem is just an opaque blob to the StarboardDecryptorCast, we
  // will simply use an int to represent it.
  int fake_drm_system_ = 1;
};

TEST_F(StarboardDrmWrapperTest, GeneratesSessionUpdateRequestForSuccessCase) {
  const std::string payload_type = "cenc";
  const std::string init_data_str = "init_data";
  base::span<const uint8_t> init_data = base::as_byte_span(init_data_str);
  const std::string content_str = "content";
  base::span<const uint8_t> content = base::as_byte_span(content_str);
  const std::string error_message = "";
  const std::string session_id = "session_id";
  const std::string url = "";
  const StarboardDrmStatus status =
      StarboardDrmStatus::kStarboardDrmStatusSuccess;
  const StarboardDrmSessionRequestType request_type =
      StarboardDrmSessionRequestType::
          kStarboardDrmSessionRequestTypeLicenseRequest;
  const int ticket = 7;

  // This will get populated by StarboardDrmWrapper.
  const StarboardDrmSystemCallbackHandler* decryptor_provided_callbacks =
      nullptr;
  EXPECT_CALL(starboard_, CreateDrmSystem("com.widevine.alpha", _))
      .WillOnce(DoAll(SaveArg<1>(&decryptor_provided_callbacks),
                      Return(&fake_drm_system_)));
  int actual_wrapper_ticket = StarboardDrmWrapper::kInvalidTicket;
  EXPECT_CALL(starboard_,
              DrmGenerateSessionUpdateRequest(
                  &fake_drm_system_, _, StrEq(payload_type),
                  StrEqWhenCast(init_data_str), init_data_str.size()))
      .WillOnce(SaveArg<1>(&actual_wrapper_ticket));
  StarboardDrmWrapper::SetSingletonForTesting(&starboard_);

  client_.emplace();
  EXPECT_CALL(*client_, OnSessionUpdateRequest(ticket, status, request_type,
                                               error_message, session_id,
                                               ElementsAreArray(content)))
      .Times(1);

  StarboardDrmWrapper::GetInstance().GenerateSessionUpdateRequest(
      &*client_, ticket, payload_type,
      std::vector<uint8_t>(init_data.begin(), init_data.end()));

  // Simulate Starboard responding.
  ASSERT_THAT(decryptor_provided_callbacks, NotNull());
  ASSERT_THAT(decryptor_provided_callbacks->update_request_fn, NotNull());
  EXPECT_NE(actual_wrapper_ticket, StarboardDrmWrapper::kInvalidTicket);
  decryptor_provided_callbacks->update_request_fn(
      &fake_drm_system_, &StarboardDrmWrapper::GetInstance(),
      actual_wrapper_ticket, status, request_type, error_message, session_id,
      std::vector<uint8_t>(content.begin(), content.end()), url);
}

TEST_F(StarboardDrmWrapperTest, GeneratesSessionUpdateRequestForErrorCase) {
  const std::string payload_type = "cenc";
  const std::string init_data_str = "init_data";
  base::span<const uint8_t> init_data = base::as_byte_span(init_data_str);
  const std::string error_message = "expected error";
  const std::string session_id = "";
  const std::string url = "";
  const StarboardDrmStatus status =
      StarboardDrmStatus::kStarboardDrmStatusTypeError;
  const StarboardDrmSessionRequestType request_type =
      StarboardDrmSessionRequestType::
          kStarboardDrmSessionRequestTypeLicenseRequest;
  const int ticket = 7;

  // This will get populated by StarboardDrmWrapper.
  const StarboardDrmSystemCallbackHandler* decryptor_provided_callbacks =
      nullptr;
  EXPECT_CALL(starboard_, CreateDrmSystem("com.widevine.alpha", _))
      .WillOnce(DoAll(SaveArg<1>(&decryptor_provided_callbacks),
                      Return(&fake_drm_system_)));
  int actual_wrapper_ticket = StarboardDrmWrapper::kInvalidTicket;
  EXPECT_CALL(starboard_,
              DrmGenerateSessionUpdateRequest(
                  &fake_drm_system_, _, StrEq(payload_type),
                  StrEqWhenCast(init_data_str), init_data_str.size()))
      .WillOnce(SaveArg<1>(&actual_wrapper_ticket));
  StarboardDrmWrapper::SetSingletonForTesting(&starboard_);

  client_.emplace();
  EXPECT_CALL(*client_, OnSessionUpdateRequest(ticket, status, _, error_message,
                                               session_id, IsEmpty()))
      .Times(1);

  StarboardDrmWrapper::GetInstance().GenerateSessionUpdateRequest(
      &*client_, ticket, payload_type,
      std::vector<uint8_t>(init_data.begin(), init_data.end()));

  // Simulate Starboard responding with an error.
  ASSERT_THAT(decryptor_provided_callbacks, NotNull());
  ASSERT_THAT(decryptor_provided_callbacks->update_request_fn, NotNull());
  EXPECT_NE(actual_wrapper_ticket, StarboardDrmWrapper::kInvalidTicket);
  decryptor_provided_callbacks->update_request_fn(
      &fake_drm_system_, &StarboardDrmWrapper::GetInstance(),
      actual_wrapper_ticket, status, request_type, error_message, session_id,
      {}, url);
}

TEST_F(StarboardDrmWrapperTest, UpdatesSessionForSuccessCase) {
  const std::string payload_type = "cenc";
  const std::string init_data_str = "init_data";
  base::span<const uint8_t> init_data = base::as_byte_span(init_data_str);
  const std::string content_str = "content";
  base::span<const uint8_t> content = base::as_byte_span(content_str);
  constexpr std::string_view key = "key";
  const std::string error_message = "";
  const std::string session_id = "session_id";
  const std::string url = "";
  const StarboardDrmStatus status =
      StarboardDrmStatus::kStarboardDrmStatusSuccess;
  const StarboardDrmSessionRequestType request_type =
      StarboardDrmSessionRequestType::
          kStarboardDrmSessionRequestTypeLicenseRequest;
  const int generate_session_ticket = 7;
  const int update_session_ticket = 200;

  // This will get populated by StarboardDrmWrapper.
  const StarboardDrmSystemCallbackHandler* decryptor_provided_callbacks =
      nullptr;
  EXPECT_CALL(starboard_, CreateDrmSystem("com.widevine.alpha", _))
      .WillOnce(DoAll(SaveArg<1>(&decryptor_provided_callbacks),
                      Return(&fake_drm_system_)));
  int actual_wrapper_generate_session_ticket =
      StarboardDrmWrapper::kInvalidTicket;
  int actual_wrapper_update_session_ticket =
      StarboardDrmWrapper::kInvalidTicket;
  EXPECT_CALL(starboard_,
              DrmGenerateSessionUpdateRequest(
                  &fake_drm_system_, _, StrEq(payload_type),
                  StrEqWhenCast(init_data_str), init_data_str.size()))
      .WillOnce(SaveArg<1>(&actual_wrapper_generate_session_ticket));
  EXPECT_CALL(
      starboard_,
      DrmUpdateSession(&fake_drm_system_, _, StrEqWhenCast(key), key.size(),
                       StrEqWhenCast(session_id), session_id.size()))
      .WillOnce(SaveArg<1>(&actual_wrapper_update_session_ticket));
  StarboardDrmWrapper::SetSingletonForTesting(&starboard_);

  client_.emplace();
  EXPECT_CALL(*client_,
              OnSessionUpdateRequest(generate_session_ticket, status,
                                     request_type, error_message, session_id,
                                     ElementsAreArray(content)))
      .Times(1);
  EXPECT_CALL(*client_, OnSessionUpdated(update_session_ticket, status,
                                         error_message, session_id))
      .Times(1);

  StarboardDrmWrapper::GetInstance().GenerateSessionUpdateRequest(
      &*client_, generate_session_ticket, payload_type,
      std::vector<uint8_t>(init_data.begin(), init_data.end()));
  base::span<const uint8_t> key_span = base::as_byte_span(key);
  StarboardDrmWrapper::GetInstance().UpdateSession(
      &*client_, update_session_ticket, session_id,
      std::vector<uint8_t>(key_span.begin(), key_span.end()));

  // Simulate Starboard responding.
  ASSERT_THAT(decryptor_provided_callbacks, NotNull());
  ASSERT_THAT(decryptor_provided_callbacks->update_request_fn, NotNull());
  ASSERT_THAT(decryptor_provided_callbacks->session_updated_fn, NotNull());
  EXPECT_NE(actual_wrapper_generate_session_ticket,
            StarboardDrmWrapper::kInvalidTicket);
  EXPECT_NE(actual_wrapper_update_session_ticket,
            StarboardDrmWrapper::kInvalidTicket);

  decryptor_provided_callbacks->update_request_fn(
      &fake_drm_system_, &StarboardDrmWrapper::GetInstance(),
      actual_wrapper_generate_session_ticket, status, request_type,
      error_message, session_id,
      std::vector<uint8_t>(content.begin(), content.end()), url);
  decryptor_provided_callbacks->session_updated_fn(
      &fake_drm_system_, &StarboardDrmWrapper::GetInstance(),
      actual_wrapper_update_session_ticket, status, error_message, session_id);
}

TEST_F(StarboardDrmWrapperTest, ForwardsKeyStatusUpdates) {
  const std::string payload_type = "cenc";
  const std::string init_data_str = "init_data";
  base::span<const uint8_t> init_data = base::as_byte_span(init_data_str);
  const std::string content_str = "content";
  base::span<const uint8_t> content = base::as_byte_span(content_str);
  constexpr std::string_view key = "key";
  const std::string error_message = "";
  const std::string session_id = "session_id";
  const std::string url = "";
  const StarboardDrmStatus status =
      StarboardDrmStatus::kStarboardDrmStatusSuccess;
  const StarboardDrmSessionRequestType request_type =
      StarboardDrmSessionRequestType::
          kStarboardDrmSessionRequestTypeLicenseRequest;
  const int generate_session_ticket = 7;

  // This will get populated by StarboardDrmWrapper.
  const StarboardDrmSystemCallbackHandler* decryptor_provided_callbacks =
      nullptr;
  EXPECT_CALL(starboard_, CreateDrmSystem("com.widevine.alpha", _))
      .WillOnce(DoAll(SaveArg<1>(&decryptor_provided_callbacks),
                      Return(&fake_drm_system_)));
  int actual_wrapper_generate_session_ticket =
      StarboardDrmWrapper::kInvalidTicket;
  EXPECT_CALL(starboard_,
              DrmGenerateSessionUpdateRequest(
                  &fake_drm_system_, _, StrEq(payload_type),
                  StrEqWhenCast(init_data_str), init_data_str.size()))
      .WillOnce(SaveArg<1>(&actual_wrapper_generate_session_ticket));
  StarboardDrmWrapper::SetSingletonForTesting(&starboard_);

  client_.emplace();
  EXPECT_CALL(*client_,
              OnSessionUpdateRequest(generate_session_ticket, status,
                                     request_type, error_message, session_id,
                                     ElementsAreArray(content)))
      .Times(1);
  EXPECT_CALL(
      *client_,
      OnKeyStatusesChanged(
          session_id, ElementsAre(StarboardDrmKeyIdMatches(key)),
          ElementsAre(StarboardDrmKeyStatus::kStarboardDrmKeyStatusUsable)))
      .Times(1);

  StarboardDrmWrapper::GetInstance().GenerateSessionUpdateRequest(
      &*client_, generate_session_ticket, payload_type,
      std::vector<uint8_t>(init_data.begin(), init_data.end()));

  // Simulate Starboard responding, including a response that keys have been
  // updated for the session.
  ASSERT_THAT(decryptor_provided_callbacks, NotNull());
  ASSERT_THAT(decryptor_provided_callbacks->update_request_fn, NotNull());
  ASSERT_THAT(decryptor_provided_callbacks->key_statuses_changed_fn, NotNull());
  EXPECT_NE(actual_wrapper_generate_session_ticket,
            StarboardDrmWrapper::kInvalidTicket);

  decryptor_provided_callbacks->update_request_fn(
      &fake_drm_system_, &StarboardDrmWrapper::GetInstance(),
      actual_wrapper_generate_session_ticket, status, request_type,
      error_message, session_id,
      std::vector<uint8_t>(content.begin(), content.end()), url);
  StarboardDrmKeyId key_id = {};
  base::span<uint8_t>(key_id.identifier)
      .take_first<key.size()>()
      .copy_from(base::as_byte_span(key));
  key_id.identifier_size = key.size();
  const StarboardDrmKeyStatus key_status =
      StarboardDrmKeyStatus::kStarboardDrmKeyStatusUsable;
  decryptor_provided_callbacks->key_statuses_changed_fn(
      &fake_drm_system_, &StarboardDrmWrapper::GetInstance(), session_id,
      {key_id}, {key_status});
}

TEST_F(StarboardDrmWrapperTest, UpdatesServerCertificates) {
  const std::string payload_type = "cenc";
  const std::string init_data_str = "init_data";
  base::span<const uint8_t> init_data = base::as_byte_span(init_data_str);
  const std::string content_str = "content";
  base::span<const uint8_t> content = base::as_byte_span(content_str);
  const std::string cert_str = "server_cert";
  base::span<const uint8_t> cert = base::as_byte_span(cert_str);
  const std::string error_message = "";
  const std::string session_id = "session_id";
  const std::string url = "";
  const StarboardDrmStatus status =
      StarboardDrmStatus::kStarboardDrmStatusSuccess;
  const StarboardDrmSessionRequestType request_type =
      StarboardDrmSessionRequestType::
          kStarboardDrmSessionRequestTypeLicenseRequest;
  const int generate_session_ticket = 7;
  const int update_cert_ticket = 9;

  // This will get populated by StarboardDrmWrapper.
  const StarboardDrmSystemCallbackHandler* decryptor_provided_callbacks =
      nullptr;
  EXPECT_CALL(starboard_, CreateDrmSystem("com.widevine.alpha", _))
      .WillOnce(DoAll(SaveArg<1>(&decryptor_provided_callbacks),
                      Return(&fake_drm_system_)));
  int actual_wrapper_generate_session_ticket =
      StarboardDrmWrapper::kInvalidTicket;
  EXPECT_CALL(starboard_,
              DrmGenerateSessionUpdateRequest(
                  &fake_drm_system_, _, StrEq(payload_type),
                  StrEqWhenCast(init_data_str), init_data_str.size()))
      .WillOnce(SaveArg<1>(&actual_wrapper_generate_session_ticket));
  int actual_wrapper_update_cert_ticket = StarboardDrmWrapper::kInvalidTicket;
  EXPECT_CALL(starboard_, DrmUpdateServerCertificate(&fake_drm_system_, _,
                                                     StrEqWhenCast(cert_str),
                                                     cert_str.size()))
      .WillOnce(SaveArg<1>(&actual_wrapper_update_cert_ticket));
  StarboardDrmWrapper::SetSingletonForTesting(&starboard_);

  client_.emplace();
  EXPECT_CALL(*client_,
              OnSessionUpdateRequest(generate_session_ticket, status,
                                     request_type, error_message, session_id,
                                     ElementsAreArray(content)))
      .Times(1);
  EXPECT_CALL(*client_, OnCertificateUpdated(
                            update_cert_ticket,
                            StarboardDrmStatus::kStarboardDrmStatusSuccess, ""))
      .Times(1);

  StarboardDrmWrapper::GetInstance().GenerateSessionUpdateRequest(
      &*client_, generate_session_ticket, payload_type,
      std::vector<uint8_t>(init_data.begin(), init_data.end()));
  StarboardDrmWrapper::GetInstance().UpdateServerCertificate(
      &*client_, update_cert_ticket,
      std::vector<uint8_t>(cert.begin(), cert.end()));

  // Simulate Starboard responding, including a response that the server cert
  // has been updated.
  ASSERT_THAT(decryptor_provided_callbacks, NotNull());
  ASSERT_THAT(decryptor_provided_callbacks->update_request_fn, NotNull());
  ASSERT_THAT(decryptor_provided_callbacks->server_certificate_updated_fn,
              NotNull());
  EXPECT_NE(actual_wrapper_generate_session_ticket,
            StarboardDrmWrapper::kInvalidTicket);
  EXPECT_NE(actual_wrapper_update_cert_ticket,
            StarboardDrmWrapper::kInvalidTicket);

  decryptor_provided_callbacks->update_request_fn(
      &fake_drm_system_, &StarboardDrmWrapper::GetInstance(),
      actual_wrapper_generate_session_ticket, status, request_type,
      error_message, session_id,
      std::vector<uint8_t>(content.begin(), content.end()), url);
  decryptor_provided_callbacks->server_certificate_updated_fn(
      &fake_drm_system_, &StarboardDrmWrapper::GetInstance(),
      actual_wrapper_update_cert_ticket,
      StarboardDrmStatus::kStarboardDrmStatusSuccess, error_message.c_str());
}

TEST_F(StarboardDrmWrapperTest, ClosesSession) {
  const std::string payload_type = "cenc";
  const std::string init_data_str = "init_data";
  base::span<const uint8_t> init_data = base::as_byte_span(init_data_str);
  const std::string content_str = "content";
  base::span<const uint8_t> content = base::as_byte_span(content_str);
  const std::string error_message = "";
  const std::string session_id = "session_id";
  const std::string url = "";
  const StarboardDrmStatus status =
      StarboardDrmStatus::kStarboardDrmStatusSuccess;
  const StarboardDrmSessionRequestType request_type =
      StarboardDrmSessionRequestType::
          kStarboardDrmSessionRequestTypeLicenseRequest;
  const int generate_session_ticket = 7;

  // This will get populated by StarboardDrmWrapper.
  const StarboardDrmSystemCallbackHandler* decryptor_provided_callbacks =
      nullptr;
  EXPECT_CALL(starboard_, CreateDrmSystem("com.widevine.alpha", _))
      .WillOnce(DoAll(SaveArg<1>(&decryptor_provided_callbacks),
                      Return(&fake_drm_system_)));
  int actual_wrapper_generate_session_ticket =
      StarboardDrmWrapper::kInvalidTicket;
  EXPECT_CALL(starboard_,
              DrmGenerateSessionUpdateRequest(
                  &fake_drm_system_, _, StrEq(payload_type),
                  StrEqWhenCast(init_data_str), init_data_str.size()))
      .WillOnce(SaveArg<1>(&actual_wrapper_generate_session_ticket));
  EXPECT_CALL(starboard_,
              DrmCloseSession(&fake_drm_system_, StrEqWhenCast(session_id),
                              session_id.size()));
  StarboardDrmWrapper::SetSingletonForTesting(&starboard_);

  client_.emplace();
  EXPECT_CALL(*client_,
              OnSessionUpdateRequest(generate_session_ticket, status,
                                     request_type, error_message, session_id,
                                     ElementsAreArray(content)))
      .Times(1);
  EXPECT_CALL(*client_, OnSessionClosed(session_id)).Times(1);

  StarboardDrmWrapper::GetInstance().GenerateSessionUpdateRequest(
      &*client_, generate_session_ticket, payload_type,
      std::vector<uint8_t>(init_data.begin(), init_data.end()));
  StarboardDrmWrapper::GetInstance().CloseSession(&*client_, session_id);

  // Simulate Starboard responding.
  ASSERT_THAT(decryptor_provided_callbacks, NotNull());
  ASSERT_THAT(decryptor_provided_callbacks->update_request_fn, NotNull());
  ASSERT_THAT(decryptor_provided_callbacks->session_closed_fn, NotNull());
  EXPECT_NE(actual_wrapper_generate_session_ticket,
            StarboardDrmWrapper::kInvalidTicket);

  decryptor_provided_callbacks->update_request_fn(
      &fake_drm_system_, &StarboardDrmWrapper::GetInstance(),
      actual_wrapper_generate_session_ticket, status, request_type,
      error_message, session_id,
      std::vector<uint8_t>(content.begin(), content.end()), url);
  decryptor_provided_callbacks->session_closed_fn(
      &fake_drm_system_, &StarboardDrmWrapper::GetInstance(), session_id);
}

TEST_F(StarboardDrmWrapperTest,
       ReturnsWhetherServerCertIsUpdatableFromStarboard) {
  EXPECT_CALL(starboard_, CreateDrmSystem("com.widevine.alpha", _))
      .WillOnce(Return(&fake_drm_system_));
  EXPECT_CALL(starboard_, DrmIsServerCertificateUpdatable)
      .WillOnce(Return(true))
      .WillOnce(Return(false));

  StarboardDrmWrapper::SetSingletonForTesting(&starboard_);
  EXPECT_TRUE(
      StarboardDrmWrapper::GetInstance().IsServerCertificateUpdatable());
  EXPECT_FALSE(
      StarboardDrmWrapper::GetInstance().IsServerCertificateUpdatable());
}

TEST_F(StarboardDrmWrapperTest, RespondsToCorrectClientByTicket) {
  const std::string payload_type = "cenc";
  const std::string init_data_str = "init_data";
  base::span<const uint8_t> init_data = base::as_byte_span(init_data_str);
  const std::string content_str_1 = "content_1";
  base::span<const uint8_t> content_1 = base::as_byte_span(content_str_1);
  const std::string content_str_2 = "content_2";
  base::span<const uint8_t> content_2 = base::as_byte_span(content_str_2);
  const std::string error_message = "";
  const std::string session_id_1 = "session_id_1";
  const std::string session_id_2 = "session_id_2";
  const std::string url = "";
  const StarboardDrmStatus status =
      StarboardDrmStatus::kStarboardDrmStatusSuccess;
  const StarboardDrmSessionRequestType request_type =
      StarboardDrmSessionRequestType::
          kStarboardDrmSessionRequestTypeLicenseRequest;
  // Clients may use the same ticket; test that this is supported.
  const int ticket_1 = 7;
  const int ticket_2 = 7;

  MockStarboardDrmWrapperClient client_1;
  MockStarboardDrmWrapperClient client_2;

  // This will get populated by StarboardDrmWrapper.
  const StarboardDrmSystemCallbackHandler* decryptor_provided_callbacks =
      nullptr;
  EXPECT_CALL(starboard_, CreateDrmSystem("com.widevine.alpha", _))
      .WillOnce(DoAll(SaveArg<1>(&decryptor_provided_callbacks),
                      Return(&fake_drm_system_)));
  int actual_wrapper_ticket_1 = StarboardDrmWrapper::kInvalidTicket;
  int actual_wrapper_ticket_2 = StarboardDrmWrapper::kInvalidTicket;
  EXPECT_CALL(starboard_,
              DrmGenerateSessionUpdateRequest(
                  &fake_drm_system_, _, StrEq(payload_type),
                  StrEqWhenCast(init_data_str), init_data_str.size()))
      .WillOnce(SaveArg<1>(&actual_wrapper_ticket_1))
      .WillOnce(SaveArg<1>(&actual_wrapper_ticket_2));
  EXPECT_CALL(client_1, OnSessionUpdateRequest(ticket_1, status, request_type,
                                               error_message, session_id_1,
                                               ElementsAreArray(content_1)))
      .Times(1);
  EXPECT_CALL(client_2, OnSessionUpdateRequest(ticket_2, status, request_type,
                                               error_message, session_id_2,
                                               ElementsAreArray(content_2)))
      .Times(1);

  StarboardDrmWrapper::SetSingletonForTesting(&starboard_);
  StarboardDrmWrapper::GetInstance().GenerateSessionUpdateRequest(
      &client_1, ticket_1, payload_type,
      std::vector<uint8_t>(init_data.begin(), init_data.end()));
  StarboardDrmWrapper::GetInstance().GenerateSessionUpdateRequest(
      &client_2, ticket_2, payload_type,
      std::vector<uint8_t>(init_data.begin(), init_data.end()));

  // Simulate Starboard responding. To ensure that the logic in
  // StarboardDrmWrapper is not dependent on ordering, we respond to the second
  // session update request first.
  ASSERT_THAT(decryptor_provided_callbacks, NotNull());
  ASSERT_THAT(decryptor_provided_callbacks->update_request_fn, NotNull());
  EXPECT_NE(actual_wrapper_ticket_1, StarboardDrmWrapper::kInvalidTicket);
  EXPECT_NE(actual_wrapper_ticket_2, StarboardDrmWrapper::kInvalidTicket);
  decryptor_provided_callbacks->update_request_fn(
      &fake_drm_system_, &StarboardDrmWrapper::GetInstance(),
      actual_wrapper_ticket_2, status, request_type, error_message,
      session_id_2, std::vector<uint8_t>(content_2.begin(), content_2.end()),
      url);
  decryptor_provided_callbacks->update_request_fn(
      &fake_drm_system_, &StarboardDrmWrapper::GetInstance(),
      actual_wrapper_ticket_1, status, request_type, error_message,
      session_id_1, std::vector<uint8_t>(content_1.begin(), content_1.end()),
      url);
}

TEST_F(StarboardDrmWrapperTest, RespondsToCorrectClientBySessionId) {
  // For this test we simulate the provisoining flow. Starboard will send an
  // individualization request with an invalid ticket. StarboardDrmWrapper needs
  // to find the correct client by looking at the session ID.
  const std::string payload_type = "cenc";
  const std::string init_data_str = "init_data";
  base::span<const uint8_t> init_data = base::as_byte_span(init_data_str);
  const std::string content_str_1 = "content_1";
  base::span<const uint8_t> content_1 = base::as_byte_span(content_str_1);
  const std::string content_str_2 = "content_2";
  base::span<const uint8_t> content_2 = base::as_byte_span(content_str_2);
  const std::string provisioning_data_str = "provisioning_data";
  base::span<const uint8_t> provisioning_data =
      base::as_byte_span(provisioning_data_str);
  const std::string error_message = "";
  const std::string session_id_1 = "session_id_1";
  const std::string session_id_2 = "session_id_2";
  const std::string url = "";
  const StarboardDrmStatus status =
      StarboardDrmStatus::kStarboardDrmStatusSuccess;
  const StarboardDrmSessionRequestType request_type =
      StarboardDrmSessionRequestType::
          kStarboardDrmSessionRequestTypeLicenseRequest;
  // Clients may use the same ticket; test that this is supported.
  const int ticket_1 = 7;
  const int ticket_2 = 7;

  MockStarboardDrmWrapperClient client_1;
  MockStarboardDrmWrapperClient client_2;

  // This will get populated by StarboardDrmWrapper.
  const StarboardDrmSystemCallbackHandler* decryptor_provided_callbacks =
      nullptr;
  EXPECT_CALL(starboard_, CreateDrmSystem("com.widevine.alpha", _))
      .WillOnce(DoAll(SaveArg<1>(&decryptor_provided_callbacks),
                      Return(&fake_drm_system_)));
  int actual_wrapper_ticket_1 = StarboardDrmWrapper::kInvalidTicket;
  int actual_wrapper_ticket_2 = StarboardDrmWrapper::kInvalidTicket;
  EXPECT_CALL(starboard_,
              DrmGenerateSessionUpdateRequest(
                  &fake_drm_system_, _, StrEq(payload_type),
                  StrEqWhenCast(init_data_str), init_data_str.size()))
      .WillOnce(SaveArg<1>(&actual_wrapper_ticket_1))
      .WillOnce(SaveArg<1>(&actual_wrapper_ticket_2));
  EXPECT_CALL(client_1, OnSessionUpdateRequest(ticket_1, status, request_type,
                                               error_message, session_id_1,
                                               ElementsAreArray(content_1)))
      .Times(1);
  EXPECT_CALL(
      client_1,
      OnSessionUpdateRequest(
          StarboardDrmWrapper::kInvalidTicket, status,
          StarboardDrmSessionRequestType::
              kStarboardDrmSessionRequestTypeIndividualizationRequest,
          error_message, session_id_1, ElementsAreArray(provisioning_data)));
  EXPECT_CALL(client_2, OnSessionUpdateRequest(ticket_2, status, request_type,
                                               error_message, session_id_2,
                                               ElementsAreArray(content_2)))
      .Times(1);

  StarboardDrmWrapper::SetSingletonForTesting(&starboard_);
  StarboardDrmWrapper::GetInstance().GenerateSessionUpdateRequest(
      &client_1, ticket_1, payload_type,
      std::vector<uint8_t>(init_data.begin(), init_data.end()));
  StarboardDrmWrapper::GetInstance().GenerateSessionUpdateRequest(
      &client_2, ticket_2, payload_type,
      std::vector<uint8_t>(init_data.begin(), init_data.end()));

  // Simulate Starboard responding.
  ASSERT_THAT(decryptor_provided_callbacks, NotNull());
  ASSERT_THAT(decryptor_provided_callbacks->update_request_fn, NotNull());
  EXPECT_NE(actual_wrapper_ticket_1, StarboardDrmWrapper::kInvalidTicket);
  EXPECT_NE(actual_wrapper_ticket_2, StarboardDrmWrapper::kInvalidTicket);
  decryptor_provided_callbacks->update_request_fn(
      &fake_drm_system_, &StarboardDrmWrapper::GetInstance(),
      actual_wrapper_ticket_1, status, request_type, error_message,
      session_id_1, std::vector<uint8_t>(content_1.begin(), content_1.end()),
      url);
  decryptor_provided_callbacks->update_request_fn(
      &fake_drm_system_, &StarboardDrmWrapper::GetInstance(),
      actual_wrapper_ticket_2, status, request_type, error_message,
      session_id_2, std::vector<uint8_t>(content_2.begin(), content_2.end()),
      url);
  decryptor_provided_callbacks->update_request_fn(
      &fake_drm_system_, &StarboardDrmWrapper::GetInstance(),
      StarboardDrmWrapper::kInvalidTicket, status,
      StarboardDrmSessionRequestType::
          kStarboardDrmSessionRequestTypeIndividualizationRequest,
      error_message, session_id_1,
      std::vector<uint8_t>(provisioning_data.begin(), provisioning_data.end()),
      url);
}

TEST_F(StarboardDrmWrapperTest, DrmSystemResourceAddsToResourceSet) {
  ON_CALL(starboard_, CreateDrmSystem).WillByDefault(Return(&fake_drm_system_));
  StarboardDrmWrapper::SetSingletonForTesting(&starboard_);

  std::optional<StarboardDrmWrapper::DrmSystemResource> resource1;
  std::optional<StarboardDrmWrapper::DrmSystemResource> resource2;

  EXPECT_EQ(StarboardDrmWrapperTestPeer::GetNumResources(), 0u);

  resource1.emplace();
  EXPECT_EQ(StarboardDrmWrapperTestPeer::GetNumResources(), 1u);

  resource2.emplace();
  EXPECT_EQ(StarboardDrmWrapperTestPeer::GetNumResources(), 2u);

  resource1.reset();
  EXPECT_EQ(StarboardDrmWrapperTestPeer::GetNumResources(), 1u);

  resource2.reset();
  EXPECT_EQ(StarboardDrmWrapperTestPeer::GetNumResources(), 0u);
}

TEST_F(StarboardDrmWrapperTest,
       DrmSystemIsNotDestructedUntilAllResourcesAreDestructed) {
  bool drm_system_destroyed = false;
  EXPECT_CALL(starboard_, CreateDrmSystem("com.widevine.alpha", _))
      .WillOnce(Return(&fake_drm_system_));
  EXPECT_CALL(starboard_, DrmDestroySystem(&fake_drm_system_))
      .WillOnce(WithoutArgs(
          [&drm_system_destroyed]() { drm_system_destroyed = true; }));
  StarboardDrmWrapper::SetSingletonForTesting(&starboard_);

  std::optional<StarboardDrmWrapper::DrmSystemResource> resource1;
  std::optional<StarboardDrmWrapper::DrmSystemResource> resource2;

  resource1.emplace();
  resource2.emplace();

  // Simulate the AtExit logic via the test peer.
  StarboardDrmWrapperTestPeer::AttemptDestroySbDrmSystem();

  // Since the resources have not been destructed, the SbDrmSystem should not
  // have been destroyed.
  EXPECT_FALSE(drm_system_destroyed);

  resource1.reset();

  // resource2 still exists.
  EXPECT_FALSE(drm_system_destroyed);

  // Release the last resource; now the DRM system should be destroyed.
  resource2.reset();
  EXPECT_TRUE(drm_system_destroyed);
}

TEST_F(StarboardDrmWrapperTest,
       DrmSystemIsDestructedImmediatelyIfNoResourcesAreHeld) {
  bool drm_system_destroyed = false;
  EXPECT_CALL(starboard_, CreateDrmSystem("com.widevine.alpha", _))
      .WillOnce(Return(&fake_drm_system_));
  EXPECT_CALL(starboard_, DrmDestroySystem(&fake_drm_system_))
      .WillOnce(WithoutArgs(
          [&drm_system_destroyed]() { drm_system_destroyed = true; }));
  StarboardDrmWrapper::SetSingletonForTesting(&starboard_);

  EXPECT_FALSE(drm_system_destroyed);
  // Simulate the AtExit logic via the test peer.
  StarboardDrmWrapperTestPeer::AttemptDestroySbDrmSystem();
  EXPECT_TRUE(drm_system_destroyed);
}

}  // namespace
}  // namespace media
}  // namespace chromecast
