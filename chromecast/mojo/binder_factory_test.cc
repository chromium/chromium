// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/mojo/binder_factory.h"

#include <string>

#include "base/functional/bind.h"
#include "base/test/task_environment.h"
#include "chromecast/mojo/test/test_interfaces.test-mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
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

class BinderFactoryTest : public testing::Test {
 protected:
  BinderFactoryTest() {}

  base::test::TaskEnvironment task_environment_;

  MockInterface mock_interface_;
};

TEST_F(BinderFactoryTest, BinderFactoryBind) {
  BinderFactory<test::mojom::StringInterface> s_factory(&mock_interface_);

  mojo::Remote<test::mojom::StringInterface> s_remote;
  s_factory.Bind(s_remote.BindNewPipeAndPassReceiver());

  // Verify implementation is connected.
  std::string s = "Hello, world!";
  EXPECT_CALL(mock_interface_, StringMethod(s));
  s_remote->StringMethod(s);
  task_environment_.RunUntilIdle();
}

TEST_F(BinderFactoryTest, BinderFactoryGetBinder) {
  BinderFactory<test::mojom::StringInterface> s_factory(&mock_interface_);

  auto binder = s_factory.GetBinder();
  ASSERT_TRUE(!binder.is_null());

  mojo::Remote<test::mojom::StringInterface> s_remote;
  s_factory.Bind(s_remote.BindNewPipeAndPassReceiver());

  // Verify implementation is connected.
  std::string s = "Hello, world!";
  EXPECT_CALL(mock_interface_, StringMethod(s));
  s_remote->StringMethod(s);
  task_environment_.RunUntilIdle();
}

TEST_F(BinderFactoryTest, BinderFactoryLifetime) {
  MockErrorHandler s_handler;
  auto s_factory =
      std::make_unique<BinderFactory<test::mojom::StringInterface>>(
          &mock_interface_);

  auto binder = s_factory->GetBinder();
  ASSERT_TRUE(!binder.is_null());

  mojo::Remote<test::mojom::StringInterface> s_remote;
  s_factory->Bind(s_remote.BindNewPipeAndPassReceiver());

  s_remote.set_disconnect_handler(
      base::BindOnce(&MockErrorHandler::OnError, base::Unretained(&s_handler)));

  // Remotes which were bound via the BinderFactory will invalidated when the
  // factory is destroyed.
  EXPECT_CALL(s_handler, OnError()).Times(1);

  s_factory.reset();
  task_environment_.RunUntilIdle();
}

TEST_F(BinderFactoryTest, MultiBinderFactory) {
  MockErrorHandler s_handler;
  MockErrorHandler i_handler;
  auto factory = std::make_unique<MultiBinderFactory>();
  factory->AddInterface<test::mojom::StringInterface>(&mock_interface_);
  factory->AddInterface<test::mojom::IntInterface>(&mock_interface_);

  auto s_binder = factory->GetBinder<test::mojom::StringInterface>();
  ASSERT_FALSE(s_binder.is_null());
  auto i_binder = factory->GetBinder<test::mojom::IntInterface>();
  ASSERT_FALSE(i_binder.is_null());

  // There is no binder for BoolInterface.
  ASSERT_TRUE(factory->GetBinder<test::mojom::BoolInterface>().is_null());

  mojo::Remote<test::mojom::StringInterface> s_remote;
  s_binder.Run(s_remote.BindNewPipeAndPassReceiver());

  mojo::Remote<test::mojom::IntInterface> i_remote;
  i_binder.Run(i_remote.BindNewPipeAndPassReceiver());

  std::string s = "Hello, world!";
  EXPECT_CALL(mock_interface_, StringMethod(s));
  s_remote->StringMethod(s);
  task_environment_.RunUntilIdle();

  int i = 1234;
  EXPECT_CALL(mock_interface_, IntMethod(i));
  i_remote->IntMethod(i);
  task_environment_.RunUntilIdle();

  s_remote.set_disconnect_handler(
      base::BindOnce(&MockErrorHandler::OnError, base::Unretained(&s_handler)));
  i_remote.set_disconnect_handler(
      base::BindOnce(&MockErrorHandler::OnError, base::Unretained(&i_handler)));

  // Pointers which were bound via the MultiBinderFactory will invalidated when
  // the factory is destroyed.
  EXPECT_CALL(s_handler, OnError()).Times(1);
  EXPECT_CALL(i_handler, OnError()).Times(1);

  factory.reset();
  task_environment_.RunUntilIdle();
}

}  // namespace chromecast
