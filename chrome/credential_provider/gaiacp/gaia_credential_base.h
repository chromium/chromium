// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CREDENTIAL_PROVIDER_GAIACP_GAIA_CREDENTIAL_BASE_H_
#define CHROME_CREDENTIAL_PROVIDER_GAIACP_GAIA_CREDENTIAL_BASE_H_

#include "chrome/credential_provider/gaiacp/stdafx.h"

#include <memory>

#include "base/strings/string16.h"
#include "base/values.h"
#include "base/win/scoped_handle.h"
#include "base/win/scoped_process_information.h"
#include "chrome/credential_provider/gaiacp/gaia_credential_provider_i.h"
#include "chrome/credential_provider/gaiacp/gcp_utils.h"
#include "chrome/credential_provider/gaiacp/grit/gaia_resources.h"
#include "chrome/credential_provider/gaiacp/scoped_handle.h"

namespace base {
class CommandLine;
class FilePath;
}  // namespace base

namespace credential_provider {

class OSProcessManager;
class OSUserManager;

enum FIELDID {
  FID_DESCRIPTION,
  FID_SUBMIT,
  FID_PROVIDER_LOGO,
  FID_PROVIDER_LABEL,
  FIELD_COUNT  // Must be last.
};

// Implementation of an ICredentialProviderCredential backed by a Gaia account.
// This is used as a base class for the COM objects that implement first time
// sign in and password update.
class ATL_NO_VTABLE CGaiaCredentialBase
    : public IGaiaCredential,
      public ICredentialProviderCredential2 {
 public:
  // Size in wchar_t of string buffer to pass account information to background
  // process to save that information into the registry.
  static const int kAccountInfoBufferSize = 2048;

  // Called when the DLL is registered or unregistered.
  static HRESULT OnDllRegisterServer();
  static HRESULT OnDllUnregisterServer();

  // Saves gaia information in the OS account that was just created.
  static HRESULT SaveAccountInfo(const base::DictionaryValue& properties);

  // Allocates a BSTR from a DLL string resource given by |id|.
  static BSTR AllocErrorString(UINT id);

  // Gets the directory where the credential provider is installed.
  static HRESULT GetInstallDirectory(base::FilePath* path);

  // Passed to WaitForLoginUI().
  struct UIProcessInfo {
    UIProcessInfo();
    ~UIProcessInfo();

    CComPtr<IGaiaCredential> credential;
    base::win::ScopedHandle logon_token;
    base::win::ScopedProcessInformation procinfo;
    StdParentHandles parent_handles;
  };

 protected:
  CGaiaCredentialBase();
  ~CGaiaCredentialBase();

  // Creates a new windows OS user with the given username, fullname, and
  // password on the local machine.  Returns the SID of the new user.
  static HRESULT CreateNewUser(OSUserManager* manager,
                               const wchar_t* username,
                               const wchar_t* password,
                               const wchar_t* fullname,
                               const wchar_t* comment,
                               bool add_to_users_group,
                               BSTR* sid);

  // Members to access user credentials.
  const CComBSTR& get_username() const { return username_; }
  const CComBSTR& get_password() const { return password_; }
  const CComBSTR& get_sid() const { return sid_; }
  bool AreCredentialsValid() const;

  // Gets the string value for the given credential UI field.
  HRESULT GetStringValueImpl(DWORD field_id, wchar_t** value);

  // Called from derived classes when implementing OnUserAuthenticated().
  HRESULT FinishOnUserAuthenticated(BSTR username, BSTR password, BSTR sid);

 private:
  // Resets the state of the credential, forgetting any username or password
  // that may have been set previously.  Derived classes may override to
  // perform more state resetting if needed, but should always call the base
  // class method.
  virtual void ResetInternalState();

  // Derived classes should implement this function to return an email address
  // only when reauthenticating the user.
  virtual HRESULT GetEmailForReauth(wchar_t* email, size_t length);

  // Gets the command line to run the Gaia Logon stub (GLS).
  virtual HRESULT GetGlsCommandline(const wchar_t* email,
                                    base::CommandLine* command_line);

  // Display error message to the user.  Virtual so that tests can override.
  virtual void DisplayErrorInUI(LONG status, LONG substatus, BSTR status_text);

  // Called from GetSerialization() to handle auto-logon.  If the credential
  // has enough information in internal state to auto-logon, the two arguments
  // are filled in as needed and S_OK is returned.  S_FALSE is returned to
  // indicate that the UI should be shown to the user.
  HRESULT HandleAutologon(
      CREDENTIAL_PROVIDER_GET_SERIALIZATION_RESPONSE* pcpgsr,
      CREDENTIAL_PROVIDER_CREDENTIAL_SERIALIZATION* pcpcs);

  // Writes value to omaha registry to record that GCP has been used.
  static void TellOmahaDidRun();

  // Sets up the envriroment for the Gaia logon stub, runs it, and waits for
  // it to finish in a background thread.
  HRESULT CreateAndRunLogonStub();

  // Creates a restricted token for the Gaia account that can be used to run
  // the logon stub.  The returned SID is a logon SID and not the SID of the
  // Gaia account.
  static HRESULT CreateGaiaLogonToken(base::win::ScopedHandle* token,
                                      PSID* sid);

  // Forks the logon stub process and waits for it to start.
  static HRESULT ForkGaiaLogonStub(OSProcessManager* process_manager,
                                   const base::CommandLine& command_line,
                                   UIProcessInfo* uiprocinfo);

  // Forks a stub process to save account information for a user.
  static HRESULT ForkSaveAccountInfoStub(
      const std::unique_ptr<base::DictionaryValue>& dict,
      BSTR* status_text);

  // The param is a pointer to a UIProcessInfo struct.  This function must
  // release the memory for this structure using delete operator.
  static unsigned __stdcall WaitForLoginUI(void* param);
  static HRESULT WaitForLoginUIImpl(
      UIProcessInfo* uiprocinfo,
      std::unique_ptr<base::DictionaryValue>* properties,
      BSTR* status_text);

  // ICredentialProviderCredential2
  IFACEMETHODIMP Advise(ICredentialProviderCredentialEvents* cpce) override;
  IFACEMETHODIMP UnAdvise(void) override;
  IFACEMETHODIMP SetSelected(BOOL* auto_login) override;
  IFACEMETHODIMP SetDeselected(void) override;
  IFACEMETHODIMP GetFieldState(
      DWORD dwFieldID,
      CREDENTIAL_PROVIDER_FIELD_STATE* pcpfs,
      CREDENTIAL_PROVIDER_FIELD_INTERACTIVE_STATE* pcpfis) override;
  IFACEMETHODIMP GetStringValue(DWORD dwFieldID, wchar_t** ppsz) override;
  IFACEMETHODIMP GetBitmapValue(DWORD dwFieldID, HBITMAP* phbmp) override;
  IFACEMETHODIMP GetCheckboxValue(DWORD field_id,
                                  BOOL* pbChecked,
                                  wchar_t** ppszLabel) override;
  IFACEMETHODIMP GetSubmitButtonValue(DWORD field_id,
                                      DWORD* pdwAdjacentTo) override;
  IFACEMETHODIMP GetComboBoxValueCount(DWORD field_id,
                                       DWORD* pcItems,
                                       DWORD* pdwSelectedItem) override;
  IFACEMETHODIMP GetComboBoxValueAt(DWORD field_id,
                                    DWORD dwItem,
                                    wchar_t** ppszItem) override;
  IFACEMETHODIMP SetStringValue(DWORD field_id, const wchar_t* psz) override;
  IFACEMETHODIMP SetCheckboxValue(DWORD field_id, BOOL bChecked) override;
  IFACEMETHODIMP SetComboBoxSelectedValue(DWORD field_id,
                                          DWORD dwSelectedItem) override;
  IFACEMETHODIMP CommandLinkClicked(DWORD field_id) override;
  IFACEMETHODIMP GetSerialization(
      CREDENTIAL_PROVIDER_GET_SERIALIZATION_RESPONSE* cpgsr,
      CREDENTIAL_PROVIDER_CREDENTIAL_SERIALIZATION* cpcs,
      wchar_t** status_text,
      CREDENTIAL_PROVIDER_STATUS_ICON* status_icon) override;
  IFACEMETHODIMP ReportResult(
      NTSTATUS ntsStatus,
      NTSTATUS ntsSubstatus,
      wchar_t** ppszOptionalStatusText,
      CREDENTIAL_PROVIDER_STATUS_ICON* pcpsiOptionalStatusIcon) override;
  IFACEMETHODIMP GetUserSid(wchar_t** sid) override;

  // IGaiaCredential
  IFACEMETHODIMP Initialize(IGaiaCredentialProvider* provider) override;
  IFACEMETHODIMP Terminate() override;
  IFACEMETHODIMP FinishAuthentication(BSTR username,
                                      BSTR password,
                                      BSTR fullname,
                                      BSTR* sid,
                                      BSTR* error_text) override;
  IFACEMETHODIMP OnUserAuthenticated(BSTR username,
                                     BSTR password,
                                     BSTR sid) override;
  IFACEMETHODIMP ReportError(LONG status,
                             LONG substatus,
                             BSTR status_text) override;

  CComPtr<ICredentialProviderCredentialEvents> events_;

  // Handle to the logon UI process.
  HANDLE logon_ui_process_;

  CComPtr<IGaiaCredentialProvider> provider_;

  // Information about the just created or re-auth-ed user.
  CComBSTR username_;
  CComBSTR password_;
  CComBSTR sid_;

  // Whether success or failure, these members hold information about result.
  NTSTATUS result_status_;
  NTSTATUS result_substatus_;
  base::string16 result_status_text_;
};

}  // namespace credential_provider

#endif  // CHROME_CREDENTIAL_PROVIDER_GAIACP_GAIA_CREDENTIAL_BASE_H_
