// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/ios/browser/fake_signin_enabled_datasource.h"

namespace signin {

bool FakeSigninEnabledDataSource::SigninEnabled() const {
  return true;
}

}  // namespace signin
