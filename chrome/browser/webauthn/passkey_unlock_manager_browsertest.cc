// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/passkey_unlock_manager.h"

#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/test/integration/sync_service_impl_harness.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/webauthn/enclave_authenticator_browsertest_base.h"
#include "chrome/browser/webauthn/enclave_manager.h"
#include "chrome/browser/webauthn/enclave_manager_factory.h"
#include "chrome/browser/webauthn/passkey_unlock_manager_factory.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "device/fido/features.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/gurl.h"

// These tests are also disabled under MSAN. The enclave subprocess is written
// in Rust and FFI from Rust to C++ doesn't work in Chromium at this time
// (crbug.com/1369167).
#if !defined(MEMORY_SANITIZER)

namespace webauthn {

namespace {

constexpr char kAccountsGoogleHost[] = "accounts.google.com";
#if BUILDFLAG(IS_CHROMEOS)
constexpr char kEncryptionUnlockDesktopPath[] = "/encryption/unlock/chromeos";
#else
constexpr char kEncryptionUnlockDesktopPath[] = "/encryption/unlock/desktop";
#endif

// Custom request handler to serve the encryption unlock page.
std::unique_ptr<net::test_server::HttpResponse>
HandleEncryptionUnlockPageRequest(
    const net::test_server::HttpRequest& request) {
  GURL absolute_url = request.base_url.Resolve(request.relative_url);

  if (absolute_url.host() == kAccountsGoogleHost &&
      absolute_url.path() == kEncryptionUnlockDesktopPath) {
    auto http_response =
        std::make_unique<net::test_server::BasicHttpResponse>();
    http_response->set_code(net::HTTP_OK);
    http_response->set_content_type("text/html");
    http_response->set_content("<html><body>OK</body></html>");
    return http_response;
  }
  return nullptr;  // Let other handlers process if not matched.
}

class MockPasskeyUnlockManagerObserver : public PasskeyUnlockManager::Observer {
 public:
  MOCK_METHOD(void, OnPasskeyUnlockManagerStateChanged, (), (override));
  MOCK_METHOD(void, OnPasskeyUnlockManagerShuttingDown, (), (override));
  MOCK_METHOD(void, OnPasskeyUnlockManagerIsReady, (), (override));
};

class PasskeyUnlockManagerBrowserTest : public EnclaveAuthenticatorTestBase {
 public:
  PasskeyUnlockManagerBrowserTest() { EnableUVKeySupport(); }

  ~PasskeyUnlockManagerBrowserTest() override = default;

  webauthn::PasskeyUnlockManager* passkey_unlock_manager() {
    return webauthn::PasskeyUnlockManagerFactory::GetForProfile(
        browser()->profile());
  }

  signin::IdentityManager* identity_manager() {
    return IdentityManagerFactory::GetForProfile(browser()->profile());
  }

  SyncServiceImplHarness* sync_harness() {
    if (sync_harness_) {
      return sync_harness_.get();
    }

    sync_harness_ = SyncServiceImplHarness::Create(
        browser()->profile(), SyncServiceImplHarness::SigninType::FAKE_SIGNIN);
    return sync_harness_.get();
  }

  void EnableSync() {
    ASSERT_TRUE(sync_harness()->SetupSync());
    ASSERT_TRUE(
        identity_manager()->HasPrimaryAccount(signin::ConsentLevel::kSync));
 }

 protected:
  void SetUpOnMainThread() override {
    EnclaveAuthenticatorTestBase::SetUpOnMainThread();
    // Make the browser's network stack route requests to the
    // embedded_test_server.
    host_resolver()->AddRule("*", "127.0.0.1");
    embedded_test_server()->RegisterRequestHandler(
        base::BindRepeating(&HandleEncryptionUnlockPageRequest));
    ASSERT_TRUE(embedded_test_server()->Start());

    base::test::TestFuture<void> load_future;
    EnclaveManager* enclave_manager =
        EnclaveManagerFactory::GetAsEnclaveManagerForProfile(
            browser()->profile());
    enclave_manager->Load(load_future.GetCallback());
    ASSERT_TRUE(load_future.Wait());

    EnableSync();
  }

 private:
  base::test::ScopedFeatureList feature_list_{device::kPasskeyUnlockManager};
};

IN_PROC_BROWSER_TEST_F(PasskeyUnlockManagerBrowserTest, IsCreated) {
  EXPECT_NE(passkey_unlock_manager(), nullptr);
}

// Test that calling `OpenTabWithPasskeyUnlockChallenge` opens a new tab with a
// Passkey unlock URL.
IN_PROC_BROWSER_TEST_F(PasskeyUnlockManagerBrowserTest,
                       OpensNewTabWithPasskeyUnlockUrl) {
  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  int initial_tab_count = tab_strip_model->count();

  PasskeyUnlockManager::OpenTabWithPasskeyUnlockChallenge(browser());

  // Ensure that a new tab with an expected URL has been added.
  EXPECT_EQ(initial_tab_count + 1, tab_strip_model->count());
  content::WebContents* new_contents = tab_strip_model->GetActiveWebContents();
  ASSERT_TRUE(new_contents);
#if BUILDFLAG(IS_CHROMEOS)
  EXPECT_EQ(GURL("https://accounts.google.com/encryption/unlock/"
                 "chromeos?kdi=CAESDgoMaHdfcHJvdGVjdGVk"),
            new_contents->GetVisibleURL());
#else
  EXPECT_EQ(GURL("https://accounts.google.com/encryption/unlock/"
                 "desktop?kdi=CAESDgoMaHdfcHJvdGVjdGVk"),
            new_contents->GetVisibleURL());
#endif
}

IN_PROC_BROWSER_TEST_F(PasskeyUnlockManagerBrowserTest,
                       NotifyObserversOnEnclaveStateUpdated) {
  AddTestPasskeyToModel();
  testing::NiceMock<MockPasskeyUnlockManagerObserver> observer;
  passkey_unlock_manager()->AddObserver(&observer);

  base::test::TestFuture<void> event_future;
  EXPECT_CALL(observer, OnPasskeyUnlockManagerStateChanged())
      .WillOnce([&event_future]() {
        // Signal the TestFuture when OnPasskeyUnlockManagerStateChanged is
        // called.
        event_future.SetValue();
      });

  ASSERT_TRUE(passkey_unlock_manager()->ShouldDisplayErrorUi());
  // Simulate the operations that make the EnclaveManager ready. This causes a
  // state change, which should be observed by the PasskeyUnlockManager.
  SimulateSuccessfulGpmPinCreation("123456");

  EXPECT_TRUE(event_future.Wait());
  ASSERT_FALSE(passkey_unlock_manager()->ShouldDisplayErrorUi());

  passkey_unlock_manager()->RemoveObserver(&observer);
}

}  // namespace

}  // namespace webauthn

#endif  // !defined(MEMORY_SANITIZER)
