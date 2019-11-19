// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CREDENTIAL_PROVIDER_TEST_TEST_CREDENTIAL_PROVIDER_H_
#define CHROME_CREDENTIAL_PROVIDER_TEST_TEST_CREDENTIAL_PROVIDER_H_

#include <atlbase.h>
#include <atlcom.h>
#include <atlcomcli.h>

namespace credential_provider {

namespace testing {

class DECLSPEC_UUID("d8108fd0-1e0d-4853-9a8a-1f6aed8bf64d")
    ITestCredentialProvider : public IUnknown {
 public:
  virtual const CComBSTR& STDMETHODCALLTYPE username() const = 0;
  virtual const CComBSTR& STDMETHODCALLTYPE password() const = 0;
  virtual const CComBSTR& STDMETHODCALLTYPE sid() const = 0;
  virtual bool STDMETHODCALLTYPE credentials_changed_fired() const = 0;
  virtual void STDMETHODCALLTYPE ResetCredentialsChangedFired() = 0;
};

}  // namespace testing
}  // namespace credential_provider

#endif  // CHROME_CREDENTIAL_PROVIDER_TEST_TEST_CREDENTIAL_PROVIDER_H_
