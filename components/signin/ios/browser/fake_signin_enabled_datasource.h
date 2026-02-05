// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_IOS_BROWSER_FAKE_SIGNIN_ENABLED_DATASOURCE_H_
#define COMPONENTS_SIGNIN_IOS_BROWSER_FAKE_SIGNIN_ENABLED_DATASOURCE_H_

#import "components/signin/ios/browser/signin_enabled_datasource.h"

namespace signin {

// A class that states that signin is enabled.
class FakeSigninEnabledDataSource : public signin::SigninEnabledDataSource {
 public:
  bool SigninEnabled() const final;
};

}  // namespace signin
#endif  // COMPONENTS_SIGNIN_IOS_BROWSER_FAKE_SIGNIN_ENABLED_DATASOURCE_H_
