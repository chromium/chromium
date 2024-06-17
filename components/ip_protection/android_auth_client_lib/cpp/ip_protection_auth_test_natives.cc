// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <ostream>

#include "base/android/jni_android.h"
#include "base/check.h"
#include "base/run_loop.h"
#include "base/task/bind_post_task.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_timeouts.h"
#include "components/ip_protection/android_auth_client_lib/cpp/ip_protection_auth_client.h"
#include "components/ip_protection/android_auth_client_lib/cpp/ip_protection_auth_client_interface.h"
#include "components/ip_protection/android_auth_client_lib/javatests/jni_headers/IpProtectionAuthTestNatives_jni.h"
#include "net/third_party/quiche/src/quiche/blind_sign_auth/proto/auth_and_sign.pb.h"
#include "net/third_party/quiche/src/quiche/blind_sign_auth/proto/get_initial_data.pb.h"

namespace {

const char kMockPackageName[] = "org.chromium.components.ip_protection_auth";

const char kMockClassNameForDefault[] =
    "org.chromium.components.ip_protection_auth.mock_service."
    "IpProtectionAuthServiceMock";
const char kMockClassNameForTransientError[] =
    "org.chromium.components.ip_protection_auth.mock_service."
    "ConstantResponseService$TransientError";
const char kMockClassNameForPersistentError[] =
    "org.chromium.components.ip_protection_auth.mock_service."
    "ConstantResponseService$PersistentError";
const char kMockClassNameForIllegalErrorCode[] =
    "org.chromium.components.ip_protection_auth.mock_service."
    "ConstantResponseService$IllegalErrorCode";
const char kMockClassNameForNullResponse[] =
    "org.chromium.components.ip_protection_auth.mock_service."
    "ConstantResponseService$Null";
const char kMockClassNameForUnparsableResponse[] =
    "org.chromium.components.ip_protection_auth.mock_service."
    "ConstantResponseService$Unparsable";
const char kMockClassNameForSynchronousError[] =
    "org.chromium.components.ip_protection_auth.mock_service."
    "ConstantResponseService$SynchronousError";

// Perform an IpProtectionAuthClient::CreateConnectedInstanceForTesting call in
// a RunLoop, quitting the RunLoop when either a client is ready or an error
// occurs. Returns the resulting base::expected value.
//
// A task environment must already be set up.
//
// Beware that this only wraps the _creation_ of an IpProtectionAuthClient in a
// RunLoop and not its destruction! If any tasks need to be run as part of
// client destruction, such as handling unresolved getInitialData/authAndSign
// callbacks, this is the responsibility of the caller (or client owner).
base::expected<
    std::unique_ptr<ip_protection::android::IpProtectionAuthClientInterface>,
    std::string>
CreateClientBlocking(const std::string_view mockClassName) {
  base::expected<
      std::unique_ptr<ip_protection::android::IpProtectionAuthClientInterface>,
      std::string>
      result;
  base::RunLoop run_loop;
  ip_protection::android::IpProtectionAuthClient::
      CreateConnectedInstanceForTesting(
          kMockPackageName, mockClassName,
          base::BindPostTask(
              base::SequencedTaskRunner::GetCurrentDefault(),
              base::BindLambdaForTesting(
                  [&run_loop, &result](
                      base::expected<
                          std::unique_ptr<ip_protection::android::
                                              IpProtectionAuthClientInterface>,
                          std::string> maybe_client) {
                    result = std::move(maybe_client);
                    run_loop.Quit();
                  })));
  run_loop.Run();
  return result;
}

// Same as CreateClientBlocking, but asserts that a client is acquired and
// returns the unwrapped client from the base::expected.
//
// A task environment must already be set up.
std::unique_ptr<ip_protection::android::IpProtectionAuthClientInterface>
CreateAndExpectClientBlocking(const std::string_view mockClassName) {
  base::expected<
      std::unique_ptr<ip_protection::android::IpProtectionAuthClientInterface>,
      std::string>
      client = CreateClientBlocking(mockClassName);
  CHECK(client.has_value()) << client.error();
  return std::move(*client);
}

// Perform an getInitialData request in a RunLoop, quitting the RunLoop when a
// response is received and returning the base::expected result.
//
// A task environment must already be set up.
base::expected<privacy::ppn::GetInitialDataResponse,
               ip_protection::android::AuthRequestError>
GetInitialDataBlocking(
    ip_protection::android::IpProtectionAuthClientInterface& client,
    privacy::ppn::GetInitialDataRequest request) {
  base::expected<privacy::ppn::GetInitialDataResponse,
                 ip_protection::android::AuthRequestError>
      result;
  base::RunLoop run_loop;
  client.GetInitialData(
      std::move(request),
      base::BindLambdaForTesting(
          [&run_loop,
           &result](base::expected<privacy::ppn::GetInitialDataResponse,
                                   ip_protection::android::AuthRequestError>
                        response) {
            result = std::move(response);
            run_loop.Quit();
          }));
  run_loop.Run();
  return result;
}

// Perform an authAndSign request in a RunLoop, quitting the RunLoop when a
// response is received and returning the base::expected result.
base::expected<privacy::ppn::AuthAndSignResponse,
               ip_protection::android::AuthRequestError>
AuthAndSignBlocking(
    ip_protection::android::IpProtectionAuthClientInterface& client,
    privacy::ppn::AuthAndSignRequest request) {
  base::expected<privacy::ppn::AuthAndSignResponse,
                 ip_protection::android::AuthRequestError>
      result;
  base::RunLoop run_loop;
  client.AuthAndSign(
      std::move(request),
      base::BindLambdaForTesting(
          [&run_loop,
           &result](base::expected<privacy::ppn::AuthAndSignResponse,
                                   ip_protection::android::AuthRequestError>
                        response) {
            result = std::move(response);
            run_loop.Quit();
          }));
  run_loop.Run();
  return result;
}

}  // namespace

static void JNI_IpProtectionAuthTestNatives_Initialize(JNIEnv* env) {
  // TaskEnvironment requires TestTimeouts::Initialize() to be called in order
  // to run posted tasks. It must be run exactly once, so this function is
  // called in the static initializer of IpProtectionAuthTestNatives.java.
  TestTimeouts::Initialize();
}

static void JNI_IpProtectionAuthTestNatives_CreateConnectedInstanceForTesting(
    JNIEnv* env) {
  base::test::SingleThreadTaskEnvironment task_environment;

  CreateAndExpectClientBlocking(kMockClassNameForDefault);
}

static void JNI_IpProtectionAuthTestNatives_TestGetInitialData(JNIEnv* env) {
  base::test::SingleThreadTaskEnvironment task_environment;
  std::unique_ptr<ip_protection::android::IpProtectionAuthClientInterface>
      client = CreateAndExpectClientBlocking(kMockClassNameForDefault);
  privacy::ppn::GetInitialDataRequest get_initial_data_request;
  get_initial_data_request.set_service_type("webviewipblinding");

  base::expected<privacy::ppn::GetInitialDataResponse,
                 ip_protection::android::AuthRequestError>
      get_initial_data_response =
          GetInitialDataBlocking(*client, get_initial_data_request);

  CHECK(get_initial_data_response.has_value());
  CHECK(get_initial_data_response->privacy_pass_data().token_key_id() == "test")
      << "expected \"test\" for token_key_id, got: "
      << get_initial_data_response->privacy_pass_data().token_key_id();
}

static void JNI_IpProtectionAuthTestNatives_TestAuthAndSign(JNIEnv* env) {
  base::test::SingleThreadTaskEnvironment task_environment;
  std::unique_ptr<ip_protection::android::IpProtectionAuthClientInterface>
      client = CreateAndExpectClientBlocking(kMockClassNameForDefault);
  privacy::ppn::AuthAndSignRequest auth_and_sign_request;
  auth_and_sign_request.set_oauth_token("test");

  base::expected<privacy::ppn::AuthAndSignResponse,
                 ip_protection::android::AuthRequestError>
      auth_and_sign_response =
          AuthAndSignBlocking(*client, auth_and_sign_request);

  CHECK(auth_and_sign_response.has_value());
  CHECK(auth_and_sign_response->apn_type() == "test")
      << "expected \"test\" for apn_type, got: "
      << auth_and_sign_response->apn_type();
}

static void JNI_IpProtectionAuthTestNatives_TestTransientError(JNIEnv* env) {
  base::test::SingleThreadTaskEnvironment task_environment;
  std::unique_ptr<ip_protection::android::IpProtectionAuthClientInterface>
      client = CreateAndExpectClientBlocking(kMockClassNameForTransientError);

  base::expected<privacy::ppn::GetInitialDataResponse,
                 ip_protection::android::AuthRequestError>
      get_initial_data_response = GetInitialDataBlocking(
          *client, privacy::ppn::GetInitialDataRequest());
  base::expected<privacy::ppn::AuthAndSignResponse,
                 ip_protection::android::AuthRequestError>
      auth_and_sign_response =
          AuthAndSignBlocking(*client, privacy::ppn::AuthAndSignRequest());

  CHECK(!get_initial_data_response.has_value());
  CHECK(get_initial_data_response.error() ==
        ip_protection::android::AuthRequestError::kTransient);
  CHECK(!auth_and_sign_response.has_value());
  CHECK(auth_and_sign_response.error() ==
        ip_protection::android::AuthRequestError::kTransient);
}

static void JNI_IpProtectionAuthTestNatives_TestPersistentError(JNIEnv* env) {
  base::test::SingleThreadTaskEnvironment task_environment;
  std::unique_ptr<ip_protection::android::IpProtectionAuthClientInterface>
      client = CreateAndExpectClientBlocking(kMockClassNameForPersistentError);

  base::expected<privacy::ppn::GetInitialDataResponse,
                 ip_protection::android::AuthRequestError>
      get_initial_data_response = GetInitialDataBlocking(
          *client, privacy::ppn::GetInitialDataRequest());
  base::expected<privacy::ppn::AuthAndSignResponse,
                 ip_protection::android::AuthRequestError>
      auth_and_sign_response =
          AuthAndSignBlocking(*client, privacy::ppn::AuthAndSignRequest());

  CHECK(!get_initial_data_response.has_value());
  CHECK(get_initial_data_response.error() ==
        ip_protection::android::AuthRequestError::kPersistent);
  CHECK(!auth_and_sign_response.has_value());
  CHECK(auth_and_sign_response.error() ==
        ip_protection::android::AuthRequestError::kPersistent);
}

static void JNI_IpProtectionAuthTestNatives_TestIllegalErrorCode(JNIEnv* env) {
  base::test::SingleThreadTaskEnvironment task_environment;
  std::unique_ptr<ip_protection::android::IpProtectionAuthClientInterface>
      client = CreateAndExpectClientBlocking(kMockClassNameForIllegalErrorCode);

  base::expected<privacy::ppn::GetInitialDataResponse,
                 ip_protection::android::AuthRequestError>
      get_initial_data_response = GetInitialDataBlocking(
          *client, privacy::ppn::GetInitialDataRequest());
  base::expected<privacy::ppn::AuthAndSignResponse,
                 ip_protection::android::AuthRequestError>
      auth_and_sign_response =
          AuthAndSignBlocking(*client, privacy::ppn::AuthAndSignRequest());

  CHECK(!get_initial_data_response.has_value());
  CHECK(get_initial_data_response.error() ==
        ip_protection::android::AuthRequestError::kOther);
  CHECK(!auth_and_sign_response.has_value());
  CHECK(auth_and_sign_response.error() ==
        ip_protection::android::AuthRequestError::kOther);
}

static void JNI_IpProtectionAuthTestNatives_TestNullResponse(JNIEnv* env) {
  base::test::SingleThreadTaskEnvironment task_environment;
  std::unique_ptr<ip_protection::android::IpProtectionAuthClientInterface>
      client = CreateAndExpectClientBlocking(kMockClassNameForNullResponse);

  base::expected<privacy::ppn::GetInitialDataResponse,
                 ip_protection::android::AuthRequestError>
      get_initial_data_response = GetInitialDataBlocking(
          *client, privacy::ppn::GetInitialDataRequest());
  base::expected<privacy::ppn::AuthAndSignResponse,
                 ip_protection::android::AuthRequestError>
      auth_and_sign_response =
          AuthAndSignBlocking(*client, privacy::ppn::AuthAndSignRequest());

  CHECK(!get_initial_data_response.has_value());
  CHECK(get_initial_data_response.error() ==
        ip_protection::android::AuthRequestError::kOther);
  CHECK(!auth_and_sign_response.has_value());
  CHECK(auth_and_sign_response.error() ==
        ip_protection::android::AuthRequestError::kOther);
}

static void JNI_IpProtectionAuthTestNatives_TestUnparsableResponse(
    JNIEnv* env) {
  base::test::SingleThreadTaskEnvironment task_environment;
  std::unique_ptr<ip_protection::android::IpProtectionAuthClientInterface>
      client =
          CreateAndExpectClientBlocking(kMockClassNameForUnparsableResponse);

  base::expected<privacy::ppn::GetInitialDataResponse,
                 ip_protection::android::AuthRequestError>
      get_initial_data_response = GetInitialDataBlocking(
          *client, privacy::ppn::GetInitialDataRequest());
  base::expected<privacy::ppn::AuthAndSignResponse,
                 ip_protection::android::AuthRequestError>
      auth_and_sign_response =
          AuthAndSignBlocking(*client, privacy::ppn::AuthAndSignRequest());

  CHECK(!get_initial_data_response.has_value());
  CHECK(get_initial_data_response.error() ==
        ip_protection::android::AuthRequestError::kOther);
  CHECK(!auth_and_sign_response.has_value());
  CHECK(auth_and_sign_response.error() ==
        ip_protection::android::AuthRequestError::kOther);
}

static void JNI_IpProtectionAuthTestNatives_TestSynchronousError(JNIEnv* env) {
  base::test::SingleThreadTaskEnvironment task_environment;
  std::unique_ptr<ip_protection::android::IpProtectionAuthClientInterface>
      client = CreateAndExpectClientBlocking(kMockClassNameForSynchronousError);

  base::expected<privacy::ppn::GetInitialDataResponse,
                 ip_protection::android::AuthRequestError>
      get_initial_data_response = GetInitialDataBlocking(
          *client, privacy::ppn::GetInitialDataRequest());
  base::expected<privacy::ppn::AuthAndSignResponse,
                 ip_protection::android::AuthRequestError>
      auth_and_sign_response =
          AuthAndSignBlocking(*client, privacy::ppn::AuthAndSignRequest());

  CHECK(!get_initial_data_response.has_value());
  CHECK(get_initial_data_response.error() ==
        ip_protection::android::AuthRequestError::kOther);
  CHECK(!auth_and_sign_response.has_value());
  CHECK(auth_and_sign_response.error() ==
        ip_protection::android::AuthRequestError::kOther);
}
