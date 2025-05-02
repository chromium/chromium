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

class TabsApiControllerImplTest : public testing::Test {
 protected:
  TabsApiControllerImplTest() = default;
  TabsApiControllerImplTest(const TabsApiControllerImplTest&) = delete;
  TabsApiControllerImplTest operator=(const TabsApiControllerImplTest&) =
      delete;
  ~TabsApiControllerImplTest() override = default;

  void SetUp() override { client_.Bind(bridge_.GetRemote()); }

  mojo::Remote<tabs_api::mojom::TabStripController> client_;

 private:
  base::test::TaskEnvironment task_environment_;
  ServiceBridge bridge_;
};

TEST_F(TabsApiControllerImplTest, CreatNewTab) {
  tabs_api::mojom::TabStripController::CreateNewTabResult result;
  bool success = client_->CreateNewTab(&result);

  ASSERT_TRUE(success);
  ASSERT_FALSE(result.has_value());
  ASSERT_EQ(result.error()->code, mojo_base::mojom::Code::kUnimplemented);
}

}  // namespace
