// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_COMMAND_LISTENER_H_
#define CHROME_TEST_CHROMEDRIVER_COMMAND_LISTENER_H_

#include <string>

class Status;

class CommandListener {
 public:
  virtual ~CommandListener() {}

  // Called just before a WebDriver command is run, but only
  // for commands that operate on an existing session. Will be called for
  // WindowCommands, ElementCommands, SessionCommands, and AlertCommands.
  virtual Status BeforeCommand(const std::string& command_name) = 0;
};

#endif  // CHROME_TEST_CHROMEDRIVER_COMMAND_LISTENER_H_
