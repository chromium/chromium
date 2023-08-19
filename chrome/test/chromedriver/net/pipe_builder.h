// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_NET_PIPE_BUILDER_H_
#define CHROME_TEST_CHROMEDRIVER_NET_PIPE_BUILDER_H_

#include <memory>
#include "base/command_line.h"
#include "base/files/file.h"
#include "base/files/scoped_file.h"
#include "base/process/launch.h"
#include "chrome/test/chromedriver/chrome/status.h"
#include "chrome/test/chromedriver/net/pipe_connection.h"
#include "chrome/test/chromedriver/net/sync_websocket.h"

// Builder of a PipeConnection object
class PipeBuilder {
 public:
  static const char kAsciizProtocolMode[];
  static const char kCborProtocolMode[];

  PipeBuilder();
  virtual ~PipeBuilder();

  // Return true if PipeConnection is implemented for the current platform.
  static bool PlatformIsSupported();

  PipeBuilder(const PipeBuilder&) = delete;
  PipeBuilder& operator=(const PipeBuilder&) = delete;

  // Set protocol mode.
  // Supported values:
  // * PipeBuilder::kAsciizProtocolMode,
  // * PipeBuilder::kCborProtocolMode,
  // * empty string resolving to PipeBuilder::kAsciizProtocolMode.
  void SetProtocolMode(std::string mode);

  // Return the underlying pipe connection object.
  // The ownership is transferred to the caller.
  // Prerequisties:
  // * Protocol mode must be set with SetProtocolMode.
  // * PipeConnection must be constructed with BuildSocket.
  std::unique_ptr<SyncWebSocket> TakeSocket();

  // Build the underlying pipe connection object.
  Status BuildSocket();

  // Close the endpoints intended for the child process.
  // This method is intended to be called after starting the child process.
  // If called earlier the child process will not be able to use its endpoints.
  Status CloseChildEndpoints();

  // Save the remote endpoints to the launch options and command line.
  // This information needs to be passed to base::LaunchProcess function.
  Status SetUpPipes(base::LaunchOptions* options, base::CommandLine* command);

 private:
  std::string protocol_mode_;
  base::ScopedPlatformFile read_file_;
  base::ScopedPlatformFile write_file_;
  base::ScopedPlatformFile child_ends_[2];
  std::unique_ptr<PipeConnection> connection_;
};

#endif  // CHROME_TEST_CHROMEDRIVER_NET_PIPE_BUILDER_H_
