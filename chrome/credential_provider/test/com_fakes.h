// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CREDENTIAL_PROVIDER_TEST_COM_FAKES_H_
#define CHROME_CREDENTIAL_PROVIDER_TEST_COM_FAKES_H_

#include <atlcomcli.h>
#include <credentialprovider.h>
#include <propkey.h>

#include <vector>

#include "base/strings/string16.h"
#include "chrome/credential_provider/gaiacp/gaia_credential_provider_i.h"

namespace credential_provider {

///////////////////////////////////////////////////////////////////////////////

// Fake the CredentialProviderUserArray COM object.
class FakeCredentialProviderUser : public ICredentialProviderUser {
 public:
  FakeCredentialProviderUser(const wchar_t* sid, const wchar_t* username);
  virtual ~FakeCredentialProviderUser();

 private:
  // ICredentialProviderUser
  IFACEMETHODIMP QueryInterface(REFIID riid, void** ppv) override;
  ULONG STDMETHODCALLTYPE AddRef() override;
  ULONG STDMETHODCALLTYPE Release(void) override;
  HRESULT STDMETHODCALLTYPE GetSid(wchar_t** sid) override;
  HRESULT STDMETHODCALLTYPE GetProviderID(GUID* providerID) override;
  HRESULT STDMETHODCALLTYPE GetStringValue(REFPROPERTYKEY key,
                                           wchar_t** stringValue) override;
  HRESULT STDMETHODCALLTYPE GetValue(REFPROPERTYKEY key,
                                     PROPVARIANT* value) override;

  base::string16 sid_;
  base::string16 username_;
};

///////////////////////////////////////////////////////////////////////////////

// Fake the CredentialProviderUserArray COM object.
class FakeCredentialProviderUserArray : public ICredentialProviderUserArray {
 public:
  FakeCredentialProviderUserArray();
  virtual ~FakeCredentialProviderUserArray();

  void AddUser(const wchar_t* sid, const wchar_t* username) {
    users_.emplace_back(sid, username);
  }

 private:
  // ICredentialProviderUserArray
  IFACEMETHODIMP QueryInterface(REFIID riid, void** ppv) override;
  ULONG STDMETHODCALLTYPE AddRef() override;
  ULONG STDMETHODCALLTYPE Release(void) override;
  IFACEMETHODIMP SetProviderFilter(REFGUID guidProviderToFilterTo) override;
  IFACEMETHODIMP GetAccountOptions(
      CREDENTIAL_PROVIDER_ACCOUNT_OPTIONS* credentialProviderAccountOptions)
      override;
  IFACEMETHODIMP GetCount(DWORD* userCount) override;
  IFACEMETHODIMP GetAt(DWORD index, ICredentialProviderUser** user) override;

  std::vector<FakeCredentialProviderUser> users_;
};

///////////////////////////////////////////////////////////////////////////////

// Fake OS imlpementation of ICredentialProviderEvents.
class FakeCredentialProviderEvents : public ICredentialProviderEvents {
 public:
  FakeCredentialProviderEvents();
  virtual ~FakeCredentialProviderEvents();

  // ICredentialProviderEvents
  IFACEMETHODIMP QueryInterface(REFIID riid, void** ppv) override;
  ULONG STDMETHODCALLTYPE AddRef() override;
  ULONG STDMETHODCALLTYPE Release(void) override;
  IFACEMETHODIMP CredentialsChanged(UINT_PTR upAdviseContext) override;

 private:
  bool did_change_ = false;
};

///////////////////////////////////////////////////////////////////////////////

// Fake the GaiaCredentialProvider COM object.
class FakeGaiaCredentialProvider : public IGaiaCredentialProvider {
 public:
  FakeGaiaCredentialProvider();
  virtual ~FakeGaiaCredentialProvider();

  const CComBSTR& username() const { return username_; }
  const CComBSTR& password() const { return password_; }
  const CComBSTR& sid() const { return sid_; }

 private:
  // IGaiaCredentialProvider
  IFACEMETHODIMP QueryInterface(REFIID riid, void** ppv) override;
  ULONG STDMETHODCALLTYPE AddRef() override;
  ULONG STDMETHODCALLTYPE Release(void) override;
  IFACEMETHODIMP OnUserAuthenticated(IUnknown* credential,
                                     BSTR username,
                                     BSTR password,
                                     BSTR sid) override;

  CComBSTR username_;
  CComBSTR password_;
  CComBSTR sid_;
};

}  // namespace credential_provider

#endif  // CHROME_CREDENTIAL_PROVIDER_TEST_COM_FAKES_H_
