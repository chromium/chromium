// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/picker/picker_client_impl.h"

#include <memory>
#include <utility>

#include "ash/picker/picker_controller.h"
#include "base/functional/bind.h"
#include "base/test/test_future.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/user_manager/fake_user_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/network/test/test_shared_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

struct TestServerSetup {
  std::unique_ptr<net::test_server::EmbeddedTestServer> server;
  net::test_server::EmbeddedTestServerHandle handle;
};

TestServerSetup CreateAndStartTestServer(
    net::test_server::EmbeddedTestServer::HandleRequestCallback
        handle_request_callback) {
  auto test_server = std::make_unique<net::test_server::EmbeddedTestServer>();
  test_server->RegisterRequestHandler(std::move(handle_request_callback));
  net::test_server::EmbeddedTestServerHandle handle =
      test_server->StartAndReturnHandle();
  return {
      .server = std::move(test_server),
      .handle = std::move(handle),
  };
}

class PickerClientImplTest : public testing::Test {
 public:
  PickerClientImplTest()
      : task_environment_(content::BrowserTaskEnvironment::IO_MAINLOOP),
        test_shared_url_loader_factory_(
            base::MakeRefCounted<network::TestSharedURLLoaderFactory>()),
        fake_user_manager_(std::make_unique<user_manager::FakeUserManager>()),
        testing_profile_manager_(TestingBrowserProcess::GetGlobal()) {}

  void SetUp() override { ASSERT_TRUE(testing_profile_manager_.SetUp()); }
  void TearDown() override {
    for (const user_manager::User* user : fake_user_manager_->GetUsers()) {
      fake_user_manager_->OnUserProfileWillBeDestroyed(user->GetAccountId());
    }
  }

  // Returns the user manager used in this test, logged into a fake user.
  user_manager::UserManager* GetUserManagerLoggedInAsFakeUser() {
    AccountId account_id = AccountId::FromUserEmail("test@test");

    const user_manager::User* user = fake_user_manager_->AddUser(account_id);
    fake_user_manager_->UserLoggedIn(account_id, user->username_hash(),
                                     /*browser_restart=*/false,
                                     /*is_child=*/false);
    TestingProfile* profile = CreateTestingProfileForAccount(account_id);
    fake_user_manager_->OnUserProfileCreated(account_id, profile->GetPrefs());
    return fake_user_manager_.Get();
  }

 private:
  TestingProfile* CreateTestingProfileForAccount(const AccountId& account_id) {
    return testing_profile_manager_.CreateTestingProfile(
        account_id.GetUserEmail(), /*is_main_profile=*/false,
        test_shared_url_loader_factory_);
  }

  content::BrowserTaskEnvironment task_environment_;
  scoped_refptr<network::SharedURLLoaderFactory>
      test_shared_url_loader_factory_;
  // Keep `fake_user_manager_` before `testing_profile_manager_` to match
  // destruction order in production:
  // https://crsrc.org/c/chrome/browser/ash/chrome_browser_main_parts_ash.cc;l=1668;drc=c7da8fba0e20c71d61e5c78ecd6a3872c4c56e6c
  // https://crsrc.org/c/chrome/browser/ash/chrome_browser_main_parts_ash.cc;l=1719;drc=c7da8fba0e20c71d61e5c78ecd6a3872c4c56e6c
  user_manager::TypedScopedUserManager<user_manager::FakeUserManager>
      fake_user_manager_;
  TestingProfileManager testing_profile_manager_;
};

TEST_F(PickerClientImplTest, DownloadGifReturnsGifOnSuccess) {
  ash::PickerController controller;
  PickerClientImpl client(&controller, GetUserManagerLoggedInAsFakeUser());
  TestServerSetup server_setup = CreateAndStartTestServer(base::BindRepeating(
      [](const net::test_server::HttpRequest& request)
          -> std::unique_ptr<net::test_server::HttpResponse> {
        auto http_response =
            std::make_unique<net::test_server::BasicHttpResponse>();
        http_response->set_code(net::HTTP_OK);
        http_response->set_content("hello");
        http_response->set_content_type("text/plain");
        return http_response;
      }));

  base::test::TestFuture<const std::string&> future;
  client.DownloadGifToString(ash::ValidGifUrl::CreateForTesting(
                                 server_setup.server->GetURL("/test.gif")),
                             future.GetCallback());

  EXPECT_EQ(future.Get(), "hello");
}

TEST_F(PickerClientImplTest, DownloadGifReturnsEmptyOnFailure) {
  ash::PickerController controller;
  PickerClientImpl client(&controller, GetUserManagerLoggedInAsFakeUser());
  TestServerSetup server_setup = CreateAndStartTestServer(base::BindRepeating(
      [](const net::test_server::HttpRequest& request)
          -> std::unique_ptr<net::test_server::HttpResponse> {
        auto http_response =
            std::make_unique<net::test_server::BasicHttpResponse>();
        http_response->set_code(net::HTTP_NOT_FOUND);
        return http_response;
      }));

  base::test::TestFuture<const std::string&> future;
  client.DownloadGifToString(ash::ValidGifUrl::CreateForTesting(
                                 server_setup.server->GetURL("/test.gif")),
                             future.GetCallback());

  EXPECT_EQ(future.Get(), "");
}

}  // namespace
