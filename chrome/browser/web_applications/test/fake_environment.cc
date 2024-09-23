// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/test/fake_environment.h"

#include "base/containers/contains.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace web_app {

FakeEnvironment::FakeEnvironment() = default;
FakeEnvironment::~FakeEnvironment() = default;

void FakeEnvironment::Set(std::string_view name, const std::string& value) {
  const std::string key(name);
  variables_[key] = value;
}

bool FakeEnvironment::GetVar(std::string_view variable_name,
                             std::string* result) {
  const std::string key(variable_name);
  if (base::Contains(variables_, key)) {
    *result = variables_[key];
    return true;
  }

  return false;
}

bool FakeEnvironment::SetVar(std::string_view variable_name,
                             const std::string& new_value) {
  ADD_FAILURE();
  return false;
}

bool FakeEnvironment::UnSetVar(std::string_view variable_name) {
  ADD_FAILURE();
  return false;
}

}  // namespace web_app
