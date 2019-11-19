// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/json_writer.h"
#include "base/process/launch.h"
#include "base/values.h"
#include "base/win/windows_version.h"
#include "chrome/browser/ui/startup/credential_provider_signin_dialog_win_test_data.h"
#include "chrome/credential_provider/common/gcp_strings.h"
#include "chrome/credential_provider/gaiacp/gcp_utils.h"
#include "google_apis/gaia/gaia_switches.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/test/spawned_test_server/spawned_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace credential_provider {

class GcpUsingChromeTest : public ::testing::Test {
 protected:
  struct TestGoogleApiResponse {
    TestGoogleApiResponse()
        : info_code_(net::HTTP_BAD_REQUEST), response_given_(false) {}
    TestGoogleApiResponse(net::HttpStatusCode info_code,
                          const std::string& data)
        : info_code_(info_code), data_(data), response_given_(false) {}

    net::HttpStatusCode info_code_;
    std::string data_;
    bool response_given_;
  };

  GcpUsingChromeTest();

  void SetUp() override;
  void TearDown() override;

  void SetPasswordForSignin(const std::string& password) {
    test_data_storage_.SetSigninPassword(password);
  }
  void SetUserInfoResponse(TestGoogleApiResponse response) {
    user_info_response_ = response;
  }
  void SetTokenInfoResponse(TestGoogleApiResponse response) {
    token_info_response_ = response;
  }
  void SetMdmTokenResponse(TestGoogleApiResponse response) {
    mdm_token_response_ = response;
  }
  void SetSigninTokenResponse(TestGoogleApiResponse response) {
    signin_token_response_ = response;
  }

  std::string MakeInlineSigninCompletionScript(
      const std::string& email,
      const std::string& password,
      const std::string& gaia_id) const;

  std::string RunChromeAndExtractOutput() const;
  base::CommandLine GetCommandLineForChromeGls(
      const base::FilePath& user_data_dir) const;
  std::string RunProcessAndExtractOutput(
      const base::CommandLine& command_line) const;

  bool ShouldRunTestOnThisOS() const {
    // TODO(crbug.com/909722) Enable tests again once they are all passing. Currently, all tests are
    // flaky on all bots except win-asan.
    return false;
    // TODO(crbug.com/906793). For some reason handle inheritance does not work
    // correctly on Windows7 and causes all the tests to stall indefinetely.
    // Since GCPW is only targeted for Windows 10 currently, disable these
    // unit tests for now until the problem can be resolved.
    // return base::win::GetVersion() >= base::win::Version::WIN10;
  }
  std::unique_ptr<net::test_server::HttpResponse> GaiaHtmlResponseHandler(
      const net::test_server::HttpRequest& request);
  std::unique_ptr<net::test_server::HttpResponse> GoogleApisHtmlResponseHandler(
      const net::test_server::HttpRequest& request);

  CredentialProviderSigninDialogTestDataStorage test_data_storage_;
  net::test_server::EmbeddedTestServer gaia_server_;
  net::test_server::EmbeddedTestServer google_apis_server_;
  net::SpawnedTestServer proxy_server_;

  TestGoogleApiResponse signin_token_response_;
  TestGoogleApiResponse user_info_response_;
  TestGoogleApiResponse token_info_response_;
  TestGoogleApiResponse mdm_token_response_;

 private:
  DISALLOW_COPY_AND_ASSIGN(GcpUsingChromeTest);
};

GcpUsingChromeTest::GcpUsingChromeTest()
    : proxy_server_(net::SpawnedTestServer::TYPE_PROXY, base::FilePath()) {}

void GcpUsingChromeTest::SetUp() {
  if (!ShouldRunTestOnThisOS())
    return;

  // Redirect connections to signin related pages to a handler that will
  // generate the needed headers and content to move the signin flow
  // forward automatically.
  gaia_server_.RegisterRequestHandler(base::BindRepeating(
      &GcpUsingChromeTest::GaiaHtmlResponseHandler, base::Unretained(this)));
  EXPECT_TRUE(gaia_server_.Start());

  google_apis_server_.RegisterRequestHandler(
      base::BindRepeating(&GcpUsingChromeTest::GoogleApisHtmlResponseHandler,
                          base::Unretained(this)));
  EXPECT_TRUE(google_apis_server_.Start());

  // Run a proxy server to redirect all non signin related requests to a
  // page showing failed connections.
  proxy_server_.set_redirect_connect_to_localhost(true);
  EXPECT_TRUE(proxy_server_.Start());
}

void GcpUsingChromeTest::TearDown() {
  if (!ShouldRunTestOnThisOS())
    return;

  EXPECT_TRUE(gaia_server_.ShutdownAndWaitUntilComplete());
  EXPECT_TRUE(google_apis_server_.ShutdownAndWaitUntilComplete());
  EXPECT_TRUE(proxy_server_.Stop());
}

std::string GcpUsingChromeTest::RunChromeAndExtractOutput() const {
  base::ScopedTempDir user_data_dir;
  EXPECT_TRUE(user_data_dir.CreateUniqueTempDir());
  return RunProcessAndExtractOutput(
      GetCommandLineForChromeGls(user_data_dir.GetPath()));
}

base::CommandLine GcpUsingChromeTest::GetCommandLineForChromeGls(
    const base::FilePath& user_data_dir) const {
  auto* process_command_line = base::CommandLine::ForCurrentProcess();
  base::CommandLine command_line(
      process_command_line->GetProgram().DirName().Append(L"chrome.exe"));

  // Redirect gaia and google api pages to the servers that are being run
  // locally.
  const GURL& gaia_url = gaia_server_.base_url();
  command_line.AppendSwitchASCII(::switches::kGaiaUrl, gaia_url.spec());
  command_line.AppendSwitchASCII(::switches::kLsoUrl, gaia_url.spec());
  const GURL& google_apis_url = google_apis_server_.base_url();
  command_line.AppendSwitchASCII(::switches::kGoogleApisUrl,
                                 google_apis_url.spec());

  command_line.AppendSwitch(kGcpwSigninSwitch);
  command_line.AppendSwitchPath("user-data-dir", user_data_dir);
  command_line.AppendSwitchASCII("proxy-server",
                                 proxy_server_.host_port_pair().ToString());
  return command_line;
}

std::string GcpUsingChromeTest::RunProcessAndExtractOutput(
    const base::CommandLine& command_line) const {
  base::win::ScopedHandle read_handle;
  base::Process process;
  // The write handle is only needed until the process starts. If it is not
  // closed afterwards, it will not be possible to detect the end of the
  // output from the process since there will be more than one handle held
  // on the output pipe and it will not close when the process dies.
  {
    base::win::ScopedHandle write_handle;
    EXPECT_EQ(
        CreatePipeForChildProcess(false, false, &read_handle, &write_handle),
        S_OK);

    base::LaunchOptions options;
    options.stdin_handle = INVALID_HANDLE_VALUE;
    options.stdout_handle = write_handle.Get();
    options.stderr_handle = INVALID_HANDLE_VALUE;
    options.handles_to_inherit.push_back(write_handle.Get());

    process = base::Process(
        base::LaunchProcess(command_line.GetCommandLineString(), options));
    EXPECT_TRUE(process.IsValid());
  }

  constexpr DWORD kTimeout = 1000;
  std::string output_from_process;
  char buffer[1024];
  for (bool is_done = false; !is_done;) {
    DWORD length = base::size(buffer) - 1;

    DWORD ret = ::WaitForSingleObject(read_handle.Get(), kTimeout);
    if (ret == WAIT_OBJECT_0) {
      if (!::ReadFile(read_handle.Get(), buffer, length, &length, nullptr)) {
        break;
      }

      buffer[length] = 0;
      output_from_process += buffer;
    } else if (ret != WAIT_IO_COMPLETION) {
      break;
    }
  }

  // If the pipe is no longer readable it is expected that the process will be
  // terminating shortly.
  int exit_code;
  EXPECT_TRUE(process.WaitForExitWithTimeout(
      base::TimeDelta::FromMilliseconds(kTimeout), &exit_code));
  EXPECT_EQ(exit_code, 0);

  return output_from_process;
}

std::string GcpUsingChromeTest::MakeInlineSigninCompletionScript(
    const std::string& email,
    const std::string& password,
    const std::string& gaia_id) const {
  // Script that sends the two messages needed by inline_signin in order to
  // continue with the signin flow.
  return "<script>"
         "let webview = null;"
         "let onMessageEventHandler = function(event) {"
         "  if (!webview) {"
         "    webview = event.source;"
         "    var attempt_login_msg = {"
         "      'method' : 'attemptLogin',"
         "      'email' : '" +
         email +
         "',"
         "      'password' : '" +
         password +
         "',"
         "      'attemptToken' : 'attemptToken'"
         "    };"
         "    webview.postMessage(attempt_login_msg, '*');"
         "    var user_info_msg = {"
         "    'method' : 'userInfo',"
         "    'email' : '" +
         email +
         "',"
         "    'gaiaId' : '" +
         gaia_id +
         "',"
         "    'services' : []"
         "    };"
         "    webview.postMessage(user_info_msg, '*');"
         "  }"
         "};"
         "window.addEventListener('message', onMessageEventHandler);"
         "</script>";
}

std::unique_ptr<net::test_server::HttpResponse>
GcpUsingChromeTest::GaiaHtmlResponseHandler(
    const net::test_server::HttpRequest& request) {
  auto http_response = std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->set_code(net::HTTP_OK);
  http_response->set_content_type("text/html");
  std::string content = "<html><head>";
  // When the "/embedded/setup/chrome" is requested on the gaia web site
  // (accounts.google.com) then the embedded server can send a page with scripts
  // that can force immediate signin.
  if (request.GetURL().path().find("/embedded/setup/chrome") == 0) {
    // This is the  header that is sent by Gaia that the inline sign in page
    // listens to in order to fill the information abou the email and Gaia ID.
    http_response->AddCustomHeader("google-accounts-signin",
                                   "email=\"" +
                                       test_data_storage_.GetSuccessEmail() +
                                       "\","
                                       "obfuscatedid=\"" +
                                       test_data_storage_.GetSuccessId() +
                                       "\", "
                                       "sessionindex=0");
    // On a successful signin, the oauth_code cookie must also be set for the
    // site.
    http_response->AddCustomHeader("Set-Cookie",
                                   "oauth_code=oauth_code; Path=/");
    // This header is needed to ensure that the inline_signin page does not
    // break out of the constrained dialog.
    http_response->AddCustomHeader("google-accounts-embedded", std::string());

    content += MakeInlineSigninCompletionScript(
        test_data_storage_.GetSuccessEmail(),
        test_data_storage_.GetSuccessPassword(),
        test_data_storage_.GetSuccessId());
  }
  content += "</head></html>";
  http_response->set_content(content);
  return std::move(http_response);
}

std::unique_ptr<net::test_server::HttpResponse>
GcpUsingChromeTest::GoogleApisHtmlResponseHandler(
    const net::test_server::HttpRequest& request) {
  auto http_response = std::make_unique<net::test_server::BasicHttpResponse>();

  // The following Google API requests are expected in this order:
  // 1. A /oauth2/v4/token request that requests the initial access_token. This
  // request is made after the user usually has entered a valid user id and
  // password.
  // Either of the two following requests (in any order):
  // 1. A /oauth2/v2/tokeninfo to get the token handle for the user.
  // 2. A /oauth2/v4/token to get the required id token for MDM
  // registration as well as to request access to fetch the user info.
  // Finally if the second "/oauth2/v4/token" request is made to get the MDM
  // ID token then is expected that a request for "/oauth2/v1/userinfo" will
  // be made to get the full name of the user.

  // All other Google API requests will be ignored with a 404 error.

  TestGoogleApiResponse* api_response = nullptr;
  if (request.GetURL().path().find("/oauth2/v2/tokeninfo") == 0) {
    api_response = &token_info_response_;
  } else if (request.GetURL().path().find("/oauth2/v1/userinfo") == 0) {
    // User info should never be requested before the mdm id token request is
    // made.
    EXPECT_TRUE(mdm_token_response_.response_given_);
    api_response = &user_info_response_;
  } else if (request.GetURL().path().find("/oauth2/v4/token") == 0) {
    // Does the request want an auth_code for signin or is it the second request
    // made to get the id token.
    if (request.content.find("grant_type=authorization_code") ==
        std::string::npos) {
      api_response = &mdm_token_response_;
    } else {
      api_response = &signin_token_response_;
    }
  }

  if (api_response) {
    EXPECT_FALSE(api_response->response_given_);
    api_response->response_given_ = true;
    http_response->set_content_type("text/html");
    http_response->set_content(api_response->data_);
    http_response->set_code(api_response->info_code_);
  } else {
    http_response->set_code(net::HTTP_NOT_FOUND);
  }

  return std::move(http_response);
}

TEST_F(GcpUsingChromeTest, VerifyMissingSigninInfoOutput) {
  if (!ShouldRunTestOnThisOS())
    return;

  SetPasswordForSignin(std::string());
  SetTokenInfoResponse(
      {net::HTTP_OK, test_data_storage_.GetSuccessfulTokenInfoFetchResult()});
  SetUserInfoResponse(
      {net::HTTP_OK, test_data_storage_.GetSuccessfulUserInfoFetchResult()});
  SetMdmTokenResponse(
      {net::HTTP_OK, test_data_storage_.GetSuccessfulMdmTokenFetchResult()});
  SetSigninTokenResponse(
      {net::HTTP_OK, test_data_storage_.GetSuccessfulSigninTokenFetchResult()});

  std::string output_from_chrome = RunChromeAndExtractOutput();

  EXPECT_EQ(output_from_chrome, std::string());
  EXPECT_TRUE(signin_token_response_.response_given_);
  EXPECT_FALSE(user_info_response_.response_given_);
  EXPECT_FALSE(token_info_response_.response_given_);
  EXPECT_FALSE(mdm_token_response_.response_given_);
}

TEST_F(GcpUsingChromeTest, VerifySigninFailureOutput) {
  if (!ShouldRunTestOnThisOS())
    return;

  SetTokenInfoResponse(
      {net::HTTP_OK, test_data_storage_.GetSuccessfulTokenInfoFetchResult()});
  SetUserInfoResponse(
      {net::HTTP_OK, test_data_storage_.GetSuccessfulUserInfoFetchResult()});
  SetMdmTokenResponse(
      {net::HTTP_OK, test_data_storage_.GetSuccessfulMdmTokenFetchResult()});
  SetSigninTokenResponse({net::HTTP_OK,
                          CredentialProviderSigninDialogTestDataStorage::
                              kInvalidTokenFetchResponse});

  std::string output_from_chrome = RunChromeAndExtractOutput();

  EXPECT_EQ(output_from_chrome, std::string());
  EXPECT_TRUE(signin_token_response_.response_given_);
  EXPECT_FALSE(user_info_response_.response_given_);
  EXPECT_FALSE(token_info_response_.response_given_);
  EXPECT_FALSE(mdm_token_response_.response_given_);
}

TEST_F(GcpUsingChromeTest, VerifyTokenInfoFailureOutput) {
  if (!ShouldRunTestOnThisOS())
    return;

  SetTokenInfoResponse({net::HTTP_OK,
                        CredentialProviderSigninDialogTestDataStorage::
                            kInvalidTokenInfoResponse});
  SetUserInfoResponse(
      {net::HTTP_OK, test_data_storage_.GetSuccessfulUserInfoFetchResult()});
  SetMdmTokenResponse(
      {net::HTTP_OK, test_data_storage_.GetSuccessfulMdmTokenFetchResult()});
  SetSigninTokenResponse(
      {net::HTTP_OK, test_data_storage_.GetSuccessfulSigninTokenFetchResult()});

  std::string output_from_chrome = RunChromeAndExtractOutput();

  EXPECT_EQ(output_from_chrome, std::string());
  EXPECT_TRUE(signin_token_response_.response_given_);
  EXPECT_TRUE(token_info_response_.response_given_);
}

TEST_F(GcpUsingChromeTest, VerifyUserInfoFailureOutput) {
  if (!ShouldRunTestOnThisOS())
    return;

  SetTokenInfoResponse(
      {net::HTTP_OK, test_data_storage_.GetSuccessfulTokenInfoFetchResult()});
  SetUserInfoResponse({net::HTTP_OK,
                       CredentialProviderSigninDialogTestDataStorage::
                           kInvalidUserInfoResponse});
  SetMdmTokenResponse(
      {net::HTTP_OK, test_data_storage_.GetSuccessfulMdmTokenFetchResult()});
  SetSigninTokenResponse(
      {net::HTTP_OK, test_data_storage_.GetSuccessfulSigninTokenFetchResult()});

  std::string output_from_chrome = RunChromeAndExtractOutput();

  EXPECT_EQ(output_from_chrome, std::string());
  EXPECT_TRUE(signin_token_response_.response_given_);
  EXPECT_TRUE(user_info_response_.response_given_);
  EXPECT_TRUE(mdm_token_response_.response_given_);
}

TEST_F(GcpUsingChromeTest, VerifyMdmFailureOutput) {
  if (!ShouldRunTestOnThisOS())
    return;

  SetTokenInfoResponse(
      {net::HTTP_OK, test_data_storage_.GetSuccessfulTokenInfoFetchResult()});
  SetUserInfoResponse(
      {net::HTTP_OK, test_data_storage_.GetSuccessfulUserInfoFetchResult()});
  SetMdmTokenResponse({net::HTTP_OK,
                       CredentialProviderSigninDialogTestDataStorage::
                           kInvalidTokenFetchResponse});
  SetSigninTokenResponse(
      {net::HTTP_OK, test_data_storage_.GetSuccessfulSigninTokenFetchResult()});

  std::string output_from_chrome = RunChromeAndExtractOutput();

  EXPECT_EQ(output_from_chrome, std::string());
  EXPECT_TRUE(mdm_token_response_.response_given_);
  EXPECT_FALSE(user_info_response_.response_given_);
}

TEST_F(GcpUsingChromeTest, VerifySuccessOutput) {
  if (!ShouldRunTestOnThisOS())
    return;

  SetTokenInfoResponse(
      {net::HTTP_OK, test_data_storage_.GetSuccessfulTokenInfoFetchResult()});
  SetUserInfoResponse(
      {net::HTTP_OK, test_data_storage_.GetSuccessfulUserInfoFetchResult()});
  SetMdmTokenResponse(
      {net::HTTP_OK, test_data_storage_.GetSuccessfulMdmTokenFetchResult()});
  SetSigninTokenResponse(
      {net::HTTP_OK, test_data_storage_.GetSuccessfulSigninTokenFetchResult()});

  std::string output_from_chrome = RunChromeAndExtractOutput();

  std::string expected_result;
  base::JSONWriter::Write(test_data_storage_.expected_full_result(),
                          &expected_result);

  EXPECT_EQ(output_from_chrome, expected_result);
  EXPECT_TRUE(signin_token_response_.response_given_);
  EXPECT_TRUE(user_info_response_.response_given_);
  EXPECT_TRUE(token_info_response_.response_given_);
  EXPECT_TRUE(mdm_token_response_.response_given_);
}

}  // namespace credential_provider
