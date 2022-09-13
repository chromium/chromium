// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_ASSISTANT_TEST_SUPPORT_FAKE_S3_SERVER_H_
#define CHROME_BROWSER_UI_ASH_ASSISTANT_TEST_SUPPORT_FAKE_S3_SERVER_H_

#include <memory>
#include <string>

#include "base/process/process.h"

namespace ash::assistant {

class PortSelector;

enum class FakeS3Mode {
  // In this mode all S3 requests are forwarded to the S3 server.
  kProxy,
  // In this mode all S3 requests are forwarded to the S3 server, and the
  // responses are recorded.
  kRecord,
  // In this mode all S3 requests are handled by replaying the responses stored
  // while running in |kRecord| mode.
  kReplay,
};

// Class that starts/stops a fake S3 server.
// Note that this will also ensure the Assistant service knows to use the fake
// s3 server.
//
// A valid access token is required if mode is |kProxy| or |kReplay|. See
// |kGenerateTokenInstructions| for information on how to get one.
class FakeS3Server {
 public:
  // |data_file_version| is used to look for a particular set of test data
  // files to use. This enables updating tests and checking in test data and
  // test themselves separately.  If the latest version of the test data file
  // does not exist, it will automatically looker for an older version of the
  // file.
  explicit FakeS3Server(int data_file_version);

  FakeS3Server(const FakeS3Server&) = delete;
  FakeS3Server& operator=(const FakeS3Server&) = delete;

  ~FakeS3Server();

  // Starts the fake S3 server, and tells the Assistant service to use its URI
  // for all S3 requests.
  void Setup(FakeS3Mode mode);
  void Teardown();

  // Returns the access token used by the S3 Server. This is only populated
  // after |Setup| is called.
  std::string GetAccessToken() const;

 private:
  void SetAccessTokenForMode(FakeS3Mode mode);
  void SetFakeS3ServerURI();
  void UnsetFakeS3ServerURI();
  void SetDeviceId();
  void UnsetDeviceId();
  void StartS3ServerProcess(FakeS3Mode mode);
  void StopS3ServerProcess();
  std::string GetTestDataFileName();

  int port() const;

  std::string access_token_{"FAKE_ACCESS_TOKEN"};
  std::string fake_s3_server_uri_;
  int data_file_version_;
  bool process_running_ = false;

  std::unique_ptr<PortSelector> port_selector_;

  base::Process fake_s3_server_;
};

}  // namespace ash::assistant

#endif  // CHROME_BROWSER_UI_ASH_ASSISTANT_TEST_SUPPORT_FAKE_S3_SERVER_H_
