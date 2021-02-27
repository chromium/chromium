// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBAUTHN_OBSERVABLE_AUTHENTICATOR_LIST_H_
#define CHROME_BROWSER_WEBAUTHN_OBSERVABLE_AUTHENTICATOR_LIST_H_

#include <stddef.h>

#include <string>
#include <vector>

#include "base/macros.h"
#include "base/strings/string_piece.h"
#include "chrome/browser/webauthn/authenticator_reference.h"

class AuthenticatorListObserver;

// List of AuthenticatorReference maintained by AuthenticatorRequestDialogModel
// that BleDeviceHoverListModel observes to add views to WebAuthN UI modal
// dialog views.
class ObservableAuthenticatorList {
 public:
  ObservableAuthenticatorList();
  ~ObservableAuthenticatorList();

  void AddAuthenticator(AuthenticatorReference authenticator);
  void RemoveAuthenticator(base::StringPiece authenticator_id);
  void RemoveAllAuthenticators();
  AuthenticatorReference* GetAuthenticator(base::StringPiece authenticator_id);

  void SetObserver(AuthenticatorListObserver* observer);
  void RemoveObserver();

  std::vector<AuthenticatorReference>& authenticator_list() {
    return authenticator_list_;
  }

  size_t size() const { return authenticator_list_.size(); }

 private:
  using AuthenticatorListIterator =
      std::vector<AuthenticatorReference>::iterator;

  AuthenticatorListIterator GetAuthenticatorIterator(
      base::StringPiece authenticator_id);

  std::vector<AuthenticatorReference> authenticator_list_;
  AuthenticatorListObserver* observer_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(ObservableAuthenticatorList);
};

#endif  // CHROME_BROWSER_WEBAUTHN_OBSERVABLE_AUTHENTICATOR_LIST_H_
