// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_strip_api/tab_strip_controller_impl.h"

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "chrome/browser/ui/tabs/tab_strip_api/tab_strip_api.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/mojom/base/error.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Temporary scaffolding to link the mojo receiver with the impl. Once we
// we figure out how clients will connect with the service, this scaffolding
// should be removed.
class ServiceBridge {
 public:
  // impl_ currently doesn't do any interesting with the deps. nullptr for
  // now.
  ServiceBridge() : impl_{nullptr, nullptr} {}
  ServiceBridge(const ServiceBridge&) = delete;
  ServiceBridge operator=(const ServiceBridge&) = delete;
  ~ServiceBridge() = default;

  mojo::PendingRemote<tabs_api::mojom::TabStripController> GetRemote() {
    return service_.BindNewPipeAndPassRemote();
  }

 private:
  mojo::Receiver<tabs_api::mojom::TabStripController> service_{&impl_};
  TabStripControllerImpl impl_;
};

class TabStripControllerImplTest : public testing::Test {
 protected:
  TabStripControllerImplTest() = default;
  TabStripControllerImplTest(const TabStripControllerImplTest&) = delete;
  TabStripControllerImplTest operator=(const TabStripControllerImplTest&) =
      delete;
  ~TabStripControllerImplTest() override = default;

  void SetUp() override { client_.Bind(bridge_.GetRemote()); }

  mojo::Remote<tabs_api::mojom::TabStripController> client_;

 private:
  base::test::TaskEnvironment task_environment_;
  ServiceBridge bridge_;
};

TEST_F(TabStripControllerImplTest, CreatNewTab) {
  base::RunLoop loop;

  // TODO(crbug.com/40841428): the synchronous version of the method does not
  // correctly unwrap the shadow type. This should be fixed in mojo.
  client_->CreateNewTab(base::BindLambdaForTesting(
      [&](base::expected<bool, mojo_base::mojom::ErrorPtr> result) {
        EXPECT_FALSE(result.has_value());
        EXPECT_EQ(result.error()->code, mojo_base::mojom::Code::kUnimplemented);
        loop.Quit();
      }));

  loop.Run();
}

}  // namespace
