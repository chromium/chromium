// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_TEST_FAKE_ENVIRONMENT_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_TEST_FAKE_ENVIRONMENT_H_

#include <map>
#include <string>
#include <string_view>

#include "base/environment.h"

namespace web_app {

// Provides mock environment variables values based on a stored map.
class FakeEnvironment : public base::Environment {
 public:
  FakeEnvironment();
  FakeEnvironment(const FakeEnvironment&) = delete;
  FakeEnvironment& operator=(const FakeEnvironment&) = delete;
  ~FakeEnvironment() override;

  void Set(std::string_view name, const std::string& value);

  bool GetVar(std::string_view variable_name, std::string* result) override;

  bool SetVar(std::string_view variable_name,
              const std::string& new_value) override;

  bool UnSetVar(std::string_view variable_name) override;

 private:
  std::map<std::string, std::string> variables_;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_TEST_FAKE_ENVIRONMENT_H_
