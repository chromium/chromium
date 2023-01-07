// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_HTTP_AUTH_OBSERVER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_HTTP_AUTH_OBSERVER_H_

#include <string>

namespace password_manager {

// Abstract class to define a communication interface for the http-auth UI and
// the provider of stored credentials.
class HttpAuthObserver {
 public:
  HttpAuthObserver() = default;

  HttpAuthObserver(const HttpAuthObserver&) = delete;
  HttpAuthObserver& operator=(const HttpAuthObserver&) = delete;

  // Called by the model when |credentials| has been identified as a match for
  // the pending login prompt. Checks that the realm matches, and passes
  // |credentials| to OnAutofillDataAvailableInternal.
  virtual void OnAutofillDataAvailable(const std::u16string& username,
                                       const std::u16string& password) = 0;

  virtual void OnLoginModelDestroying() = 0;

 protected:
  virtual ~HttpAuthObserver() = default;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_HTTP_AUTH_OBSERVER_H_
