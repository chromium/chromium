// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_ASSISTANT_ASSISTANT_TEST_MIXIN_H_
#define CHROME_BROWSER_UI_ASH_ASSISTANT_ASSISTANT_TEST_MIXIN_H_

#include <memory>
#include <string>
#include <vector>

#include "base/test/test_timeouts.h"
#include "base/time/time.h"
#include "chrome/browser/ui/ash/assistant/test_support/fake_s3_server.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"

class PrefService;

namespace ash {
class AssistantTestApi;
}

namespace ash::assistant {

class LoggedInUserMixin;

// Creates everything required to test the Assistant in browser tests.
// This includes:
//     - Installing a fake Gaia server so the test user is able to use the
//       Assistant.
//     - Setting up a fake S3 server to spoof fake interactions.
//     - Enabling the Assistant service.
//     - Disabling all Assistant animations.
//
// See definition of `FakeS3Server` for an explanation of the different modes
// the fake S3 server can run in (specified by passing `FakeS3Mode` into the
// constructor).
class AssistantTestMixin : public InProcessBrowserTestMixin {
 public:
  AssistantTestMixin(InProcessBrowserTestMixinHost* host,
                     InProcessBrowserTest* test_base,
                     net::EmbeddedTestServer* embedded_test_server,
                     FakeS3Mode mode,
                     int test_data_version);

  AssistantTestMixin(const AssistantTestMixin&) = delete;
  AssistantTestMixin& operator=(const AssistantTestMixin&) = delete;

  ~AssistantTestMixin() override;

  // InProcessBrowserTestMixin overrides:
  void SetUpCommandLine(base::CommandLine* command_line) override;
  void SetUpOnMainThread() override;
  void TearDownOnMainThread() override;

  void DisableFakeS3Server();

  // Starts the Assistant service and wait until it is ready to process
  // queries. Should be called as the first action in every test.
  void StartAssistantAndWaitForReady(
      base::TimeDelta wait_timeout = TestTimeouts::action_timeout());

  // Changes the user setting controlling if the user has enabled Assistant.
  void SetAssistantEnabled(bool enabled);

  // Changes the user setting controlling if the user prefers voice or keyboard.
  void SetPreferVoice(bool prefer_voice);

  // Submits a text query. Can only be used when the Assistant UI is visible and
  // displaying the query input text field.
  void SendTextQuery(const std::string& query);

  // Synchronize an async method call to make testing simpler. |func| is the
  // async method to be invoked, the inner callback is the result callback. The
  // result with type |T| will be the return value.
  //
  // NOTE: This is a template method. If you need to use it with a new type,
  // you may see a link error. You will need to manually instantiate for the
  // new type.  Please see .cc file for examples.
  template <typename T>
  T SyncCall(base::OnceCallback<void(base::OnceCallback<void(T)>)> func);

  // Waits until a card response is rendered that contains the given text.
  // If |expected_response| is not received in |wait_timeout|, this will fail
  // the test.
  void ExpectCardResponse(
      const std::string& expected_response,
      base::TimeDelta wait_timeout = TestTimeouts::action_timeout());

  // Waits until an error response is rendered that contains the given text. If
  // |expected_response| is not received in |wait_timeout|, this will fail the
  // test.
  void ExpectErrorResponse(
      const std::string& expected_response,
      base::TimeDelta wait_timeout = TestTimeouts::action_timeout());

  // Waits until a text response is rendered that contains the given text.
  // If |expected_response| is not received in |wait_timeout|, this will fail
  // the test.
  void ExpectTextResponse(
      const std::string& expected_response,
      base::TimeDelta wait_timeout = TestTimeouts::action_timeout());

  // Same as above but checks if any of the given responses are encountered.
  void ExpectAnyOfTheseTextResponses(
      const std::vector<std::string>& expected_responses,
      base::TimeDelta wait_timeout = TestTimeouts::action_timeout());

  // Waits until a timers response is rendered that contains the given timers.
  // If the expected response is not received in |wait_timeout|, this will fail
  // the test.
  void ExpectTimersResponse(
      const std::vector<base::TimeDelta>& timers,
      base::TimeDelta wait_timeout = TestTimeouts::action_timeout());

  // Waits until a timers response is rendered and returns the time remaining of
  // the rendered timers. If a timers response is not received in |wait_timeout|
  // this will fail the test.
  std::vector<base::TimeDelta> ExpectAndReturnTimersResponse(
      base::TimeDelta wait_timeout = TestTimeouts::action_timeout());

  // Presses the Assistant key, which will toggle the Assistant UI.
  void PressAssistantKey();

  // Returns true if the Assistant UI is currently visible.
  bool IsVisible();

  // Watches the view hierarchy for change and fails if
  // a view is updated / deleted / added before wait_timeout time elapses.
  void ExpectNoChange(
      base::TimeDelta wait_timeout = TestTimeouts::action_timeout());

 private:
  PrefService* GetUserPreferences();
  void SendKeyPress(::ui::KeyboardCode key);
  void DisableAssistant();

  FakeS3Server fake_s3_server_;
  FakeS3Mode mode_;
  std::unique_ptr<AssistantTestApi> test_api_;
  std::unique_ptr<LoggedInUserMixin> user_mixin_;
};

}  // namespace ash::assistant

#endif  // CHROME_BROWSER_UI_ASH_ASSISTANT_ASSISTANT_TEST_MIXIN_H_
