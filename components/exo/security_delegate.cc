// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/security_delegate.h"

#include <memory>
#include <string>

namespace exo {

namespace {

class DefaultSecurityDelegate : public SecurityDelegate {
 public:
  ~DefaultSecurityDelegate() override = default;

  std::string GetSecurityContext() const override { return ""; }
};

}  // namespace

SecurityDelegate::~SecurityDelegate() = default;

// static
std::unique_ptr<SecurityDelegate>
SecurityDelegate::GetDefaultSecurityDelegate() {
  return std::make_unique<DefaultSecurityDelegate>();
}

}  // namespace exo
