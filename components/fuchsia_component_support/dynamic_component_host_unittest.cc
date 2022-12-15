// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/fuchsia_component_support/dynamic_component_host.h"

#include <fuchsia/component/cpp/fidl_test_base.h>
#include <lib/sys/cpp/service_directory.h>
#include <lib/vfs/cpp/pseudo_dir.h>
#include <lib/vfs/cpp/service.h>

#include <memory>
#include <utility>

#include "base/fuchsia/scoped_service_binding.h"
#include "base/fuchsia/test_component_context_for_process.h"
#include "base/test/bind.h"
#include "base/test/gtest_util.h"
#include "base/test/task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace fuchsia_component_support {

namespace {

using testing::_;

MATCHER_P(EqCollectionRef, name, "") {
  return arg.name == name;
}

MATCHER_P2(EqChildDecl, name, url, "") {
  return arg.has_name() && arg.name() == name && arg.has_url() &&
         arg.url() == url;
}

MATCHER_P2(EqChildRef, name, collection, "") {
  return arg.name == name && arg.collection == collection;
}

constexpr char kTestCollection[] = "test_collection";
constexpr char kTestChildId[] = "test-child-id";
constexpr char kTestComponentUrl[] = "dummy:url";

class MockRealm : public fuchsia::component::testing::Realm_TestBase {
 public:
  MockRealm(sys::OutgoingDirectory* outgoing) : binding_(outgoing, this) {}

  MOCK_METHOD(void,
              CreateChild,
              (fuchsia::component::decl::CollectionRef collection,
               fuchsia::component::decl::Child decl,
               fuchsia::component::CreateChildArgs args,
               fuchsia::component::Realm::CreateChildCallback callback));
  MOCK_METHOD(void,
              OpenExposedDir,
              (fuchsia::component::decl::ChildRef child,
               fidl::InterfaceRequest<fuchsia::io::Directory> exposed_dir,
               fuchsia::component::Realm::OpenExposedDirCallback callback));
  MOCK_METHOD(void,
              DestroyChild,
              (fuchsia::component::decl::ChildRef child,
               fuchsia::component::Realm::DestroyChildCallback callback));

  void NotImplemented_(const std::string& name) override {
    ADD_FAILURE() << "NotImplemented_: " << name;
  }

 protected:
  base::ScopedServiceBinding<fuchsia::component::Realm> binding_;
};

class DynamicComponentHostTest : public testing::Test {
 protected:
  DynamicComponentHostTest() : realm_(test_context_.additional_services()) {
    // By default simply reply indicating success, from Create/DestroyChild.
    ON_CALL(realm_, CreateChild)
        .WillByDefault(
            [](fuchsia::component::decl::CollectionRef,
               fuchsia::component::decl::Child,
               fuchsia::component::CreateChildArgs,
               fuchsia::component::Realm::CreateChildCallback callback) {
              callback({});
            });
    ON_CALL(realm_, DestroyChild)
        .WillByDefault(
            [](fuchsia::component::decl::ChildRef,
               fuchsia::component::Realm::DestroyChildCallback callback) {
              callback({});
            });

    // By default connect exposed directory requests to `exposed_`, to simplify
    // tests for exposed capabilities.
    ON_CALL(realm_, OpenExposedDir)
        .WillByDefault(
            [this](fuchsia::component::decl::ChildRef,
                   fidl::InterfaceRequest<fuchsia::io::Directory> exposed_dir,
                   fuchsia::component::Realm::OpenExposedDirCallback callback) {
              exposed_.Serve(fuchsia::io::OpenFlags::RIGHT_READABLE |
                                 fuchsia::io::OpenFlags::RIGHT_WRITABLE,
                             exposed_dir.TakeChannel());
              callback({});
            });
  }

  // Sets expectations on CreateChild(), OpenExposedDir() and DestroyChild()
  // being called, in that order, without expecting particular parameters.
  void ExpectCreateOpenAndDestroy() {
    testing::InSequence s;
    EXPECT_CALL(realm_, CreateChild(_, _, _, _));
    EXPECT_CALL(realm_, OpenExposedDir(_, _, _));
    EXPECT_CALL(realm_, DestroyChild(_, _));
  }

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::IO};

  base::TestComponentContextForProcess test_context_;

  testing::StrictMock<MockRealm> realm_;

  // Used to fake the "exposed dir" of the component.
  vfs::PseudoDir exposed_;
};

TEST_F(DynamicComponentHostTest, Basic) {
  ExpectCreateOpenAndDestroy();

  // Create and then immediately teardown the component.
  {
    DynamicComponentHost component(kTestCollection, kTestChildId,
                                   kTestComponentUrl, base::DoNothing(),
                                   nullptr);
  }

  // Spin the loop to allow the calls to reach `realm_`.
  base::RunLoop().RunUntilIdle();
}

TEST_F(DynamicComponentHostTest, CollectionAndChildName) {
  {
    testing::InSequence s;
    EXPECT_CALL(
        realm_,
        CreateChild(EqCollectionRef(kTestCollection),
                    EqChildDecl(kTestChildId, kTestComponentUrl), _, _));
    EXPECT_CALL(realm_, OpenExposedDir(
                            EqChildRef(kTestChildId, kTestCollection), _, _));
    EXPECT_CALL(realm_,
                DestroyChild(EqChildRef(kTestChildId, kTestCollection), _));
  }

  {
    DynamicComponentHost component(kTestCollection, kTestChildId,
                                   kTestComponentUrl, base::DoNothing(),
                                   nullptr);
  }

  // Spin the loop to allow the calls to reach `realm_`.
  base::RunLoop().RunUntilIdle();
}

TEST_F(DynamicComponentHostTest, OnTeardownCalledOnBinderClose) {
  ExpectCreateOpenAndDestroy();

  // Publish a fake Binder to the `exposed_` directory.
  fidl::InterfaceRequest<fuchsia::component::Binder> binder_request;
  exposed_.AddEntry(
      fuchsia::component::Binder::Name_,
      std::make_unique<vfs::Service>(
          [&binder_request](zx::channel request, async_dispatcher_t*) {
            binder_request = fidl::InterfaceRequest<fuchsia::component::Binder>(
                std::move(request));
          }));

  {
    DynamicComponentHost component(
        kTestCollection, kTestChildId, kTestComponentUrl,
        base::MakeExpectedRunClosure(FROM_HERE), nullptr);

    // Spin the loop to process calls to `realm_` and `exposed_`.
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(binder_request);

    // Close `binder_request` and spin the loop to allow that to be observed.
    binder_request = nullptr;
    base::RunLoop().RunUntilIdle();
  }

  // Spin the loop to allow remaining calls to reach `realm_`.
  base::RunLoop().RunUntilIdle();
}

TEST_F(DynamicComponentHostTest,
       OnTeardownNotCalledIfDestroyedBeforeBinderClose) {
  ExpectCreateOpenAndDestroy();

  // Create and immediately teardown the component, so that Binder teardown
  // will not be observed until after the DynamicComponentHost has gone.
  {
    DynamicComponentHost component(
        kTestCollection, kTestChildId, kTestComponentUrl,
        base::MakeExpectedNotRunClosure(FROM_HERE), nullptr);
  }

  // Spin the loop to allow remaining calls to reach `realm_`.
  base::RunLoop().RunUntilIdle();
}

}  // namespace

}  // namespace fuchsia_component_support
