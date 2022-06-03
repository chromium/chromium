// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBAUTHN_AUTHENTICATOR_LIST_OBSERVER_H_
#define CHROME_BROWSER_WEBAUTHN_AUTHENTICATOR_LIST_OBSERVER_H_

#include "chrome/browser/webauthn/authenticator_reference.h"

// Interface to observe mutations to an ObservableAuthenticatorList.
//
// This is currently only implemented by the the BleDeviceHoverListModel that is
// used by HoverListView to visualize an ObservableAuthenticatorList.
class AuthenticatorListObserver {
 public:
  AuthenticatorListObserver() = default;
  virtual ~AuthenticatorListObserver() = default;

  virtual void OnAuthenticatorAdded(
      const AuthenticatorReference& added_authenticator) = 0;
  virtual void OnAuthenticatorRemoved(
      const AuthenticatorReference& removed_authenticator) = 0;
};

#endif  // CHROME_BROWSER_WEBAUTHN_AUTHENTICATOR_LIST_OBSERVER_H_
