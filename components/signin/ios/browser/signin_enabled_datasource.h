// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_IOS_BROWSER_SIGNIN_ENABLED_DATASOURCE_H_
#define COMPONENTS_SIGNIN_IOS_BROWSER_SIGNIN_ENABLED_DATASOURCE_H_

namespace signin {

// A class which provides the information of whether signin is enabled.
class SigninEnabledDataSource {
 public:
  // Whether the sign-in is not disabled.
  virtual bool SigninEnabled() const = 0;
};

}  // namespace signin

#endif  // COMPONENTS_SIGNIN_IOS_BROWSER_SIGNIN_ENABLED_DATASOURCE_H_
