// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/session_manager/core/fake_session_manager_delegate.h"

namespace session_manager {

FakeSessionManagerDelegate::FakeSessionManagerDelegate() = default;

FakeSessionManagerDelegate::~FakeSessionManagerDelegate() = default;

void FakeSessionManagerDelegate::RequestSignOut() {
  ++request_sign_out_count_;
}

int FakeSessionManagerDelegate::request_sign_out_count() const {
  return request_sign_out_count_;
}

}  // namespace session_manager
