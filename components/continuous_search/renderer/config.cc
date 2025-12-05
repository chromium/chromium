// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/continuous_search/renderer/config.h"

#include "base/no_destructor.h"

namespace continuous_search {

namespace {

Config& GetConfigInternal() {
  static base::NoDestructor<Config> s_config;
  return *s_config;
}

}  // namespace

Config::Config() = default;

Config::Config(const Config& other) = default;
Config::~Config() = default;

const Config& GetConfig() {
  return GetConfigInternal();
}

void SetConfigForTesting(const Config& config) {
  GetConfigInternal() = config;
}

}  // namespace continuous_search