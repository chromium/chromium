// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/credential_provider/test/com_fakes.h"

#include <sddl.h>  // For ConvertSidToStringSid()

#include "base/check.h"
#include "chrome/credential_provider/gaiacp/gaia_credential.h"
#include "chrome/credential_provider/gaiacp/gaia_credential_other_user.h"
#include "chrome/credential_provider/gaiacp/os_user_manager.h"
#include "chrome/credential_provider/gaiacp/reauth_credential.h"
#include "chrome/credential_provider/gaiacp/stdafx.h"
#include "chrome/credential_provider/test/test_credential.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace credential_provider {

namespace testing {

// This class is used to implement a test credential based off a
// CGaiaCredential.
class ATL_NO_VTABLE CTestGaiaCredential
    : public CTestCredentialBase<CGaiaCredential> {
 public:
  DECLARE_NO_REGISTRY()

  CTestGaiaCredential();
  ~CTestGaiaCredential();

 private:
  BEGIN_COM_MAP(CTestGaiaCredential)
  COM_INTERFACE_ENTRY(IGaiaCredential)
  COM_INTERFACE_ENTRY(ICredentialProviderCredential)
  COM_INTERFACE_ENTRY(ITestCredential)
  END_COM_MAP()
};

CTestGaiaCredential::CTestGaiaCredential() = default;

CTestGaiaCredential::~CTestGaiaCredential() = default;

// This class is used to implement a test credential based off a
// COtherUserGaiaCredential.
class ATL_NO_VTABLE CTestOtherUserGaiaCredential
    : public CTestCredentialBase<COtherUserGaiaCredential> {
 public:
  DECLARE_NO_REGISTRY()

  CTestOtherUserGaiaCredential();
  ~CTestOtherUserGaiaCredential();

 private:
  BEGIN_COM_MAP(CTestOtherUserGaiaCredential)
  COM_INTERFACE_ENTRY(IGaiaCredential)
  COM_INTERFACE_ENTRY(ICredentialProviderCredential)
  COM_INTERFACE_ENTRY(ICredentialProviderCredential2)
  COM_INTERFACE_ENTRY(ITestCredential)
  END_COM_MAP()
};

CTestOtherUserGaiaCredential::CTestOtherUserGaiaCredential() = default;

CTestOtherUserGaiaCredential::~CTestOtherUserGaiaCredential() = default;

#define IMPL_IUNKOWN_NOQI_WITH_REF(cls)                               \
  IFACEMETHODIMP cls::QueryInterface(REFIID riid, void** ppv) {       \
    return E_NOTIMPL;                                                 \
  }                                                                   \
  ULONG cls::AddRef() { return ::InterlockedIncrement(&ref_count_); } \
  ULONG cls::Release(void) {                                          \
    DCHECK(ref_count_ > 0);                                           \
    return ::InterlockedDecrement(&ref_count_);                       \
  }

///////////////////////////////////////////////////////////////////////////////

FakeCredentialProviderUser::FakeCredentialProviderUser(const wchar_t* sid,
                                                       const wchar_t* username)
    : sid_(sid), username_(username) {}

FakeCredentialProviderUser::~FakeCredentialProviderUser() {
  EXPECT_EQ(ref_count_, 1u);
}

HRESULT FakeCredentialProviderUser::GetSid(wchar_t** sid) {
  DWORD length = sid_.length() + 1;
  *sid = static_cast<wchar_t*>(::CoTaskMemAlloc(length * sizeof(wchar_t)));
  EXPECT_EQ(0, wcscpy_s(*sid, length, sid_.c_str()));
  return S_OK;
}

HRESULT FakeCredentialProviderUser::GetProviderID(GUID* providerID) {
  return E_NOTIMPL;
}

HRESULT FakeCredentialProviderUser::GetStringValue(REFPROPERTYKEY key,
                                                   wchar_t** value) {
  if (key != PKEY_Identity_UserName)
    return E_INVALIDARG;

  DWORD length = username_.length() + 1;
  *value = static_cast<wchar_t*>(::CoTaskMemAlloc(length * sizeof(wchar_t)));
  EXPECT_EQ(0, wcscpy_s(*value, length, username_.c_str()));
  return S_OK;
}

HRESULT FakeCredentialProviderUser::GetValue(REFPROPERTYKEY key,
                                             PROPVARIANT* value) {
  return E_NOTIMPL;
}

IMPL_IUNKOWN_NOQI_WITH_REF(FakeCredentialProviderUser)

///////////////////////////////////////////////////////////////////////////////

FakeCredentialProviderUserArray::FakeCredentialProviderUserArray() {}

FakeCredentialProviderUserArray::~FakeCredentialProviderUserArray() {
  EXPECT_EQ(ref_count_, 1u);
}

HRESULT FakeCredentialProviderUserArray::SetProviderFilter(
    REFGUID guidProviderToFilterTo) {
  return S_OK;
}

HRESULT FakeCredentialProviderUserArray::GetAccountOptions(
    CREDENTIAL_PROVIDER_ACCOUNT_OPTIONS* cpao) {
  DCHECK(cpao);
  *cpao = cpao_;
  return S_OK;
}

HRESULT FakeCredentialProviderUserArray::GetCount(DWORD* count) {
  *count = users_.size();
  return S_OK;
}

HRESULT FakeCredentialProviderUserArray::GetAt(DWORD index,
                                               ICredentialProviderUser** user) {
  EXPECT_LT(index, users_.size());
  *user = &users_[index];
  (*user)->AddRef();
  return S_OK;
}

IMPL_IUNKOWN_NOQI_WITH_REF(FakeCredentialProviderUserArray)

///////////////////////////////////////////////////////////////////////////////

FakeCredentialProviderEvents::FakeCredentialProviderEvents() {}

FakeCredentialProviderEvents::~FakeCredentialProviderEvents() {
  EXPECT_EQ(ref_count_, 1u);
}

HRESULT FakeCredentialProviderEvents::CredentialsChanged(
    UINT_PTR upAdviseContext) {
  did_change_ = true;
  return S_OK;
}

IMPL_IUNKOWN_NOQI_WITH_REF(FakeCredentialProviderEvents)

///////////////////////////////////////////////////////////////////////////////

FakeCredentialProviderCredentialEvents::
    FakeCredentialProviderCredentialEvents() {}

FakeCredentialProviderCredentialEvents::
    ~FakeCredentialProviderCredentialEvents() {}

HRESULT FakeCredentialProviderCredentialEvents::AppendFieldComboBoxItem(
    ICredentialProviderCredential* pcpc,
    DWORD dwFieldID,
    LPCWSTR pszItem) {
  return S_OK;
}

HRESULT FakeCredentialProviderCredentialEvents::DeleteFieldComboBoxItem(
    ICredentialProviderCredential* pcpc,
    DWORD dwFieldID,
    DWORD dwItem) {
  return S_OK;
}

HRESULT FakeCredentialProviderCredentialEvents::OnCreatingWindow(
    HWND* phwndOwner) {
  return S_OK;
}

HRESULT FakeCredentialProviderCredentialEvents::SetFieldBitmap(
    ICredentialProviderCredential* pcpc,
    DWORD dwFieldID,
    HBITMAP hbmp) {
  return S_OK;
}

HRESULT FakeCredentialProviderCredentialEvents::SetFieldCheckbox(
    ICredentialProviderCredential* pcpc,
    DWORD dwFieldID,
    BOOL bChecked,
    LPCWSTR pszLabel) {
  return S_OK;
}

HRESULT FakeCredentialProviderCredentialEvents::SetFieldComboBoxSelectedItem(
    ICredentialProviderCredential* pcpc,
    DWORD dwFieldID,
    DWORD dwSelectedItem) {
  return S_OK;
}

HRESULT FakeCredentialProviderCredentialEvents::SetFieldInteractiveState(
    ICredentialProviderCredential* pcpc,
    DWORD dwFieldID,
    CREDENTIAL_PROVIDER_FIELD_INTERACTIVE_STATE cpfis) {
  return S_OK;
}

HRESULT FakeCredentialProviderCredentialEvents::SetFieldState(
    ICredentialProviderCredential* pcpc,
    DWORD dwFieldID,
    CREDENTIAL_PROVIDER_FIELD_STATE cpfs) {
  field_states_[pcpc][dwFieldID] = cpfs;
  return S_OK;
}

HRESULT FakeCredentialProviderCredentialEvents::SetFieldString(
    ICredentialProviderCredential* pcpc,
    DWORD dwFieldID,
    LPCWSTR psz) {
  if (psz != nullptr) {
    std::wstring copy_wchart(psz);
    field_string_[pcpc][dwFieldID] = copy_wchart;
  }
  return S_OK;
}

HRESULT FakeCredentialProviderCredentialEvents::SetFieldSubmitButton(
    ICredentialProviderCredential* pcpc,
    DWORD dwFieldID,
    DWORD dwAdjacentTo) {
  return S_OK;
}

CREDENTIAL_PROVIDER_FIELD_STATE
FakeCredentialProviderCredentialEvents::GetFieldState(
    ICredentialProviderCredential* pcpc,
    DWORD dwFieldID) {
  DCHECK(field_states_.count(pcpc));
  DCHECK(field_states_[pcpc].count(dwFieldID));

  return field_states_[pcpc][dwFieldID];
}

LPCWSTR FakeCredentialProviderCredentialEvents::GetFieldString(
    ICredentialProviderCredential* pcpc,
    DWORD dwFieldID) {
  DCHECK(field_string_.count(pcpc));
  DCHECK(field_string_[pcpc].count(dwFieldID));

  return field_string_[pcpc][dwFieldID].c_str();
}

IMPL_IUNKOWN_NOQI_WITH_REF(FakeCredentialProviderCredentialEvents)

///////////////////////////////////////////////////////////////////////////////

CTestGaiaCredentialProvider::CTestGaiaCredentialProvider() {
  // Set functions for creating test credentials of all types.
  SetCredentialCreatorFunctionsForTesting(
      [](CGaiaCredentialProvider::GaiaCredentialComPtrStorage*
             cred_ptr_storage) {
        return CComCreator<CComObject<CTestGaiaCredential>>::CreateInstance(
            nullptr, IID_PPV_ARGS(&cred_ptr_storage->gaia_cred));
      },
      [](CGaiaCredentialProvider::GaiaCredentialComPtrStorage*
             cred_ptr_storage) {
        return CComCreator<CComObject<CTestOtherUserGaiaCredential>>::
            CreateInstance(nullptr, IID_PPV_ARGS(&cred_ptr_storage->gaia_cred));
      },
      [](CGaiaCredentialProvider::GaiaCredentialComPtrStorage*
             cred_ptr_storage) {
        return CComCreator<CComObject<testing::CTestCredentialForInherited<
            CReauthCredential, IReauthCredential>>>::
            CreateInstance(nullptr, IID_PPV_ARGS(&cred_ptr_storage->gaia_cred));
      });
}

CTestGaiaCredentialProvider::~CTestGaiaCredentialProvider() {}

const CComBSTR& CTestGaiaCredentialProvider::username() const {
  return username_;
}

const CComBSTR& CTestGaiaCredentialProvider::password() const {
  return password_;
}

const CComBSTR& CTestGaiaCredentialProvider::sid() const {
  return sid_;
}

bool CTestGaiaCredentialProvider::credentials_changed_fired() const {
  return credentials_changed_fired_;
}

void CTestGaiaCredentialProvider::ResetCredentialsChangedFired() {
  credentials_changed_fired_ = FALSE;
}

HRESULT CTestGaiaCredentialProvider::OnUserAuthenticatedImpl(
    IUnknown* credential,
    BSTR username,
    BSTR password,
    BSTR sid,
    BOOL fire_credentials_changed) {
  username_ = username;
  password_ = password;
  sid_ = sid;
  credentials_changed_fired_ = fire_credentials_changed;
  return CGaiaCredentialProvider::OnUserAuthenticatedImpl(
      credential, username, password, sid, fire_credentials_changed);
}

}  // namespace testing

}  // namespace credential_provider
