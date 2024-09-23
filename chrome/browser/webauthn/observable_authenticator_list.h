// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBAUTHN_OBSERVABLE_AUTHENTICATOR_LIST_H_
#define CHROME_BROWSER_WEBAUTHN_OBSERVABLE_AUTHENTICATOR_LIST_H_

#include <stddef.h>

#include <string_view>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/webauthn/authenticator_reference.h"

class AuthenticatorListObserver;

// List of AuthenticatorReference maintained by
// AuthenticatorRequestDialogController that BleDeviceHoverListModel observes to
// add views to WebAuthN UI modal dialog views.
class ObservableAuthenticatorList {
 public:
  ObservableAuthenticatorList();

  ObservableAuthenticatorList(const ObservableAuthenticatorList&);
  ObservableAuthenticatorList(ObservableAuthenticatorList&&);
  ObservableAuthenticatorList& operator=(const ObservableAuthenticatorList&) =
      delete;
  ObservableAuthenticatorList& operator=(ObservableAuthenticatorList&&);

  ~ObservableAuthenticatorList();

  void AddAuthenticator(AuthenticatorReference authenticator);
  void RemoveAuthenticator(std::string_view authenticator_id);
  void RemoveAllAuthenticators();
  AuthenticatorReference* GetAuthenticator(std::string_view authenticator_id);

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
      std::string_view authenticator_id);

  std::vector<AuthenticatorReference> authenticator_list_;
  raw_ptr<AuthenticatorListObserver> observer_ = nullptr;
};

#endif  // CHROME_BROWSER_WEBAUTHN_OBSERVABLE_AUTHENTICATOR_LIST_H_
