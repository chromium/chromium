// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/capabilities.h"

#include <memory>
#include <string>

namespace exo {

namespace {

class DefaultCapabilities : public Capabilities {
 public:
  ~DefaultCapabilities() override = default;

  std::string GetSecurityContext() const override { return ""; }
};

}  // namespace

Capabilities::~Capabilities() = default;

// static
std::unique_ptr<Capabilities> Capabilities::GetDefaultCapabilities() {
  return std::make_unique<DefaultCapabilities>();
}

}  // namespace exo
