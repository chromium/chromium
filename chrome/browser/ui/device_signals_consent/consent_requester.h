// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_DEVICE_SIGNALS_CONSENT_CONSENT_REQUESTER_H_
#define CHROME_BROWSER_UI_DEVICE_SIGNALS_CONSENT_CONSENT_REQUESTER_H_

#include <memory>

#include "base/functional/callback.h"

class Profile;

using RequestConsentCallback = base::RepeatingCallback<void()>;

// Class that requests the collection of user consent for
// sharing device signals.
class ConsentRequester {
 public:
  virtual ~ConsentRequester() = default;

  ConsentRequester(const ConsentRequester&) = delete;
  ConsentRequester& operator=(const ConsentRequester&) = delete;

  static std::unique_ptr<ConsentRequester> CreateConsentRequester(
      Profile* profile);

  static void SetConsentRequesterForTest(
      std::unique_ptr<ConsentRequester> consent_requester);

  virtual void RequestConsent(RequestConsentCallback callback) = 0;

 protected:
  ConsentRequester() = default;
};

#endif  // CHROME_BROWSER_UI_DEVICE_SIGNALS_CONSENT_CONSENT_REQUESTER_H_
