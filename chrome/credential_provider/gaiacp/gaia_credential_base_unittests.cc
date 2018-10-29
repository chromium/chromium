// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <atlbase.h>
#include <atlcom.h>
#include <atlcomcli.h>

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_writer.h"
#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/multiprocess_test.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/credential_provider/gaiacp/gaia_credential.h"
#include "chrome/credential_provider/gaiacp/gaia_credential_provider_i.h"
#include "chrome/credential_provider/gaiacp/gcp_strings.h"
#include "chrome/credential_provider/test/com_fakes.h"
#include "chrome/credential_provider/test/gcp_fakes.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/multiprocess_func_list.h"

namespace credential_provider {

namespace testing {

// Corresponding default email and username for tests that don't override them.
const char kDefaultEmail[] = "foo@gmail.com";
const wchar_t kDefaultUsername[] = L"foo";

namespace switches {

const char kGlsOutputFile[] = "gls-output-file";

}  // namespace switches

class DECLSPEC_UUID("3710aa3a-13c7-44c2-bc38-09ba137804d8") ITestCredential
    : public IUnknown {
 public:
  virtual HRESULT STDMETHODCALLTYPE SetGlsEmailAddress(const char* email) = 0;
  virtual HRESULT STDMETHODCALLTYPE WaitForGls() = 0;
  virtual BSTR STDMETHODCALLTYPE GetFinalUsername() = 0;
  virtual bool STDMETHODCALLTYPE AreCredentialsValid() = 0;
};

// Test implementation of an ICredentialProviderCredential backed by a Gaia
// account.  This class overrides some methods for testing purposes.
class ATL_NO_VTABLE CTestCredential
    : public CComObjectRootEx<CComMultiThreadModel>,
      public CGaiaCredentialBase,
      public ITestCredential {
 public:
  DECLARE_NO_REGISTRY()

  CTestCredential();
  ~CTestCredential();

  HRESULT FinalConstruct() { return S_OK; }
  void FinalRelease() {}

 private:
  BEGIN_COM_MAP(CTestCredential)
  COM_INTERFACE_ENTRY(IGaiaCredential)
  COM_INTERFACE_ENTRY(ICredentialProviderCredential)
  COM_INTERFACE_ENTRY(ITestCredential)
  END_COM_MAP()

  // ITestCredential.
  IFACEMETHODIMP SetGlsEmailAddress(const char* email) override;
  IFACEMETHODIMP WaitForGls() override;
  BSTR STDMETHODCALLTYPE GetFinalUsername() override;
  bool STDMETHODCALLTYPE AreCredentialsValid() override;

  // IGaiaCredential.
  IFACEMETHODIMP FinishAuthentication(BSTR username,
                                      BSTR password,
                                      BSTR fullname,
                                      BSTR* sid,
                                      BSTR* error_text) override;
  IFACEMETHODIMP OnUserAuthenticated(BSTR username,
                                     BSTR password,
                                     BSTR sid) override;

  // Overrides to build a dummy command line for testing.
  HRESULT GetGlsCommandline(const wchar_t* email,
                            base::CommandLine* command_line) override;

  // Override to prevent messagebox from showing up in tests.
  void DisplayErrorInUI(LONG status, LONG substatus, BSTR status_text) override;

  DECLARE_PROTECT_FINAL_CONSTRUCT()

  // Temporary file used to store JSON response from fake GLS.  This file is
  // used as the stdout of GLS.
  base::FilePath temp_json_file_;
  std::string gls_email_ = kDefaultEmail;
  base::WaitableEvent gls_done_;
};

CTestCredential::CTestCredential()
    : gls_done_(base::WaitableEvent::ResetPolicy::MANUAL,
                base::WaitableEvent::InitialState::NOT_SIGNALED) {
  base::CreateTemporaryFile(&temp_json_file_);
}

CTestCredential::~CTestCredential() {
  if (base::PathExists(temp_json_file_))
    base::DeleteFile(temp_json_file_, false);
}

HRESULT CTestCredential::SetGlsEmailAddress(const char* email) {
  gls_email_ = email;
  return S_OK;
}

HRESULT CTestCredential::WaitForGls() {
  return gls_done_.TimedWait(base::TimeDelta::FromSeconds(30))
             ? S_OK
             : HRESULT_FROM_WIN32(WAIT_TIMEOUT);
}

BSTR CTestCredential::GetFinalUsername() {
  return get_username();
}

bool CTestCredential::AreCredentialsValid() {
  return CGaiaCredentialBase::AreCredentialsValid();
}

HRESULT CTestCredential::FinishAuthentication(BSTR username,
                                              BSTR password,
                                              BSTR fullname,
                                              BSTR* sid,
                                              BSTR* error_text) {
  DCHECK(error_text);

  *error_text = nullptr;

  base::string16 comment(GetStringResource(IDS_USER_ACCOUNT_COMMENT));
  HRESULT hr = CreateNewUser(OSUserManager::Get(), OLE2CW(username),
                             OLE2CW(password), OLE2CW(fullname),
                             comment.c_str(), /*add_to_users_group=*/true, sid);
  EXPECT_EQ(S_OK, hr);
  return hr;
}

HRESULT CTestCredential::OnUserAuthenticated(BSTR username,
                                             BSTR password,
                                             BSTR sid) {
  HRESULT hr = FinishOnUserAuthenticated(username, password, sid);
  gls_done_.Signal();
  return hr;
}

HRESULT CTestCredential::GetGlsCommandline(const wchar_t* /*email*/,
                                           base::CommandLine* command_line) {
  base::DictionaryValue dict;
  dict.SetString(kKeyEmail, gls_email_);
  dict.SetString(kKeyFullname, "Full Name");
  dict.SetString(kKeyId, "1234567890");
  dict.SetString(kKeyMdmIdToken, "idt-123456");
  dict.SetString(kKeyPassword, "password");
  dict.SetString(kKeyRefreshToken, "rt-123456");
  dict.SetString(kKeyTokenHandle, "th-123456");

  std::string json;
  if (!base::JSONWriter::Write(dict, &json))
    return E_FAIL;

  if (base::WriteFile(temp_json_file_, json.c_str(), json.length()) == -1)
    return HRESULT_FROM_WIN32(::GetLastError());

  base::FilePath system_dir;
  if (!base::PathService::Get(base::DIR_SYSTEM, &system_dir))
    return HRESULT_FROM_WIN32(::GetLastError());

  *command_line = base::GetMultiProcessTestChildBaseCommandLine();
  command_line->AppendSwitchASCII(::switches::kTestChildProcess, "gls_main");
  command_line->AppendSwitchPath(switches::kGlsOutputFile, temp_json_file_);

  // Reset the manual event since GLS will be started upon return.
  gls_done_.Reset();
  return S_OK;
}

void CTestCredential::DisplayErrorInUI(LONG status,
                                       LONG substatus,
                                       BSTR status_text) {
  // This function is called instead of OnUserAuthenticated() when errors occur,
  // so signal that GLS is done.
  gls_done_.Signal();
}

// Writes the file specified by the command line argument kGlsOutputFile to
// stdout.  This is used as a fake GLS process for testing.  The files will
// be very small, maybe a couple of hundred characters, so fine to load into
// memory.
MULTIPROCESS_TEST_MAIN(gls_main) {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  base::FilePath path =
      command_line->GetSwitchValuePath(switches::kGlsOutputFile);

  std::string contents;
  if (base::ReadFileToString(path, &contents)) {
    HANDLE hstdout = ::GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD written;
    if (::WriteFile(hstdout, contents.c_str(), contents.length(), &written,
                    nullptr)) {
      return 0;
    }
  }

  return -1;
}

}  // namespace testing

namespace {

HRESULT CreateCredential(ICredentialProviderCredential** credential) {
  return CComCreator<CComObject<testing::CTestCredential>>::CreateInstance(
      nullptr, IID_ICredentialProviderCredential,
      reinterpret_cast<void**>(credential));
}

HRESULT CreateCredentialWithProvider(
    IGaiaCredentialProvider* provider,
    IGaiaCredential** gaia_credential,
    ICredentialProviderCredential** credential) {
  HRESULT hr = CreateCredential(credential);
  if (SUCCEEDED(hr)) {
    hr = (*credential)
             ->QueryInterface(IID_IGaiaCredential,
                              reinterpret_cast<void**>(gaia_credential));
    if (SUCCEEDED(hr))
      hr = (*gaia_credential)->Initialize(provider);
  }
  return hr;
}

}  // namespace

class GcpGaiaCredentialBaseTest : public ::testing::Test {
 public:
  GcpGaiaCredentialBaseTest();

  HRESULT StartLogonProcessAndWait(ICredentialProviderCredential* cred);

  FakeOSUserManager* fake_os_user_manager() { return &fake_os_user_manager_; }

 private:
  FakeOSProcessManager fake_os_process_manager_;
  FakeOSUserManager fake_os_user_manager_;
  FakeScopedLsaPolicyFactory fake_scoped_lsa_policy_factory_;
};

GcpGaiaCredentialBaseTest::GcpGaiaCredentialBaseTest() {
  // Create the special gaia account used to run GLS and save its password.

  BSTR sid;
  DWORD error;
  EXPECT_EQ(S_OK, fake_os_user_manager_.AddUser(kGaiaAccountName, L"password",
                                                L"fullname", L"comment", true,
                                                &sid, &error));

  auto policy = ScopedLsaPolicy::Create(POLICY_ALL_ACCESS);
  EXPECT_EQ(S_OK, policy->StorePrivateData(kLsaKeyGaiaPassword, L"password"));
}

HRESULT GcpGaiaCredentialBaseTest::StartLogonProcessAndWait(
    ICredentialProviderCredential* cred) {
  BOOL auto_login;
  EXPECT_EQ(S_OK, cred->SetSelected(&auto_login));

  // Logging on is an async process, so the call to GetSerialization() starts
  // the process, but when it returns it has not completed.
  CREDENTIAL_PROVIDER_GET_SERIALIZATION_RESPONSE cpgsr;
  CREDENTIAL_PROVIDER_CREDENTIAL_SERIALIZATION cpcs;
  wchar_t* status_text;
  CREDENTIAL_PROVIDER_STATUS_ICON status_icon;
  EXPECT_EQ(S_OK,
            cred->GetSerialization(&cpgsr, &cpcs, &status_text, &status_icon));
  EXPECT_EQ(nullptr, status_text);
  EXPECT_EQ(CPSI_NONE, status_icon);
  EXPECT_EQ(CPGSR_NO_CREDENTIAL_NOT_FINISHED, cpgsr);

  CComPtr<testing::ITestCredential> test;
  EXPECT_EQ(S_OK, cred->QueryInterface(__uuidof(testing::ITestCredential),
                                       reinterpret_cast<void**>(&test)));
  EXPECT_EQ(S_OK, test->WaitForGls());

  return S_OK;
}

TEST_F(GcpGaiaCredentialBaseTest, Advise) {
  CComPtr<ICredentialProviderCredential> cred;
  ASSERT_EQ(S_OK, CreateCredential(&cred));

  ASSERT_EQ(S_OK, cred->Advise(nullptr));
  ASSERT_EQ(S_OK, cred->UnAdvise());
}

TEST_F(GcpGaiaCredentialBaseTest, SetSelected) {
  CComPtr<ICredentialProviderCredential> cred;
  ASSERT_EQ(S_OK, CreateCredential(&cred));

  // A credential that has not attempted to sign in a user yet should return
  // false for |auto_login|.
  BOOL auto_login;
  ASSERT_EQ(S_OK, cred->SetSelected(&auto_login));
  ASSERT_FALSE(auto_login);
}

TEST_F(GcpGaiaCredentialBaseTest, GetSerialization_Start) {
  FakeGaiaCredentialProvider provider;

  CComPtr<IGaiaCredential> gaia_cred;
  CComPtr<ICredentialProviderCredential> cred;
  ASSERT_EQ(S_OK, CreateCredentialWithProvider(&provider, &gaia_cred, &cred));

  CComPtr<testing::ITestCredential> test;
  ASSERT_EQ(S_OK, cred.QueryInterface(&test));

  ASSERT_EQ(S_OK, StartLogonProcessAndWait(cred));

  ASSERT_EQ(S_OK, gaia_cred->Terminate());
}

TEST_F(GcpGaiaCredentialBaseTest, GetSerialization_Finish) {
  FakeGaiaCredentialProvider provider;

  // Start logon.
  CComPtr<IGaiaCredential> gaia_cred;
  CComPtr<ICredentialProviderCredential> cred;
  ASSERT_EQ(S_OK, CreateCredentialWithProvider(&provider, &gaia_cred, &cred));

  CComPtr<testing::ITestCredential> test;
  ASSERT_EQ(S_OK, cred.QueryInterface(&test));

  ASSERT_EQ(S_OK, StartLogonProcessAndWait(cred));

  // Now finish the logon.
  CREDENTIAL_PROVIDER_GET_SERIALIZATION_RESPONSE cpgsr;
  CREDENTIAL_PROVIDER_CREDENTIAL_SERIALIZATION cpcs;
  wchar_t* status_text;
  CREDENTIAL_PROVIDER_STATUS_ICON status_icon;
  ASSERT_EQ(S_OK,
            cred->GetSerialization(&cpgsr, &cpcs, &status_text, &status_icon));
  ASSERT_EQ(nullptr, status_text);
  ASSERT_EQ(CPSI_NONE, status_icon);
  ASSERT_EQ(CPGSR_RETURN_CREDENTIAL_FINISHED, cpgsr);
  ASSERT_LT(0u, cpcs.cbSerialization);
  ASSERT_NE(nullptr, cpcs.rgbSerialization);

  // State was reset.
  ASSERT_FALSE(test->AreCredentialsValid());

  // Make sure a "foo" user was created.
  PSID sid;
  ASSERT_EQ(S_OK, fake_os_user_manager()->GetUserSID(testing::kDefaultUsername,
                                                     &sid));
  ::LocalFree(sid);

  ASSERT_EQ(S_OK, gaia_cred->Terminate());
}

TEST_F(GcpGaiaCredentialBaseTest, StripEmailTLD) {
  USES_CONVERSION;
  FakeGaiaCredentialProvider provider;

  CComPtr<IGaiaCredential> gaia_cred;
  CComPtr<ICredentialProviderCredential> cred;
  ASSERT_EQ(S_OK, CreateCredentialWithProvider(&provider, &gaia_cred, &cred));

  CComPtr<testing::ITestCredential> test;
  ASSERT_EQ(S_OK, cred.QueryInterface(&test));
  ASSERT_EQ(S_OK, test->SetGlsEmailAddress("foo@imfl.info"));

  ASSERT_EQ(S_OK, StartLogonProcessAndWait(cred));

  ASSERT_STREQ(W2COLE(L"foo_imfl"), test->GetFinalUsername());

  ASSERT_EQ(S_OK, gaia_cred->Terminate());
}

TEST_F(GcpGaiaCredentialBaseTest, StripEmailTLD_Gmail) {
  USES_CONVERSION;
  FakeGaiaCredentialProvider provider;

  CComPtr<IGaiaCredential> gaia_cred;
  CComPtr<ICredentialProviderCredential> cred;
  ASSERT_EQ(S_OK, CreateCredentialWithProvider(&provider, &gaia_cred, &cred));

  CComPtr<testing::ITestCredential> test;
  ASSERT_EQ(S_OK, cred.QueryInterface(&test));
  ASSERT_EQ(S_OK, test->SetGlsEmailAddress("bar@gmail.com"));

  ASSERT_EQ(S_OK, StartLogonProcessAndWait(cred));

  ASSERT_STREQ(W2COLE(L"bar"), test->GetFinalUsername());

  ASSERT_EQ(S_OK, gaia_cred->Terminate());
}

TEST_F(GcpGaiaCredentialBaseTest, StripEmailTLD_Googlemail) {
  USES_CONVERSION;
  FakeGaiaCredentialProvider provider;

  CComPtr<IGaiaCredential> gaia_cred;
  CComPtr<ICredentialProviderCredential> cred;
  ASSERT_EQ(S_OK, CreateCredentialWithProvider(&provider, &gaia_cred, &cred));

  CComPtr<testing::ITestCredential> test;
  ASSERT_EQ(S_OK, cred.QueryInterface(&test));
  ASSERT_EQ(S_OK, test->SetGlsEmailAddress("toto@googlemail.com"));

  ASSERT_EQ(S_OK, StartLogonProcessAndWait(cred));

  ASSERT_STREQ(W2COLE(L"toto"), test->GetFinalUsername());

  ASSERT_EQ(S_OK, gaia_cred->Terminate());
}

TEST_F(GcpGaiaCredentialBaseTest, InvalidUsernameCharacters) {
  USES_CONVERSION;
  FakeGaiaCredentialProvider provider;

  CComPtr<IGaiaCredential> gaia_cred;
  CComPtr<ICredentialProviderCredential> cred;
  ASSERT_EQ(S_OK, CreateCredentialWithProvider(&provider, &gaia_cred, &cred));

  CComPtr<testing::ITestCredential> test;
  ASSERT_EQ(S_OK, cred.QueryInterface(&test));
  ASSERT_EQ(S_OK, test->SetGlsEmailAddress("a\\[]:|<>+=;?*z@gmail.com"));

  ASSERT_EQ(S_OK, StartLogonProcessAndWait(cred));

  ASSERT_STREQ(W2COLE(L"a____________z"), test->GetFinalUsername());

  ASSERT_EQ(S_OK, gaia_cred->Terminate());
}

TEST_F(GcpGaiaCredentialBaseTest, EmailTooLong) {
  USES_CONVERSION;
  FakeGaiaCredentialProvider provider;

  CComPtr<IGaiaCredential> gaia_cred;
  CComPtr<ICredentialProviderCredential> cred;
  ASSERT_EQ(S_OK, CreateCredentialWithProvider(&provider, &gaia_cred, &cred));

  CComPtr<testing::ITestCredential> test;
  ASSERT_EQ(S_OK, cred.QueryInterface(&test));
  ASSERT_EQ(S_OK,
            test->SetGlsEmailAddress("areallylongemailadressdude@gmail.com"));

  ASSERT_EQ(S_OK, StartLogonProcessAndWait(cred));

  ASSERT_STREQ(W2COLE(L"areallylongemailadre"), test->GetFinalUsername());

  ASSERT_EQ(S_OK, gaia_cred->Terminate());
}

TEST_F(GcpGaiaCredentialBaseTest, EmailTooLong2) {
  USES_CONVERSION;
  FakeGaiaCredentialProvider provider;

  CComPtr<IGaiaCredential> gaia_cred;
  CComPtr<ICredentialProviderCredential> cred;
  ASSERT_EQ(S_OK, CreateCredentialWithProvider(&provider, &gaia_cred, &cred));

  CComPtr<testing::ITestCredential> test;
  ASSERT_EQ(S_OK, cred.QueryInterface(&test));
  ASSERT_EQ(S_OK, test->SetGlsEmailAddress("foo@areallylongdomaindude.com"));

  ASSERT_EQ(S_OK, StartLogonProcessAndWait(cred));

  ASSERT_STREQ(W2COLE(L"foo_areallylongdomai"), test->GetFinalUsername());

  ASSERT_EQ(S_OK, gaia_cred->Terminate());
}

TEST_F(GcpGaiaCredentialBaseTest, EmailIsNoAt) {
  USES_CONVERSION;
  FakeGaiaCredentialProvider provider;

  CComPtr<IGaiaCredential> gaia_cred;
  CComPtr<ICredentialProviderCredential> cred;
  ASSERT_EQ(S_OK, CreateCredentialWithProvider(&provider, &gaia_cred, &cred));

  CComPtr<testing::ITestCredential> test;
  ASSERT_EQ(S_OK, cred.QueryInterface(&test));
  ASSERT_EQ(S_OK, test->SetGlsEmailAddress("foo"));

  ASSERT_EQ(S_OK, StartLogonProcessAndWait(cred));

  ASSERT_STREQ(W2COLE(L"foo_gmail"), test->GetFinalUsername());

  ASSERT_EQ(S_OK, gaia_cred->Terminate());
}

TEST_F(GcpGaiaCredentialBaseTest, EmailIsAtCom) {
  USES_CONVERSION;
  FakeGaiaCredentialProvider provider;

  CComPtr<IGaiaCredential> gaia_cred;
  CComPtr<ICredentialProviderCredential> cred;
  ASSERT_EQ(S_OK, CreateCredentialWithProvider(&provider, &gaia_cred, &cred));

  CComPtr<testing::ITestCredential> test;
  ASSERT_EQ(S_OK, cred.QueryInterface(&test));
  ASSERT_EQ(S_OK, test->SetGlsEmailAddress("@com"));

  ASSERT_EQ(S_OK, StartLogonProcessAndWait(cred));

  ASSERT_STREQ(W2COLE(L"_com"), test->GetFinalUsername());

  ASSERT_EQ(S_OK, gaia_cred->Terminate());
}

TEST_F(GcpGaiaCredentialBaseTest, EmailIsAtDotCom) {
  USES_CONVERSION;
  FakeGaiaCredentialProvider provider;

  CComPtr<IGaiaCredential> gaia_cred;
  CComPtr<ICredentialProviderCredential> cred;
  ASSERT_EQ(S_OK, CreateCredentialWithProvider(&provider, &gaia_cred, &cred));

  CComPtr<testing::ITestCredential> test;
  ASSERT_EQ(S_OK, cred.QueryInterface(&test));
  ASSERT_EQ(S_OK, test->SetGlsEmailAddress("@.com"));

  ASSERT_EQ(S_OK, StartLogonProcessAndWait(cred));

  ASSERT_STREQ(W2COLE(L"_.com"), test->GetFinalUsername());

  ASSERT_EQ(S_OK, gaia_cred->Terminate());
}

}  // namespace credential_provider
