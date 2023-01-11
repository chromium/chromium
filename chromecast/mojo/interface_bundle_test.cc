// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/mojo/interface_bundle.h"

#include <string>

#include "base/functional/bind.h"
#include "base/test/task_environment.h"
#include "chromecast/mojo/remote_interfaces.h"
#include "chromecast/mojo/test/test_interfaces.test-mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;

namespace chromecast {

class MockInterface : public test::mojom::StringInterface,
                      public test::mojom::IntInterface,
                      public test::mojom::BoolInterface {
 public:
  MOCK_METHOD(void, StringMethod, (const std::string& s), (override));
  MOCK_METHOD(void, IntMethod, (int32_t i), (override));
  MOCK_METHOD(void, BoolMethod, (bool b), (override));
};

class MockErrorHandler {
 public:
  MOCK_METHOD(void, OnError, ());
};

class InterfaceBundleTest : public testing::Test {
 protected:
  InterfaceBundleTest() {}

  base::test::TaskEnvironment task_environment_;

  MockInterface mock_interface_;
};

TEST_F(InterfaceBundleTest, ExampleUsage) {
  InterfaceBundle bundle;
  RemoteInterfaces interfaces(bundle.CreateRemote());

  // ===========================================================================
  // Success Scenarios
  // ===========================================================================
  // Add an implementation of StringInterface to this bundle.
  ASSERT_TRUE(
      bundle.AddInterface<test::mojom::StringInterface>(&mock_interface_));

  // RemoteInterfaces dispenses Remotes on behalf of InterfaceBundle.
  mojo::Remote<test::mojom::StringInterface> remote;
  interfaces.BindNewPipe(&remote);
  ASSERT_TRUE(remote.is_bound());

  // The newly created Remote will call into the implementation.
  std::string s = "Hello, world!";
  EXPECT_CALL(mock_interface_, StringMethod(s));
  remote->StringMethod(s);
  task_environment_.RunUntilIdle();

  // ===========================================================================
  // Error Scenarios
  // ===========================================================================
  // IntInterface wasn't provided, but we allow a new Remote to be created.
  // However, once the request is rejected by InterfaceBundle, the Remote will
  // be disconnected.
  auto bad_remote = interfaces.CreateRemote<test::mojom::IntInterface>();
  ASSERT_TRUE(bad_remote.is_bound());
  ASSERT_TRUE(bad_remote.is_connected());

  task_environment_.RunUntilIdle();
  ASSERT_FALSE(bad_remote.is_connected());
}

TEST_F(InterfaceBundleTest, Lifetime) {
  MockErrorHandler error_handler_;
  mojo::Remote<test::mojom::StringInterface> remote;
  {
    InterfaceBundle bundle;
    RemoteInterfaces interfaces(bundle.CreateRemote());

    ASSERT_TRUE(
        bundle.AddInterface<test::mojom::StringInterface>(&mock_interface_));
    interfaces.BindNewPipe(&remote);

    // Verify the Remote is usable.
    std::string s = "Hello, world!";
    EXPECT_CALL(mock_interface_, StringMethod(s));
    remote->StringMethod(s);
    task_environment_.RunUntilIdle();

    remote.set_disconnect_handler(base::BindOnce(
        &MockErrorHandler::OnError, base::Unretained(&error_handler_)));

    // When the InterfaceBundle is destroyed, all dispensed Remotes will be
    // invalidated.
    EXPECT_CALL(error_handler_, OnError()).Times(1);
  }
  task_environment_.RunUntilIdle();
  ASSERT_FALSE(remote.is_connected());
}

TEST_F(InterfaceBundleTest, LateBinding) {
  InterfaceBundle bundle;
  // Don't link to the InterfaceBundle just yet.
  RemoteInterfaces interfaces;

  ASSERT_TRUE(
      bundle.AddInterface<test::mojom::StringInterface>(&mock_interface_));

  mojo::Remote<test::mojom::StringInterface> remote;
  interfaces.BindNewPipe(&remote);
  std::string s = "Hello, world!";

  // We do not have a link to an implementation yet, but the client can still
  // try to use the interface anyway.
  EXPECT_CALL(mock_interface_, StringMethod(s)).Times(0);
  remote->StringMethod(s);
  remote->StringMethod(s);
  remote->StringMethod(s);
  task_environment_.RunUntilIdle();

  // When we bind the provider late, all requests should be fulfilled.
  EXPECT_CALL(mock_interface_, StringMethod(s)).Times(3);
  interfaces.SetProvider(bundle.CreateRemote());
  task_environment_.RunUntilIdle();
}

}  // namespace chromecast
