// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_HTTP_AUTH_OBSERVER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_HTTP_AUTH_OBSERVER_H_

#include <string>

#include "base/macros.h"
#include "base/strings/string16.h"
#include "components/autofill/core/common/password_form.h"

namespace password_manager {

// Abstract class to define a communication interface for the http-auth UI and
// the provider of stored credentials.
class HttpAuthObserver {
 public:
  HttpAuthObserver() = default;

  // Called by the model when |credentials| has been identified as a match for
  // the pending login prompt. Checks that the realm matches, and passes
  // |credentials| to OnAutofillDataAvailableInternal.
  virtual void OnAutofillDataAvailable(const base::string16& username,
                                       const base::string16& password) = 0;

  virtual void OnLoginModelDestroying() = 0;

 protected:
  virtual ~HttpAuthObserver() = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(HttpAuthObserver);
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_HTTP_AUTH_OBSERVER_H_
