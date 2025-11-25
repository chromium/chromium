// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/password_credential_fetcher.h"

#include <memory>
#include <vector>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/test/task_environment.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/password_manager/core/browser/fake_form_fetcher.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_store/mock_password_store_interface.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "content/public/browser/web_contents.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using password_manager::PasswordForm;
using testing::_;
using testing::NiceMock;
using testing::Return;

namespace {

const std::string_view kTestURL = "https://example.com";

PasswordForm CreatePasswordForm(std::u16string_view username,
                                std::u16string_view password) {
  PasswordForm form;
  form.url = GURL(kTestURL);
  form.username_value = std::u16string(username);
  form.password_value = std::u16string(password);
  return form;
}

PasswordForm CreateFederatedPasswordForm(std::u16string_view username,
                                         std::u16string_view password) {
  PasswordForm form = CreatePasswordForm(username, password);
  form.federation_origin = url::SchemeHostPort(GURL("https://idp.example.com"));
  return form;
}

PasswordForm CreateEmptyUsernamePasswordForm(std::u16string_view password) {
  PasswordForm form;
  form.url = GURL(kTestURL);
  form.password_value = std::u16string(password);
  return form;
}

class MockPasswordManagerClient
    : public password_manager::StubPasswordManagerClient {
 public:
  MOCK_METHOD(password_manager::PasswordStoreInterface*,
              GetProfilePasswordStore,
              (),
              (const, override));
  MOCK_METHOD(password_manager::PasswordStoreInterface*,
              GetAccountPasswordStore,
              (),
              (const, override));
};

}  // namespace

class PasswordCredentialFetcherTest : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    auto form_fetcher = std::make_unique<password_manager::FakeFormFetcher>();
    form_fetcher_ = form_fetcher.get();
    client_ = std::make_unique<NiceMock<MockPasswordManagerClient>>();

    profile_store_ = base::MakeRefCounted<
        NiceMock<password_manager::MockPasswordStoreInterface>>();
    account_store_ = base::MakeRefCounted<
        NiceMock<password_manager::MockPasswordStoreInterface>>();

    ON_CALL(*client_, GetProfilePasswordStore)
        .WillByDefault(Return(profile_store_.get()));
    ON_CALL(*client_, GetAccountPasswordStore)
        .WillByDefault(Return(account_store_.get()));

    fetcher_ = PasswordCredentialFetcher::CreateForTesting(
        web_contents()->GetPrimaryMainFrame(), std::move(form_fetcher),
        client_.get());
  }

  void TearDown() override {
    form_fetcher_ = nullptr;
    fetcher_ = nullptr;
    client_ = nullptr;
    profile_store_.reset();
    account_store_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

 protected:
  std::unique_ptr<PasswordCredentialFetcher> fetcher_;
  raw_ptr<password_manager::FakeFormFetcher> form_fetcher_;
  std::unique_ptr<NiceMock<MockPasswordManagerClient>> client_;
  scoped_refptr<NiceMock<password_manager::MockPasswordStoreInterface>>
      profile_store_;
  scoped_refptr<NiceMock<password_manager::MockPasswordStoreInterface>>
      account_store_;
};

TEST_F(PasswordCredentialFetcherTest, NoPasswords) {
  form_fetcher_->SetBestMatches({});

  fetcher_->FetchPasswords(
      GURL(kTestURL),
      base::BindOnce([](std::vector<std::unique_ptr<PasswordForm>> passwords) {
        EXPECT_TRUE(passwords.empty());
      }));
  form_fetcher_->NotifyFetchCompleted();
}

TEST_F(PasswordCredentialFetcherTest, FetchPasswords) {
  form_fetcher_->SetBestMatches({CreatePasswordForm(u"user", u"password")});

  fetcher_->FetchPasswords(
      GURL(kTestURL),
      base::BindOnce([](std::vector<std::unique_ptr<PasswordForm>> passwords) {
        EXPECT_EQ(passwords.size(), 1u);
        EXPECT_EQ(passwords[0]->username_value, u"user");
      }));
  form_fetcher_->NotifyFetchCompleted();
}

TEST_F(PasswordCredentialFetcherTest, FilterFederated) {
  form_fetcher_->SetBestMatches(
      {CreateFederatedPasswordForm(u"user", u"password")});

  fetcher_->FetchPasswords(
      GURL(kTestURL),
      base::BindOnce([](std::vector<std::unique_ptr<PasswordForm>> passwords) {
        EXPECT_TRUE(passwords.empty());
      }));
  form_fetcher_->NotifyFetchCompleted();
}

TEST_F(PasswordCredentialFetcherTest, FilterEmptyUsername) {
  form_fetcher_->SetBestMatches({CreateEmptyUsernamePasswordForm(u"password")});

  fetcher_->FetchPasswords(
      GURL(kTestURL),
      base::BindOnce([](std::vector<std::unique_ptr<PasswordForm>> passwords) {
        EXPECT_TRUE(passwords.empty());
      }));
  form_fetcher_->NotifyFetchCompleted();
}

TEST_F(PasswordCredentialFetcherTest, UpdateDateLastUsed) {
  PasswordForm form = CreatePasswordForm(u"user", u"password");
  form.in_store = password_manager::PasswordForm::Store::kProfileStore |
                  password_manager::PasswordForm::Store::kAccountStore;
  form_fetcher_->SetBestMatches({form});

  // Expect UpdateLogin to be called on both stores.
  EXPECT_CALL(*profile_store_, UpdateLogin);
  EXPECT_CALL(*account_store_, UpdateLogin);

  fetcher_->UpdateDateLastUsed(u"user", u"password");
}

TEST_F(PasswordCredentialFetcherTest, UpdateDateLastUsed_ProfileStoreOnly) {
  PasswordForm form = CreatePasswordForm(u"user", u"password");
  form.in_store = password_manager::PasswordForm::Store::kProfileStore;
  form_fetcher_->SetBestMatches({form});

  // Expect UpdateLogin to be called only on profile store.
  EXPECT_CALL(*profile_store_, UpdateLogin);
  EXPECT_CALL(*account_store_, UpdateLogin).Times(0);

  fetcher_->UpdateDateLastUsed(u"user", u"password");
}
