// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_DEVICE_SIGNALS_CONSENT_MOCK_CONSENT_REQUESTER_H_
#define CHROME_BROWSER_UI_DEVICE_SIGNALS_CONSENT_MOCK_CONSENT_REQUESTER_H_

#include "chrome/browser/ui/device_signals_consent/consent_requester.h"
#include "testing/gmock/include/gmock/gmock.h"

// Controller that displays the modal dialog for collecting user consent for
// sharing device signals.
class MockConsentRequester : public ConsentRequester {
 public:
  MockConsentRequester();
  ~MockConsentRequester() override;

  MockConsentRequester(const MockConsentRequester&) = delete;
  MockConsentRequester& operator=(const MockConsentRequester&) = delete;

  MOCK_METHOD(void, RequestConsent, (RequestConsentCallback), (override));
};

#endif  // CHROME_BROWSER_UI_DEVICE_SIGNALS_CONSENT_MOCK_CONSENT_REQUESTER_H_
