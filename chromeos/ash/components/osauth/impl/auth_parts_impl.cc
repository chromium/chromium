// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/check.h"
#include "base/check_op.h"
#include "chromeos/ash/components/osauth/impl/auth_parts_impl.h"
#include "chromeos/ash/components/osauth/public/auth_parts.h"

namespace ash {

namespace {

AuthPartsImpl* g_instance = nullptr;

}

// static
std::unique_ptr<AuthPartsImpl> AuthPartsImpl::CreateTestInstance() {
  std::unique_ptr<AuthPartsImpl> result = std::make_unique<AuthPartsImpl>();
  return result;
}

// static
std::unique_ptr<AuthParts> AuthParts::Create() {
  std::unique_ptr<AuthPartsImpl> result = std::make_unique<AuthPartsImpl>();
  // Create all parts here.
  return result;
}

// static
AuthParts* AuthParts::Get() {
  CHECK(g_instance);
  return g_instance;
}

AuthPartsImpl::AuthPartsImpl() {
  CHECK(!g_instance);
  g_instance = this;
}

AuthPartsImpl::~AuthPartsImpl() {
  CHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

}  // namespace ash
