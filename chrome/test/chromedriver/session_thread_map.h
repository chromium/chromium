// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_SESSION_THREAD_MAP_H_
#define CHROME_TEST_CHROMEDRIVER_SESSION_THREAD_MAP_H_

#include <map>
#include <memory>
#include <string>

#include "base/threading/thread.h"

// Info related to session threads, one instance per session. This object should
// only be accessed on the main thread.
class SessionThreadInfo {
 public:
  SessionThreadInfo(const std::string& name, bool w3c_mode)
      : thread_(name), w3c_mode_(w3c_mode) {}
  base::Thread* thread() { return &thread_; }
  bool w3cMode() const { return w3c_mode_; }

 private:
  base::Thread thread_;
  bool w3c_mode_;
};

using SessionThreadMap =
    std::map<std::string, std::unique_ptr<SessionThreadInfo>>;

#endif  // CHROME_TEST_CHROMEDRIVER_SESSION_THREAD_MAP_H_
