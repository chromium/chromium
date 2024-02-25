// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_PPAPI_PPAPI_TEST_H_
#define CHROME_TEST_PPAPI_PPAPI_TEST_H_

#include <string>

#include "base/scoped_observation.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/infobars/core/infobar_manager.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/javascript_test_observer.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "third_party/blink/public/common/input/web_input_event.h"

namespace infobars {
class ContentInfoBarManager;
}

class PPAPITestMessageHandler : public content::TestMessageHandler {
 public:
  PPAPITestMessageHandler();

  PPAPITestMessageHandler(const PPAPITestMessageHandler&) = delete;
  PPAPITestMessageHandler& operator=(const PPAPITestMessageHandler&) = delete;

  MessageResponse HandleMessage(const std::string& json) override;
  void Reset() override;

  const std::string& message() const {
    return message_;
  }

 private:
  std::string message_;
};

class PPAPITestBase : public InProcessBrowserTest {
 public:
  PPAPITestBase();

  // InProcessBrowserTest:
  void SetUp() override;
  void SetUpCommandLine(base::CommandLine* command_line) override;
  void SetUpOnMainThread() override;

  virtual std::string BuildQuery(const std::string& base,
                                 const std::string& test_case) = 0;

  // Returns the URL to load for file: tests.
  GURL GetTestFileUrl(const std::string& test_case);
  virtual void RunTest(const std::string& test_case);
  virtual void RunTestViaHTTP(const std::string& test_case);
  virtual void RunTestWithSSLServer(const std::string& test_case);
  virtual void RunTestWithWebSocketServer(const std::string& test_case);
  virtual void RunTestIfAudioOutputAvailable(const std::string& test_case);
  virtual void RunTestViaHTTPIfAudioOutputAvailable(
      const std::string& test_case);

 protected:
  class InfoBarObserver : public infobars::InfoBarManager::Observer {
   public:
    explicit InfoBarObserver(PPAPITestBase* test_base);
    ~InfoBarObserver();

    void ExpectInfoBarAndAccept(bool should_accept);

   private:
    // infobars::InfoBarManager::Observer:
    void OnInfoBarAdded(infobars::InfoBar* infobar) override;
    void OnManagerShuttingDown(infobars::InfoBarManager* manager) override;

    infobars::ContentInfoBarManager* GetInfoBarManager();

    void VerifyInfoBarState();

    PPAPITestBase* test_base_;
    bool expecting_infobar_;
    bool should_accept_;

    base::ScopedObservation<infobars::InfoBarManager,
                            infobars::InfoBarManager::Observer>
        infobar_observation_{this};
  };

  // Runs the test for a tab given the tab that's already navigated to the
  // given URL.
  void RunTestURL(const GURL& test_url);
  // Gets the URL of the the given |test_case| for the given HTTP test server.
  // If |extra_params| is non-empty, it will be appended as URL parameters.
  GURL GetTestURL(const net::EmbeddedTestServer& http_server,
                  const std::string& test_case,
                  const std::string& extra_params);

  base::test::ScopedFeatureList scoped_feature_list_;
};

// In-process plugin test runner.  See OutOfProcessPPAPITest below for the
// out-of-process version.
class PPAPITest : public PPAPITestBase {
 public:
  PPAPITest();

  void SetUpCommandLine(base::CommandLine* command_line) override;

  std::string BuildQuery(const std::string& base,
                         const std::string& test_case) override;

 protected:
  bool in_process_;  // Controls the --ppapi-in-process switch.
};

class PPAPIPrivateTest : public PPAPITest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override;
};

// Variant of PPAPITest that runs plugins out-of-process to test proxy
// codepaths.
class OutOfProcessPPAPITest : public PPAPITest {
 public:
  OutOfProcessPPAPITest();

  void SetUpCommandLine(base::CommandLine* command_line) override;
  void RunTouchEventTest(const std::string& test_case);
};

class OutOfProcessPPAPIPrivateTest : public OutOfProcessPPAPITest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override;
};

// NaCl plugin test runner for Newlib runtime.
class PPAPINaClTest : public PPAPITestBase {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override;
  // PPAPITestBase overrides.
  void RunTest(const std::string& test_case) override;
  void RunTestViaHTTP(const std::string& test_case) override;
  void RunTestWithSSLServer(const std::string& test_case) override;
  void RunTestWithWebSocketServer(const std::string& test_case) override;
  void RunTestIfAudioOutputAvailable(const std::string& test_case) override;
  void RunTestViaHTTPIfAudioOutputAvailable(
      const std::string& test_case) override;
};

// NaCl plugin test runner for Newlib runtime.
class PPAPINaClNewlibTest : public PPAPINaClTest {
 public:
  std::string BuildQuery(const std::string& base,
                         const std::string& test_case) override;
};

class PPAPIPrivateNaClNewlibTest : public PPAPINaClNewlibTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override;
};

// NaCl plugin test runner for GNU-libc runtime.
class PPAPINaClGLibcTest : public PPAPINaClTest {
 public:
  std::string BuildQuery(const std::string& base,
                         const std::string& test_case) override;
};

class PPAPIPrivateNaClGLibcTest : public PPAPINaClGLibcTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override;
};

// NaCl plugin test runner for the PNaCl + Newlib runtime.
class PPAPINaClPNaClTest : public PPAPINaClTest {
 public:
  std::string BuildQuery(const std::string& base,
                         const std::string& test_case) override;
};

class PPAPIPrivateNaClPNaClTest : public PPAPINaClPNaClTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override;
};

class PPAPINaClTestDisallowedSockets : public PPAPITestBase {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override;

  std::string BuildQuery(const std::string& base,
                         const std::string& test_case) override;
};

class PPAPIBrokerInfoBarTest : public OutOfProcessPPAPITest {};

#endif  // CHROME_TEST_PPAPI_PPAPI_TEST_H_
