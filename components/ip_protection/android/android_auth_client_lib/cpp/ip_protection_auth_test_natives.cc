// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <ostream>

#include "base/android/jni_android.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/run_loop.h"
#include "base/task/bind_post_task.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_timeouts.h"
#include "components/ip_protection/android/android_auth_client_lib/cpp/ip_protection_auth_client.h"
#include "components/ip_protection/android/android_auth_client_lib/cpp/ip_protection_auth_client_interface.h"
#include "components/ip_protection/android/android_auth_client_lib/javatests/jni_headers/IpProtectionAuthTestNatives_jni.h"
#include "components/ip_protection/get_proxy_config.pb.h"
#include "net/third_party/quiche/src/quiche/blind_sign_auth/proto/auth_and_sign.pb.h"
#include "net/third_party/quiche/src/quiche/blind_sign_auth/proto/get_initial_data.pb.h"

namespace {

const char kMockPackageName[] = "org.chromium.components.ip_protection_auth";

const char kMockClassNameForDefault[] =
    "org.chromium.components.ip_protection_auth.mock_service."
    "IpProtectionAuthServiceMock";
const char kMockClassNameForNonexistant[] =
    "org.chromium.components.ip_protection_auth.mock_service."
    "IntentionallyNonexistantClass";
const char kMockClassNameForNullBinding[] =
    "org.chromium.components.ip_protection_auth.mock_service."
    "NullBindingService";
const char kMockClassNameForDisabled[] =
    "org.chromium.components.ip_protection_auth.mock_service."
    "NullBindingService$DisabledService";
const char kMockClassNameForRestricted[] =
    "org.chromium.components.ip_protection_auth.mock_service."
    "NullBindingService$RestrictedService";
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
const char kMockClassNameForNeverResolve[] =
    "org.chromium.components.ip_protection_auth.mock_service."
    "ConstantResponseService$NeverResolve";
const char kMockClassNameForCrashOnRequestSyncWithoutResponse[] =
    "org.chromium.components.ip_protection_auth.mock_service."
    "CrashingService$CrashOnRequestSyncWithoutResponse";
const char kMockClassNameForCrashOnRequestAsyncWithoutResponse[] =
    "org.chromium.components.ip_protection_auth.mock_service."
    "CrashingService$CrashOnRequestAsyncWithoutResponse";
const char kMockClassNameForCrashOnRequestSyncWithResponse[] =
    "org.chromium.components.ip_protection_auth.mock_service."
    "CrashingService$CrashOnRequestSyncWithResponse";
const char kMockClassNameForCrashAfterTwoRequestsSyncWithoutResponses[] =
    "org.chromium.components.ip_protection_auth.mock_service."
    "CrashingService$CrashAfterTwoRequestsSyncWithoutResponses";

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
CreateClientBlocking(std::string_view mockClassName) {
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
CreateAndExpectClientBlocking(std::string_view mockClassName) {
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
      base::BindPostTask(
          base::SequencedTaskRunner::GetCurrentDefault(),
          base::BindLambdaForTesting(
              [&run_loop,
               &result](base::expected<privacy::ppn::GetInitialDataResponse,
                                       ip_protection::android::AuthRequestError>
                            response) {
                result = std::move(response);
                run_loop.Quit();
              })));
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
      base::BindPostTask(
          base::SequencedTaskRunner::GetCurrentDefault(),
          base::BindLambdaForTesting(
              [&run_loop,
               &result](base::expected<privacy::ppn::AuthAndSignResponse,
                                       ip_protection::android::AuthRequestError>
                            response) {
                result = std::move(response);
                run_loop.Quit();
              })));
  run_loop.Run();
  return result;
}

// Perform a getProxyConfig request in a RunLoop, quitting the RunLoop when a
// response is received and returning the base::expected result.
base::expected<ip_protection::GetProxyConfigResponse,
               ip_protection::android::AuthRequestError>
GetProxyConfigBlocking(
    ip_protection::android::IpProtectionAuthClientInterface& client,
    ip_protection::GetProxyConfigRequest request) {
  base::expected<ip_protection::GetProxyConfigResponse,
                 ip_protection::android::AuthRequestError>
      result;
  base::RunLoop run_loop;
  client.GetProxyConfig(
      std::move(request),
      base::BindPostTask(
          base::SequencedTaskRunner::GetCurrentDefault(),
          base::BindLambdaForTesting(
              [&run_loop,
               &result](base::expected<ip_protection::GetProxyConfigResponse,
                                       ip_protection::android::AuthRequestError>
                            response) {
                result = std::move(response);
                run_loop.Quit();
              })));
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

// Tests a normal case where the service is available.
static void JNI_IpProtectionAuthTestNatives_CreateConnectedInstanceForTesting(
    JNIEnv* env) {
  base::test::SingleThreadTaskEnvironment task_environment;

  CreateAndExpectClientBlocking(kMockClassNameForDefault);
}

// Tests a normal case where the service isn't installed.
static void JNI_IpProtectionAuthTestNatives_TestNonexistantService(
    JNIEnv* env) {
  base::test::SingleThreadTaskEnvironment task_environment;
  base::expected<
      std::unique_ptr<ip_protection::android::IpProtectionAuthClientInterface>,
      std::string>
      client = CreateClientBlocking(kMockClassNameForNonexistant);

  const std::string expected_error =
      "Unable to locate the IP Protection authentication provider package "
      "(android.net.http.IpProtectionAuthService action). This is expected if "
      "the host system is not set up to provide IP Protection services.";

  CHECK(!client.has_value());
  CHECK_EQ(client.error(), expected_error);
}

// Tests a normal case where the service is installed but rejects bindings, for
// example, due to a feature flag.
static void JNI_IpProtectionAuthTestNatives_TestNullBindingService(
    JNIEnv* env) {
  base::test::SingleThreadTaskEnvironment task_environment;
  base::expected<
      std::unique_ptr<ip_protection::android::IpProtectionAuthClientInterface>,
      std::string>
      client = CreateClientBlocking(kMockClassNameForNullBinding);

  const std::string expected_error = "Service returned null from onBind()";

  CHECK(!client.has_value());
  CHECK_EQ(client.error(), expected_error);
}

// Tests an abnormal case where bindService would return false.
static void JNI_IpProtectionAuthTestNatives_TestDisabledService(JNIEnv* env) {
  base::test::SingleThreadTaskEnvironment task_environment;
  base::expected<
      std::unique_ptr<ip_protection::android::IpProtectionAuthClientInterface>,
      std::string>
      client = CreateClientBlocking(kMockClassNameForDisabled);

  const std::string expected_error = "bindService() failed: returned false";

  CHECK(!client.has_value());
  CHECK_EQ(client.error(), expected_error);
}

// Tests an abnormal case where bindService fails with a SecurityException. Note
// that the only permission which would apply in production is the INTERNET
// permission. IpProtectionAuthClient code shouldn't be reached if the app
// doesn't have INTERNET permission. This test case instead tests for resiliance
// against an unusual service implementation with unexpected restrictions.
static void JNI_IpProtectionAuthTestNatives_TestRestrictedService(JNIEnv* env) {
  base::test::SingleThreadTaskEnvironment task_environment;
  base::expected<
      std::unique_ptr<ip_protection::android::IpProtectionAuthClientInterface>,
      std::string>
      client = CreateClientBlocking(kMockClassNameForRestricted);

  const std::string expected_error_substr =
      "Failed to bind service: java.lang.SecurityException: ";

  CHECK(!client.has_value());
  CHECK(base::Contains(client.error(), expected_error_substr));
}

static void JNI_IpProtectionAuthTestNatives_TestGetInitialData(JNIEnv* env) {
  base::test::SingleThreadTaskEnvironment task_environment;
  std::unique_ptr<ip_protection::android::IpProtectionAuthClientInterface>
      client = CreateAndExpectClientBlocking(kMockClassNameForDefault);
  // TODO(b/344853279): This request is missing required fields. It should be
  // updated.
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
  // TODO(b/344853279): This request has deprecated OAuth field and is missing
  // required fields. It should be updated.
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

static void JNI_IpProtectionAuthTestNatives_TestGetProxyConfig(JNIEnv* env) {
  base::test::SingleThreadTaskEnvironment task_environment;
  std::unique_ptr<ip_protection::android::IpProtectionAuthClientInterface>
      client = CreateAndExpectClientBlocking(kMockClassNameForDefault);
  // TODO(b/344853279): This request is missing required fields. It should be
  // updated.
  ip_protection::GetProxyConfigRequest get_proxy_config_request;
  get_proxy_config_request.set_service_type("webviewipblinding");

  base::expected<ip_protection::GetProxyConfigResponse,
                 ip_protection::android::AuthRequestError>
      get_proxy_config_response =
          GetProxyConfigBlocking(*client, get_proxy_config_request);

  CHECK(get_proxy_config_response.has_value());
  CHECK_EQ(get_proxy_config_response->proxy_chain_size(), 1);
  CHECK_EQ(get_proxy_config_response->proxy_chain(0).proxy_a(), "test");
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
  base::expected<ip_protection::GetProxyConfigResponse,
                 ip_protection::android::AuthRequestError>
      get_proxy_config_response = GetProxyConfigBlocking(
          *client, ip_protection::GetProxyConfigRequest());

  CHECK(!get_initial_data_response.has_value());
  CHECK(get_initial_data_response.error() ==
        ip_protection::android::AuthRequestError::kTransient);
  CHECK(!auth_and_sign_response.has_value());
  CHECK(auth_and_sign_response.error() ==
        ip_protection::android::AuthRequestError::kTransient);
  CHECK(!get_proxy_config_response.has_value());
  CHECK(get_proxy_config_response.error() ==
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
  base::expected<ip_protection::GetProxyConfigResponse,
                 ip_protection::android::AuthRequestError>
      get_proxy_config_response = GetProxyConfigBlocking(
          *client, ip_protection::GetProxyConfigRequest());

  CHECK(!get_initial_data_response.has_value());
  CHECK(get_initial_data_response.error() ==
        ip_protection::android::AuthRequestError::kPersistent);
  CHECK(!auth_and_sign_response.has_value());
  CHECK(auth_and_sign_response.error() ==
        ip_protection::android::AuthRequestError::kPersistent);
  CHECK(!get_proxy_config_response.has_value());
  CHECK(get_proxy_config_response.error() ==
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
  base::expected<ip_protection::GetProxyConfigResponse,
                 ip_protection::android::AuthRequestError>
      get_proxy_config_response = GetProxyConfigBlocking(
          *client, ip_protection::GetProxyConfigRequest());

  CHECK(!get_initial_data_response.has_value());
  CHECK(get_initial_data_response.error() ==
        ip_protection::android::AuthRequestError::kOther);
  CHECK(!auth_and_sign_response.has_value());
  CHECK(auth_and_sign_response.error() ==
        ip_protection::android::AuthRequestError::kOther);
  CHECK(!get_proxy_config_response.has_value());
  CHECK(get_proxy_config_response.error() ==
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
  base::expected<ip_protection::GetProxyConfigResponse,
                 ip_protection::android::AuthRequestError>
      get_proxy_config_response = GetProxyConfigBlocking(
          *client, ip_protection::GetProxyConfigRequest());

  CHECK(!get_initial_data_response.has_value());
  CHECK(get_initial_data_response.error() ==
        ip_protection::android::AuthRequestError::kOther);
  CHECK(!auth_and_sign_response.has_value());
  CHECK(auth_and_sign_response.error() ==
        ip_protection::android::AuthRequestError::kOther);
  CHECK(!get_proxy_config_response.has_value());
  CHECK(get_proxy_config_response.error() ==
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
  base::expected<ip_protection::GetProxyConfigResponse,
                 ip_protection::android::AuthRequestError>
      get_proxy_config_response = GetProxyConfigBlocking(
          *client, ip_protection::GetProxyConfigRequest());

  CHECK(!get_initial_data_response.has_value());
  CHECK(get_initial_data_response.error() ==
        ip_protection::android::AuthRequestError::kOther);
  CHECK(!auth_and_sign_response.has_value());
  CHECK(auth_and_sign_response.error() ==
        ip_protection::android::AuthRequestError::kOther);
  CHECK(!get_proxy_config_response.has_value());
  CHECK(get_proxy_config_response.error() ==
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
  base::expected<ip_protection::GetProxyConfigResponse,
                 ip_protection::android::AuthRequestError>
      get_proxy_config_response = GetProxyConfigBlocking(
          *client, ip_protection::GetProxyConfigRequest());

  CHECK(!get_initial_data_response.has_value());
  CHECK(get_initial_data_response.error() ==
        ip_protection::android::AuthRequestError::kOther);
  CHECK(!auth_and_sign_response.has_value());
  CHECK(auth_and_sign_response.error() ==
        ip_protection::android::AuthRequestError::kOther);
  CHECK(!get_proxy_config_response.has_value());
  CHECK(get_proxy_config_response.error() ==
        ip_protection::android::AuthRequestError::kOther);
}

static void JNI_IpProtectionAuthTestNatives_TestUnresolvedWhenClosed(
    JNIEnv* env) {
  base::test::SingleThreadTaskEnvironment task_environment;
  base::expected<privacy::ppn::GetInitialDataResponse,
                 ip_protection::android::AuthRequestError>
      get_initial_data_response;
  base::expected<privacy::ppn::AuthAndSignResponse,
                 ip_protection::android::AuthRequestError>
      auth_and_sign_response;
  base::expected<ip_protection::GetProxyConfigResponse,
                 ip_protection::android::AuthRequestError>
      get_proxy_config_response;

  int remaining = 3;
  base::RunLoop run_loop;
  {
    std::unique_ptr<ip_protection::android::IpProtectionAuthClientInterface>
        client = CreateAndExpectClientBlocking(kMockClassNameForNeverResolve);
    client->GetInitialData(
        privacy::ppn::GetInitialDataRequest(),
        base::BindPostTask(
            base::SequencedTaskRunner::GetCurrentDefault(),
            base::BindLambdaForTesting(
                [&run_loop, &remaining, &get_initial_data_response](
                    base::expected<privacy::ppn::GetInitialDataResponse,
                                   ip_protection::android::AuthRequestError>
                        response) {
                  get_initial_data_response = std::move(response);
                  remaining--;
                  if (remaining == 0) {
                    run_loop.Quit();
                  }
                })));
    client->AuthAndSign(
        privacy::ppn::AuthAndSignRequest(),
        base::BindPostTask(
            base::SequencedTaskRunner::GetCurrentDefault(),
            base::BindLambdaForTesting(
                [&run_loop, &remaining, &auth_and_sign_response](
                    base::expected<privacy::ppn::AuthAndSignResponse,
                                   ip_protection::android::AuthRequestError>
                        response) {
                  auth_and_sign_response = std::move(response);
                  remaining--;
                  if (remaining == 0) {
                    run_loop.Quit();
                  }
                })));
    client->GetProxyConfig(
        ip_protection::GetProxyConfigRequest(),
        base::BindPostTask(
            base::SequencedTaskRunner::GetCurrentDefault(),
            base::BindLambdaForTesting(
                [&run_loop, &remaining, &get_proxy_config_response](
                    base::expected<ip_protection::GetProxyConfigResponse,
                                   ip_protection::android::AuthRequestError>
                        response) {
                  get_proxy_config_response = std::move(response);
                  remaining--;
                  if (remaining == 0) {
                    run_loop.Quit();
                  }
                })));
    // client closes when out of scope
  }
  run_loop.Run();

  CHECK(!get_initial_data_response.has_value());
  CHECK(get_initial_data_response.error() ==
        ip_protection::android::AuthRequestError::kOther);
  CHECK(!auth_and_sign_response.has_value());
  CHECK(auth_and_sign_response.error() ==
        ip_protection::android::AuthRequestError::kOther);
  CHECK(!get_proxy_config_response.has_value());
  CHECK(get_proxy_config_response.error() ==
        ip_protection::android::AuthRequestError::kOther);
}

// Service crashes whilst handling the binder call, without calling the
// callback. The client must synthetically report an error via the callback.
static void
JNI_IpProtectionAuthTestNatives_TestCrashOnRequestSyncWithoutResponse(
    JNIEnv* env) {
  base::test::SingleThreadTaskEnvironment task_environment;
  base::expected<privacy::ppn::GetInitialDataResponse,
                 ip_protection::android::AuthRequestError>
      get_initial_data_response;
  base::expected<privacy::ppn::AuthAndSignResponse,
                 ip_protection::android::AuthRequestError>
      auth_and_sign_response;
  base::expected<ip_protection::GetProxyConfigResponse,
                 ip_protection::android::AuthRequestError>
      get_proxy_config_response;

  {
    std::unique_ptr<ip_protection::android::IpProtectionAuthClientInterface>
        client = CreateAndExpectClientBlocking(
            kMockClassNameForCrashOnRequestSyncWithoutResponse);
    get_initial_data_response =
        GetInitialDataBlocking(*client, privacy::ppn::GetInitialDataRequest());
  }
  {
    std::unique_ptr<ip_protection::android::IpProtectionAuthClientInterface>
        client = CreateAndExpectClientBlocking(
            kMockClassNameForCrashOnRequestSyncWithoutResponse);
    auth_and_sign_response =
        AuthAndSignBlocking(*client, privacy::ppn::AuthAndSignRequest());
  }
  {
    std::unique_ptr<ip_protection::android::IpProtectionAuthClientInterface>
        client = CreateAndExpectClientBlocking(
            kMockClassNameForCrashOnRequestSyncWithoutResponse);
    get_proxy_config_response =
        GetProxyConfigBlocking(*client, ip_protection::GetProxyConfigRequest());
  }

  CHECK(!get_initial_data_response.has_value());
  CHECK(get_initial_data_response.error() ==
        ip_protection::android::AuthRequestError::kOther);
  CHECK(!auth_and_sign_response.has_value());
  CHECK(auth_and_sign_response.error() ==
        ip_protection::android::AuthRequestError::kOther);
  CHECK(!get_proxy_config_response.has_value());
  CHECK(get_proxy_config_response.error() ==
        ip_protection::android::AuthRequestError::kOther);
}

// Service handles the binder call, without calling the callback, and then later
// crashes in the background. The client must detect this and synthetically
// report an error via the callback.
static void
JNI_IpProtectionAuthTestNatives_TestCrashOnRequestAsyncWithoutResponse(
    JNIEnv* env) {
  base::test::SingleThreadTaskEnvironment task_environment;
  base::expected<privacy::ppn::GetInitialDataResponse,
                 ip_protection::android::AuthRequestError>
      get_initial_data_response;
  base::expected<privacy::ppn::AuthAndSignResponse,
                 ip_protection::android::AuthRequestError>
      auth_and_sign_response;
  base::expected<ip_protection::GetProxyConfigResponse,
                 ip_protection::android::AuthRequestError>
      get_proxy_config_response;

  {
    std::unique_ptr<ip_protection::android::IpProtectionAuthClientInterface>
        client = CreateAndExpectClientBlocking(
            kMockClassNameForCrashOnRequestAsyncWithoutResponse);
    get_initial_data_response =
        GetInitialDataBlocking(*client, privacy::ppn::GetInitialDataRequest());
  }
  {
    std::unique_ptr<ip_protection::android::IpProtectionAuthClientInterface>
        client = CreateAndExpectClientBlocking(
            kMockClassNameForCrashOnRequestAsyncWithoutResponse);
    auth_and_sign_response =
        AuthAndSignBlocking(*client, privacy::ppn::AuthAndSignRequest());
  }
  {
    std::unique_ptr<ip_protection::android::IpProtectionAuthClientInterface>
        client = CreateAndExpectClientBlocking(
            kMockClassNameForCrashOnRequestSyncWithoutResponse);
    get_proxy_config_response =
        GetProxyConfigBlocking(*client, ip_protection::GetProxyConfigRequest());
  }

  CHECK(!get_initial_data_response.has_value());
  CHECK(get_initial_data_response.error() ==
        ip_protection::android::AuthRequestError::kOther);
  CHECK(!auth_and_sign_response.has_value());
  CHECK(auth_and_sign_response.error() ==
        ip_protection::android::AuthRequestError::kOther);
  CHECK(!get_proxy_config_response.has_value());
  CHECK(get_proxy_config_response.error() ==
        ip_protection::android::AuthRequestError::kOther);
}

// Service resolves the callback and then crashes whilst still handling the
// binder call. The client must tollerate both error flows.
//
// It is not strictly deterministic as to which error flow runs first!
static void JNI_IpProtectionAuthTestNatives_TestCrashOnRequestSyncWithResponse(
    JNIEnv* env) {
  base::test::SingleThreadTaskEnvironment task_environment;
  base::expected<privacy::ppn::GetInitialDataResponse,
                 ip_protection::android::AuthRequestError>
      get_initial_data_response;
  base::expected<privacy::ppn::AuthAndSignResponse,
                 ip_protection::android::AuthRequestError>
      auth_and_sign_response;
  base::expected<ip_protection::GetProxyConfigResponse,
                 ip_protection::android::AuthRequestError>
      get_proxy_config_response;

  {
    std::unique_ptr<ip_protection::android::IpProtectionAuthClientInterface>
        client = CreateAndExpectClientBlocking(
            kMockClassNameForCrashOnRequestSyncWithResponse);
    get_initial_data_response =
        GetInitialDataBlocking(*client, privacy::ppn::GetInitialDataRequest());
  }
  {
    std::unique_ptr<ip_protection::android::IpProtectionAuthClientInterface>
        client = CreateAndExpectClientBlocking(
            kMockClassNameForCrashOnRequestSyncWithResponse);
    auth_and_sign_response =
        AuthAndSignBlocking(*client, privacy::ppn::AuthAndSignRequest());
  }
  {
    std::unique_ptr<ip_protection::android::IpProtectionAuthClientInterface>
        client = CreateAndExpectClientBlocking(
            kMockClassNameForCrashOnRequestSyncWithoutResponse);
    get_proxy_config_response =
        GetProxyConfigBlocking(*client, ip_protection::GetProxyConfigRequest());
  }

  // The synchronous error handler and the callback handler may theoretically
  // race, so don't assert between kTransient and kOther.
  //
  // The most important result is that the combination of two error paths don't
  // result in a crash.
  CHECK(!get_initial_data_response.has_value());
  CHECK(!auth_and_sign_response.has_value());
  CHECK(!get_proxy_config_response.has_value());
}

// Client tries to send three requests:
// - Service will handle the first call (without calling back).
// - Service will crash within the second call (without calling back).
// - Service is already dead by time of third request.
// This is done once for getInitialData and once for authAndSign.
//
// The client will need to reject all the callbacks itself.
static void
JNI_IpProtectionAuthTestNatives_TestUnresolvedCallbacksRejectedAfterCrash(
    JNIEnv* env) {
  base::test::SingleThreadTaskEnvironment task_environment;
  std::vector<base::expected<privacy::ppn::GetInitialDataResponse,
                             ip_protection::android::AuthRequestError>>
      get_initial_data_responses;
  std::vector<base::expected<privacy::ppn::AuthAndSignResponse,
                             ip_protection::android::AuthRequestError>>
      auth_and_sign_responses;
  std::vector<base::expected<ip_protection::GetProxyConfigResponse,
                             ip_protection::android::AuthRequestError>>
      get_proxy_config_responses;
  const int numCalls = 3;

  {
    std::unique_ptr<ip_protection::android::IpProtectionAuthClientInterface>
        client = CreateAndExpectClientBlocking(
            kMockClassNameForCrashAfterTwoRequestsSyncWithoutResponses);
    int remaining = numCalls;
    base::RunLoop run_loop;
    for (int i = 0; i < numCalls; i++) {
      client->GetInitialData(
          privacy::ppn::GetInitialDataRequest(),
          base::BindPostTask(
              base::SequencedTaskRunner::GetCurrentDefault(),
              base::BindLambdaForTesting(
                  [&run_loop, &remaining, &get_initial_data_responses](
                      base::expected<privacy::ppn::GetInitialDataResponse,
                                     ip_protection::android::AuthRequestError>
                          response) {
                    get_initial_data_responses.emplace_back(
                        std::move(response));
                    remaining--;
                    if (remaining == 0) {
                      run_loop.Quit();
                    }
                  })));
    }
    run_loop.Run();
  }
  {
    std::unique_ptr<ip_protection::android::IpProtectionAuthClientInterface>
        client = CreateAndExpectClientBlocking(
            kMockClassNameForCrashAfterTwoRequestsSyncWithoutResponses);
    int remaining = numCalls;
    base::RunLoop run_loop;
    for (int i = 0; i < numCalls; i++) {
      client->AuthAndSign(
          privacy::ppn::AuthAndSignRequest(),
          base::BindPostTask(
              base::SequencedTaskRunner::GetCurrentDefault(),
              base::BindLambdaForTesting(
                  [&run_loop, &remaining, &auth_and_sign_responses](
                      base::expected<privacy::ppn::AuthAndSignResponse,
                                     ip_protection::android::AuthRequestError>
                          response) {
                    auth_and_sign_responses.emplace_back(std::move(response));
                    remaining--;
                    if (remaining == 0) {
                      run_loop.Quit();
                    }
                  })));
    }
    run_loop.Run();
  }
  {
    std::unique_ptr<ip_protection::android::IpProtectionAuthClientInterface>
        client = CreateAndExpectClientBlocking(
            kMockClassNameForCrashAfterTwoRequestsSyncWithoutResponses);
    int remaining = numCalls;
    base::RunLoop run_loop;
    for (int i = 0; i < numCalls; i++) {
      client->GetProxyConfig(
          ip_protection::GetProxyConfigRequest(),
          base::BindPostTask(
              base::SequencedTaskRunner::GetCurrentDefault(),
              base::BindLambdaForTesting(
                  [&run_loop, &remaining, &get_proxy_config_responses](
                      base::expected<ip_protection::GetProxyConfigResponse,
                                     ip_protection::android::AuthRequestError>
                          response) {
                    get_proxy_config_responses.emplace_back(
                        std::move(response));
                    remaining--;
                    if (remaining == 0) {
                      run_loop.Quit();
                    }
                  })));
    }
    run_loop.Run();
  }

  CHECK(get_initial_data_responses.size() == 3);
  CHECK(auth_and_sign_responses.size() == 3);
  CHECK(get_proxy_config_responses.size() == 3);
  for (const auto& response : get_initial_data_responses) {
    CHECK(!response.has_value());
    CHECK(response.error() == ip_protection::android::AuthRequestError::kOther);
  }
  for (const auto& response : auth_and_sign_responses) {
    CHECK(!response.has_value());
    CHECK(response.error() == ip_protection::android::AuthRequestError::kOther);
  }
  for (const auto& response : get_proxy_config_responses) {
    CHECK(!response.has_value());
    CHECK(response.error() == ip_protection::android::AuthRequestError::kOther);
  }
}
