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
  base::RunLoop run_loop;

  ip_protection::android::IpProtectionAuthClient::
      CreateConnectedInstanceForTesting(
          kMockPackageName, kMockClassNameForDefault,
          base::BindPostTask(
              base::SequencedTaskRunner::GetCurrentDefault(),
              base::BindLambdaForTesting(
                  [&run_loop](
                      base::expected<
                          std::unique_ptr<ip_protection::android::
                                              IpProtectionAuthClientInterface>,
                          std::string> response) {
                    CHECK(response.has_value()) << response.error();
                    run_loop.Quit();
                  })));
  run_loop.Run();
}

static void JNI_IpProtectionAuthTestNatives_TestGetInitialData(JNIEnv* env) {
  base::test::SingleThreadTaskEnvironment task_environment;
  base::RunLoop run_loop;

  ip_protection::android::IpProtectionAuthClient::
      CreateConnectedInstanceForTesting(
          kMockPackageName, kMockClassNameForDefault,
          base::BindPostTask(
              base::SequencedTaskRunner::GetCurrentDefault(),
              base::BindLambdaForTesting(
                  [&run_loop](
                      base::expected<
                          std::unique_ptr<ip_protection::android::
                                              IpProtectionAuthClientInterface>,
                          std::string> response) {
                    CHECK(response.has_value()) << response.error();
                    privacy::ppn::GetInitialDataRequest request;
                    request.set_service_type("webviewipblinding");
                    (*response)->GetInitialData(
                        request,
                        base::BindLambdaForTesting(
                            [&run_loop](
                                base::expected<
                                    privacy::ppn::GetInitialDataResponse,
                                    ip_protection::android::AuthRequestError>
                                    initial_data_response) {
                              CHECK(initial_data_response.has_value());
                              CHECK(initial_data_response->privacy_pass_data()
                                        .token_key_id() == "test")
                                  << "expected \"test\" for token_key_id, got: "
                                  << initial_data_response->privacy_pass_data()
                                         .token_key_id();
                              run_loop.Quit();
                            }));
                  })));
  run_loop.Run();
}

static void JNI_IpProtectionAuthTestNatives_TestAuthAndSign(JNIEnv* env) {
  base::test::SingleThreadTaskEnvironment task_environment;
  base::RunLoop run_loop;

  ip_protection::android::IpProtectionAuthClient::
      CreateConnectedInstanceForTesting(
          kMockPackageName, kMockClassNameForDefault,
          base::BindPostTask(
              base::SequencedTaskRunner::GetCurrentDefault(),
              base::BindLambdaForTesting(
                  [&run_loop](
                      base::expected<
                          std::unique_ptr<ip_protection::android::
                                              IpProtectionAuthClientInterface>,
                          std::string> response) {
                    CHECK(response.has_value()) << response.error();
                    privacy::ppn::AuthAndSignRequest request;
                    request.set_oauth_token("test");
                    (*response)->AuthAndSign(
                        request,
                        base::BindLambdaForTesting(
                            [&run_loop](
                                base::expected<
                                    privacy::ppn::AuthAndSignResponse,
                                    ip_protection::android::AuthRequestError>
                                    auth_and_sign_response) {
                              CHECK(auth_and_sign_response.has_value());
                              CHECK(auth_and_sign_response->apn_type() ==
                                    "test")
                                  << "expected \"test\" for apn_type, got: "
                                  << auth_and_sign_response->apn_type();
                              run_loop.Quit();
                            }));
                  })));
  run_loop.Run();
}

static void JNI_IpProtectionAuthTestNatives_TestTransientError(JNIEnv* env) {
  base::test::SingleThreadTaskEnvironment task_environment;
  base::RunLoop run_loop;

  ip_protection::android::IpProtectionAuthClient::
      CreateConnectedInstanceForTesting(
          kMockPackageName, kMockClassNameForTransientError,
          base::BindPostTask(
              base::SequencedTaskRunner::GetCurrentDefault(),
              base::BindLambdaForTesting(
                  [&run_loop](
                      base::expected<
                          std::unique_ptr<ip_protection::android::
                                              IpProtectionAuthClientInterface>,
                          std::string> response) {
                    CHECK(response.has_value()) << response.error();

                    (*response)->GetInitialData(
                        privacy::ppn::GetInitialDataRequest(),
                        base::BindLambdaForTesting(
                            [&run_loop](
                                base::expected<
                                    privacy::ppn::GetInitialDataResponse,
                                    ip_protection::android::AuthRequestError>
                                    initial_data_response) {
                              CHECK(!initial_data_response.has_value());
                              CHECK(initial_data_response.error() ==
                                    ip_protection::android::AuthRequestError::
                                        kTransient);
                              run_loop.Quit();
                            }));

                    (*response)->AuthAndSign(
                        privacy::ppn::AuthAndSignRequest(),
                        base::BindLambdaForTesting(
                            [&run_loop](
                                base::expected<
                                    privacy::ppn::AuthAndSignResponse,
                                    ip_protection::android::AuthRequestError>
                                    auth_and_sign_response) {
                              CHECK(!auth_and_sign_response.has_value());
                              CHECK(auth_and_sign_response.error() ==
                                    ip_protection::android::AuthRequestError::
                                        kTransient);
                              run_loop.Quit();
                            }));
                  })));
  run_loop.Run();
}

static void JNI_IpProtectionAuthTestNatives_TestPersistentError(JNIEnv* env) {
  base::test::SingleThreadTaskEnvironment task_environment;
  base::RunLoop run_loop;

  ip_protection::android::IpProtectionAuthClient::
      CreateConnectedInstanceForTesting(
          kMockPackageName, kMockClassNameForPersistentError,
          base::BindPostTask(
              base::SequencedTaskRunner::GetCurrentDefault(),
              base::BindLambdaForTesting(
                  [&run_loop](
                      base::expected<
                          std::unique_ptr<ip_protection::android::
                                              IpProtectionAuthClientInterface>,
                          std::string> response) {
                    CHECK(response.has_value()) << response.error();

                    (*response)->GetInitialData(
                        privacy::ppn::GetInitialDataRequest(),
                        base::BindLambdaForTesting(
                            [&run_loop](
                                base::expected<
                                    privacy::ppn::GetInitialDataResponse,
                                    ip_protection::android::AuthRequestError>
                                    initial_data_response) {
                              CHECK(!initial_data_response.has_value());
                              CHECK(initial_data_response.error() ==
                                    ip_protection::android::AuthRequestError::
                                        kPersistent);
                              run_loop.Quit();
                            }));

                    (*response)->AuthAndSign(
                        privacy::ppn::AuthAndSignRequest(),
                        base::BindLambdaForTesting(
                            [&run_loop](
                                base::expected<
                                    privacy::ppn::AuthAndSignResponse,
                                    ip_protection::android::AuthRequestError>
                                    auth_and_sign_response) {
                              CHECK(!auth_and_sign_response.has_value());
                              CHECK(auth_and_sign_response.error() ==
                                    ip_protection::android::AuthRequestError::
                                        kPersistent);
                              run_loop.Quit();
                            }));
                  })));
  run_loop.Run();
}

static void JNI_IpProtectionAuthTestNatives_TestIllegalErrorCode(JNIEnv* env) {
  base::test::SingleThreadTaskEnvironment task_environment;
  base::RunLoop run_loop;

  ip_protection::android::IpProtectionAuthClient::
      CreateConnectedInstanceForTesting(
          kMockPackageName, kMockClassNameForIllegalErrorCode,
          base::BindPostTask(
              base::SequencedTaskRunner::GetCurrentDefault(),
              base::BindLambdaForTesting(
                  [&run_loop](
                      base::expected<
                          std::unique_ptr<ip_protection::android::
                                              IpProtectionAuthClientInterface>,
                          std::string> client) {
                    CHECK(client.has_value()) << client.error();

                    (*client)->GetInitialData(
                        privacy::ppn::GetInitialDataRequest(),
                        base::BindLambdaForTesting(
                            [&run_loop](
                                base::expected<
                                    privacy::ppn::GetInitialDataResponse,
                                    ip_protection::android::AuthRequestError>
                                    initial_data_response) {
                              CHECK(!initial_data_response.has_value());
                              CHECK(initial_data_response.error() ==
                                    ip_protection::android::AuthRequestError::
                                        kOther);
                              run_loop.Quit();
                            }));

                    (*client)->AuthAndSign(
                        privacy::ppn::AuthAndSignRequest(),
                        base::BindLambdaForTesting(
                            [&run_loop](
                                base::expected<
                                    privacy::ppn::AuthAndSignResponse,
                                    ip_protection::android::AuthRequestError>
                                    auth_and_sign_response) {
                              CHECK(!auth_and_sign_response.has_value());
                              CHECK(auth_and_sign_response.error() ==
                                    ip_protection::android::AuthRequestError::
                                        kOther);
                              run_loop.Quit();
                            }));
                  })));
  run_loop.Run();
}

static void JNI_IpProtectionAuthTestNatives_TestNullResponse(JNIEnv* env) {
  base::test::SingleThreadTaskEnvironment task_environment;
  base::RunLoop run_loop;

  ip_protection::android::IpProtectionAuthClient::
      CreateConnectedInstanceForTesting(
          kMockPackageName, kMockClassNameForNullResponse,
          base::BindPostTask(
              base::SequencedTaskRunner::GetCurrentDefault(),
              base::BindLambdaForTesting(
                  [&run_loop](
                      base::expected<
                          std::unique_ptr<ip_protection::android::
                                              IpProtectionAuthClientInterface>,
                          std::string> client) {
                    CHECK(client.has_value()) << client.error();

                    (*client)->GetInitialData(
                        privacy::ppn::GetInitialDataRequest(),
                        base::BindLambdaForTesting(
                            [&run_loop](
                                base::expected<
                                    privacy::ppn::GetInitialDataResponse,
                                    ip_protection::android::AuthRequestError>
                                    initial_data_response) {
                              CHECK(!initial_data_response.has_value());
                              CHECK(initial_data_response.error() ==
                                    ip_protection::android::AuthRequestError::
                                        kOther);
                              run_loop.Quit();
                            }));

                    (*client)->AuthAndSign(
                        privacy::ppn::AuthAndSignRequest(),
                        base::BindLambdaForTesting(
                            [&run_loop](
                                base::expected<
                                    privacy::ppn::AuthAndSignResponse,
                                    ip_protection::android::AuthRequestError>
                                    auth_and_sign_response) {
                              CHECK(!auth_and_sign_response.has_value());
                              CHECK(auth_and_sign_response.error() ==
                                    ip_protection::android::AuthRequestError::
                                        kOther);
                              run_loop.Quit();
                            }));
                  })));
  run_loop.Run();
}

static void JNI_IpProtectionAuthTestNatives_TestUnparsableResponse(
    JNIEnv* env) {
  base::test::SingleThreadTaskEnvironment task_environment;
  base::RunLoop run_loop;

  ip_protection::android::IpProtectionAuthClient::
      CreateConnectedInstanceForTesting(
          kMockPackageName, kMockClassNameForUnparsableResponse,
          base::BindPostTask(
              base::SequencedTaskRunner::GetCurrentDefault(),
              base::BindLambdaForTesting(
                  [&run_loop](
                      base::expected<
                          std::unique_ptr<ip_protection::android::
                                              IpProtectionAuthClientInterface>,
                          std::string> client) {
                    CHECK(client.has_value()) << client.error();

                    (*client)->GetInitialData(
                        privacy::ppn::GetInitialDataRequest(),
                        base::BindLambdaForTesting(
                            [&run_loop](
                                base::expected<
                                    privacy::ppn::GetInitialDataResponse,
                                    ip_protection::android::AuthRequestError>
                                    initial_data_response) {
                              CHECK(!initial_data_response.has_value());
                              CHECK(initial_data_response.error() ==
                                    ip_protection::android::AuthRequestError::
                                        kOther);
                              run_loop.Quit();
                            }));

                    (*client)->AuthAndSign(
                        privacy::ppn::AuthAndSignRequest(),
                        base::BindLambdaForTesting(
                            [&run_loop](
                                base::expected<
                                    privacy::ppn::AuthAndSignResponse,
                                    ip_protection::android::AuthRequestError>
                                    auth_and_sign_response) {
                              CHECK(!auth_and_sign_response.has_value());
                              CHECK(auth_and_sign_response.error() ==
                                    ip_protection::android::AuthRequestError::
                                        kOther);
                              run_loop.Quit();
                            }));
                  })));
  run_loop.Run();
}

static void JNI_IpProtectionAuthTestNatives_TestSynchronousError(JNIEnv* env) {
  base::test::SingleThreadTaskEnvironment task_environment;
  base::RunLoop run_loop;

  ip_protection::android::IpProtectionAuthClient::
      CreateConnectedInstanceForTesting(
          kMockPackageName, kMockClassNameForSynchronousError,
          base::BindPostTask(
              base::SequencedTaskRunner::GetCurrentDefault(),
              base::BindLambdaForTesting(
                  [&run_loop](
                      base::expected<
                          std::unique_ptr<ip_protection::android::
                                              IpProtectionAuthClientInterface>,
                          std::string> client) {
                    CHECK(client.has_value()) << client.error();

                    (*client)->GetInitialData(
                        privacy::ppn::GetInitialDataRequest(),
                        base::BindLambdaForTesting(
                            [&run_loop](
                                base::expected<
                                    privacy::ppn::GetInitialDataResponse,
                                    ip_protection::android::AuthRequestError>
                                    initial_data_response) {
                              CHECK(!initial_data_response.has_value());
                              CHECK(initial_data_response.error() ==
                                    ip_protection::android::AuthRequestError::
                                        kOther);
                              run_loop.Quit();
                            }));

                    (*client)->AuthAndSign(
                        privacy::ppn::AuthAndSignRequest(),
                        base::BindLambdaForTesting(
                            [&run_loop](
                                base::expected<
                                    privacy::ppn::AuthAndSignResponse,
                                    ip_protection::android::AuthRequestError>
                                    auth_and_sign_response) {
                              CHECK(!auth_and_sign_response.has_value());
                              CHECK(auth_and_sign_response.error() ==
                                    ip_protection::android::AuthRequestError::
                                        kOther);
                              run_loop.Quit();
                            }));
                  })));
  run_loop.Run();
}
