// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/credential_provider/gaiacp/stdafx.h"
#include "chrome/credential_provider/test/com_fakes.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace credential_provider {

#define IMPL_IUNKOWN_NOQI_NOREF(cls)                            \
  IFACEMETHODIMP cls::QueryInterface(REFIID riid, void** ppv) { \
    return E_NOTIMPL;                                           \
  }                                                             \
  ULONG cls::AddRef() { return 0; }                             \
  ULONG cls::Release(void) { return 0; }

///////////////////////////////////////////////////////////////////////////////

FakeCredentialProviderUser::FakeCredentialProviderUser(const wchar_t* sid,
                                                       const wchar_t* username)
    : sid_(sid), username_(username) {}

FakeCredentialProviderUser::~FakeCredentialProviderUser() {}

HRESULT STDMETHODCALLTYPE FakeCredentialProviderUser::GetSid(wchar_t** sid) {
  DWORD length = sid_.length() + 1;
  *sid = static_cast<wchar_t*>(::CoTaskMemAlloc(length * sizeof(wchar_t)));
  EXPECT_EQ(0, wcscpy_s(*sid, length, sid_.c_str()));
  return S_OK;
}

HRESULT STDMETHODCALLTYPE
FakeCredentialProviderUser::GetProviderID(GUID* providerID) {
  return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE
FakeCredentialProviderUser::GetStringValue(REFPROPERTYKEY key,
                                           wchar_t** value) {
  if (key != PKEY_Identity_UserName)
    return E_INVALIDARG;

  DWORD length = username_.length() + 1;
  *value =
      static_cast<wchar_t*>(::CoTaskMemAlloc(length * sizeof(wchar_t)));
  EXPECT_EQ(0, wcscpy_s(*value, length, username_.c_str()));
  return S_OK;
}

HRESULT STDMETHODCALLTYPE
FakeCredentialProviderUser::GetValue(REFPROPERTYKEY key, PROPVARIANT* value) {
  return E_NOTIMPL;
}

IMPL_IUNKOWN_NOQI_NOREF(FakeCredentialProviderUser);

///////////////////////////////////////////////////////////////////////////////

FakeCredentialProviderUserArray::FakeCredentialProviderUserArray() {}

FakeCredentialProviderUserArray::~FakeCredentialProviderUserArray() {}

HRESULT FakeCredentialProviderUserArray::SetProviderFilter(
    REFGUID guidProviderToFilterTo) {
  return S_OK;
}

HRESULT FakeCredentialProviderUserArray::GetAccountOptions(
    CREDENTIAL_PROVIDER_ACCOUNT_OPTIONS* credentialProviderAccountOptions) {
  return E_NOTIMPL;
}

HRESULT FakeCredentialProviderUserArray::GetCount(DWORD* count) {
  *count = users_.size();
  return S_OK;
}

HRESULT FakeCredentialProviderUserArray::GetAt(DWORD index,
                                               ICredentialProviderUser** user) {
  EXPECT_LT(index, users_.size());
  *user = &users_[index];
  return S_OK;
}

IMPL_IUNKOWN_NOQI_NOREF(FakeCredentialProviderUserArray);

///////////////////////////////////////////////////////////////////////////////

FakeCredentialProviderEvents::FakeCredentialProviderEvents() {}

FakeCredentialProviderEvents::~FakeCredentialProviderEvents() {}

HRESULT FakeCredentialProviderEvents::CredentialsChanged(
    UINT_PTR upAdviseContext) {
  did_change_ = true;
  return S_OK;
}

IMPL_IUNKOWN_NOQI_NOREF(FakeCredentialProviderEvents);

///////////////////////////////////////////////////////////////////////////////

FakeGaiaCredentialProvider::FakeGaiaCredentialProvider() {}

FakeGaiaCredentialProvider::~FakeGaiaCredentialProvider() {}

HRESULT FakeGaiaCredentialProvider::OnUserAuthenticated(IUnknown* credential,
                                                        BSTR username,
                                                        BSTR password,
                                                        BSTR sid) {
  username_ = username;
  password_ = password;
  sid_ = sid;
  return S_OK;
}

IMPL_IUNKOWN_NOQI_NOREF(FakeGaiaCredentialProvider);

}  // namespace credential_provider
