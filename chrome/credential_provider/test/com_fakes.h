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
#include "chrome/credential_provider/gaiacp/gaia_credential_provider.h"
#include "chrome/credential_provider/gaiacp/gaia_credential_provider_i.h"
#include "chrome/credential_provider/test/test_credential_provider.h"

namespace credential_provider {

namespace testing {

#define DECLARE_IUNKOWN_NOQI_WITH_REF()                            \
  IFACEMETHODIMP QueryInterface(REFIID riid, void** ppv) override; \
  ULONG STDMETHODCALLTYPE AddRef() override;                       \
  ULONG STDMETHODCALLTYPE Release(void) override;                  \
  ULONG ref_count_ = 1;

///////////////////////////////////////////////////////////////////////////////

// Fake the CredentialProviderUserArray COM object.
class FakeCredentialProviderUser : public ICredentialProviderUser {
 public:
  FakeCredentialProviderUser(const wchar_t* sid, const wchar_t* username);
  virtual ~FakeCredentialProviderUser();

 private:
  // ICredentialProviderUser
  DECLARE_IUNKOWN_NOQI_WITH_REF()
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

  void SetAccountOptions(CREDENTIAL_PROVIDER_ACCOUNT_OPTIONS cpao) {
    cpao_ = cpao;
  }

 private:
  // ICredentialProviderUserArray
  DECLARE_IUNKOWN_NOQI_WITH_REF()
  IFACEMETHODIMP SetProviderFilter(REFGUID guidProviderToFilterTo) override;
  IFACEMETHODIMP GetAccountOptions(
      CREDENTIAL_PROVIDER_ACCOUNT_OPTIONS* cpao) override;
  IFACEMETHODIMP GetCount(DWORD* userCount) override;
  IFACEMETHODIMP GetAt(DWORD index, ICredentialProviderUser** user) override;

  std::vector<FakeCredentialProviderUser> users_;
  CREDENTIAL_PROVIDER_ACCOUNT_OPTIONS cpao_ = CPAO_NONE;
};

///////////////////////////////////////////////////////////////////////////////

// Fake OS imlpementation of ICredentialProviderEvents.
class FakeCredentialProviderEvents : public ICredentialProviderEvents {
 public:
  FakeCredentialProviderEvents();
  virtual ~FakeCredentialProviderEvents();

  // ICredentialProviderEvents
  DECLARE_IUNKOWN_NOQI_WITH_REF()
  IFACEMETHODIMP CredentialsChanged(UINT_PTR upAdviseContext) override;

  bool CredentialsChangedReceived() const { return did_change_; }
  void ResetCredentialsChangedReceived() { did_change_ = false; }

 private:
  bool did_change_ = false;
};

///////////////////////////////////////////////////////////////////////////////

// Fake OS imlpementation of ICredentialProviderEvents.
class FakeCredentialProviderCredentialEvents
    : public ICredentialProviderCredentialEvents {
 public:
  FakeCredentialProviderCredentialEvents();
  virtual ~FakeCredentialProviderCredentialEvents();

  // ICredentialProviderCredentialEvents
  DECLARE_IUNKOWN_NOQI_WITH_REF()
  IFACEMETHODIMP AppendFieldComboBoxItem(ICredentialProviderCredential* pcpc,
                                         DWORD dwFieldID,
                                         LPCWSTR pszItem) override;
  IFACEMETHODIMP DeleteFieldComboBoxItem(ICredentialProviderCredential* pcpc,
                                         DWORD dwFieldID,
                                         DWORD dwItem) override;
  IFACEMETHODIMP OnCreatingWindow(HWND* phwndOwner) override;
  IFACEMETHODIMP SetFieldBitmap(ICredentialProviderCredential* pcpc,
                                DWORD dwFieldID,
                                HBITMAP hbmp) override;
  IFACEMETHODIMP SetFieldCheckbox(ICredentialProviderCredential* pcpc,
                                  DWORD dwFieldID,
                                  BOOL bChecked,
                                  LPCWSTR pszLabel) override;
  IFACEMETHODIMP SetFieldComboBoxSelectedItem(
      ICredentialProviderCredential* pcpc,
      DWORD dwFieldID,
      DWORD dwSelectedItem) override;
  IFACEMETHODIMP SetFieldInteractiveState(
      ICredentialProviderCredential* pcpc,
      DWORD dwFieldID,
      CREDENTIAL_PROVIDER_FIELD_INTERACTIVE_STATE cpfis) override;
  IFACEMETHODIMP SetFieldState(ICredentialProviderCredential* pcpc,
                               DWORD dwFieldID,
                               CREDENTIAL_PROVIDER_FIELD_STATE cpfs) override;
  IFACEMETHODIMP SetFieldString(ICredentialProviderCredential* pcpc,
                                DWORD dwFieldID,
                                LPCWSTR psz) override;
  IFACEMETHODIMP SetFieldSubmitButton(ICredentialProviderCredential* pcpc,
                                      DWORD dwFieldID,
                                      DWORD dwAdjacentTo) override;
  CREDENTIAL_PROVIDER_FIELD_STATE GetFieldState(
      ICredentialProviderCredential* pcpc,
      DWORD dwFieldID);

 private:
  std::unordered_map<ICredentialProviderCredential*,
                     std::unordered_map<DWORD, CREDENTIAL_PROVIDER_FIELD_STATE>>
      field_states_;
};

///////////////////////////////////////////////////////////////////////////////

// Test implementation of GaiaCredentialProvider that stores information from
// OnUserAuthenticatedImpl.
class CTestGaiaCredentialProvider : public CGaiaCredentialProvider,
                                    public ITestCredentialProvider {
 public:
  const CComBSTR& STDMETHODCALLTYPE username() const override;
  const CComBSTR& STDMETHODCALLTYPE password() const override;
  const CComBSTR& STDMETHODCALLTYPE sid() const override;
  bool STDMETHODCALLTYPE credentials_changed_fired() const override;
  void STDMETHODCALLTYPE ResetCredentialsChangedFired() override;

  BEGIN_COM_MAP(CTestGaiaCredentialProvider)
  COM_INTERFACE_ENTRY(IGaiaCredentialProvider)
  COM_INTERFACE_ENTRY(ICredentialProviderSetUserArray)
  COM_INTERFACE_ENTRY(ICredentialProvider)
  COM_INTERFACE_ENTRY(ICredentialUpdateEventsHandler)
  COM_INTERFACE_ENTRY(ITestCredentialProvider)
  END_COM_MAP()

 protected:
  // CGaiaCredentialProvider
  HRESULT OnUserAuthenticatedImpl(IUnknown* credential,
                                  BSTR username,
                                  BSTR password,
                                  BSTR sid,
                                  BOOL fire_credentials_changed) override;

  CTestGaiaCredentialProvider();
  ~CTestGaiaCredentialProvider() override;

 private:
  CComBSTR username_;
  CComBSTR password_;
  CComBSTR sid_;
  BOOL credentials_changed_fired_ = FALSE;
};

}  // namespace testing

}  // namespace credential_provider

#endif  // CHROME_CREDENTIAL_PROVIDER_TEST_COM_FAKES_H_
