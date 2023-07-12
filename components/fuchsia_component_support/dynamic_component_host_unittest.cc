// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/fuchsia_component_support/dynamic_component_host.h"

#include <fuchsia/component/cpp/fidl_test_base.h>
#include <lib/sys/cpp/component_context.h>
#include <lib/sys/cpp/service_directory.h>
#include <lib/vfs/cpp/pseudo_dir.h>
#include <lib/vfs/cpp/service.h>

#include <memory>
#include <utility>

#include "base/fuchsia/process_context.h"
#include "base/fuchsia/scoped_service_binding.h"
#include "base/fuchsia/test_component_context_for_process.h"
#include "base/test/bind.h"
#include "base/test/gtest_util.h"
#include "base/test/task_environment.h"
#include "components/fuchsia_component_support/mock_realm.h"
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

// Verifies that `create_child_args` includes a dynamic offer for "/svc", and
// returns a channel connected to it, if so.
fidl::InterfaceHandle<fuchsia::io::Directory> GetSvcFromChildArgs(
    fuchsia::component::CreateChildArgs& create_child_args) {
  if (!create_child_args.has_dynamic_offers()) {
    return nullptr;
  }

  for (auto& offer : create_child_args.dynamic_offers()) {
    if (!offer.is_directory()) {
      continue;
    }

    const auto& directory_offer = offer.directory();
    if (!directory_offer.has_source_name() ||
        !directory_offer.has_target_name()) {
      return nullptr;
    }
    if (directory_offer.target_name() != "svc") {
      continue;
    }

    // Connect to the outgoing directory root, to use to look up the service
    // capability.
    fidl::InterfacePtr<fuchsia::io::Directory> root_dir;
    base::ComponentContextForProcess()->outgoing()->root_dir()->Serve(
        fuchsia::io::OpenFlags::RIGHT_READABLE |
            fuchsia::io::OpenFlags::RIGHT_WRITABLE |
            fuchsia::io::OpenFlags::DIRECTORY,
        root_dir.NewRequest().TakeChannel());

    // Determine the capability path, relative to the outgoing directory of
    // the calling process, and request to open it.
    // The channel will be closed as soon as the Open() call is processed,
    // if the path cannot be resolved.
    base::FilePath path(directory_offer.source_name());
    if (directory_offer.has_subdir()) {
      path = path.Append(directory_offer.subdir());
    }
    fidl::InterfaceHandle<fuchsia::io::Node> services_handle;
    root_dir->Open(fuchsia::io::OpenFlags::RIGHT_READABLE |
                       fuchsia::io::OpenFlags::RIGHT_WRITABLE |
                       fuchsia::io::OpenFlags::DIRECTORY,
                   {}, path.value(), services_handle.NewRequest());
    return fidl::InterfaceHandle<fuchsia::io::Directory>(
        services_handle.TakeChannel());
  }

  return nullptr;
}

bool HasPeerClosedHandle(
    const fidl::InterfaceHandle<fuchsia::io::Directory>& handle) {
  return handle.channel().wait_one(ZX_CHANNEL_PEER_CLOSED, zx::time(),
                                   nullptr) != ZX_ERR_TIMED_OUT;
}

constexpr char kTestCollection[] = "test_collection";
constexpr char kTestChildId[] = "test-child-id";
constexpr char kTestComponentUrl[] = "dummy:url";

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

TEST_F(DynamicComponentHostTest, WithoutServiceDirectory) {
  // Capture the `CreateChildArgs` from the `Realm.CreateChild()` call.
  fuchsia::component::CreateChildArgs create_child_args;
  {
    testing::InSequence s;
    EXPECT_CALL(realm_, CreateChild(_, _, _, _))
        .WillOnce([&create_child_args](
                      fuchsia::component::decl::CollectionRef,
                      fuchsia::component::decl::Child,
                      fuchsia::component::CreateChildArgs args,
                      fuchsia::component::Realm::CreateChildCallback callback) {
          create_child_args = std::move(args);
          callback({});
        });
    EXPECT_CALL(realm_, OpenExposedDir(_, _, _));
    EXPECT_CALL(realm_, DestroyChild(_, _));
  }

  {
    DynamicComponentHost component(kTestCollection, kTestChildId,
                                   kTestComponentUrl, base::DoNothing(),
                                   nullptr);

    // Spin the event loop to process the `CreateChild()` call.
    base::RunLoop().RunUntilIdle();

    // Verify that no "svc" directory is offered in the `CreateChildArgs`.
    fidl::InterfaceHandle<fuchsia::io::Directory> svc_handle =
        GetSvcFromChildArgs(create_child_args);
    EXPECT_FALSE(svc_handle);
  }

  // Spin the loop to allow the teardown calls to reach `realm_`.
  base::RunLoop().RunUntilIdle();
}

TEST_F(DynamicComponentHostTest, WithServiceDirectory) {
  // Capture the `CreateChildArgs` from the `Realm.CreateChild()` call.
  fuchsia::component::CreateChildArgs create_child_args;
  {
    testing::InSequence s;
    EXPECT_CALL(realm_, CreateChild(_, _, _, _))
        .WillOnce([&create_child_args](
                      fuchsia::component::decl::CollectionRef,
                      fuchsia::component::decl::Child,
                      fuchsia::component::CreateChildArgs args,
                      fuchsia::component::Realm::CreateChildCallback callback) {
          create_child_args = std::move(args);
          callback({});
        });
    EXPECT_CALL(realm_, OpenExposedDir(_, _, _));
    EXPECT_CALL(realm_, DestroyChild(_, _));
  }

  {
    // Create a directory handle for the service directory.
    fidl::InterfaceHandle<fuchsia::io::Directory> handle;
    vfs::PseudoDir service_directory;
    service_directory.Serve(fuchsia::io::OpenFlags::RIGHT_READABLE |
                                fuchsia::io::OpenFlags::RIGHT_WRITABLE,
                            handle.NewRequest().TakeChannel());

    DynamicComponentHost component(kTestCollection, kTestChildId,
                                   kTestComponentUrl, base::DoNothing(),
                                   std::move(handle));

    // Spin the event loop to process the `CreateChild()` call.
    base::RunLoop().RunUntilIdle();

    // Verify that a "svc" directory was offered in the `CreateChildArgs`.
    fidl::InterfaceHandle<fuchsia::io::Directory> svc_handle =
        GetSvcFromChildArgs(create_child_args);
    EXPECT_TRUE(svc_handle);

    // Spin the event loop to allow the Open() of the directory attempted by
    // GetSvcFromChildArgs() to be processed, then verify that the `svc_handle`
    // was not closed by the peer.
    base::RunLoop().RunUntilIdle();
    EXPECT_FALSE(HasPeerClosedHandle(svc_handle));
  }

  // Spin the loop to allow teardown calls to reach `realm_`.
  base::RunLoop().RunUntilIdle();

  // Verify that the "svc" directory offered in the `CreateChildArgs` is no
  // longer available after the `DynamicComponentHost` has been destroyed.
  fidl::InterfaceHandle<fuchsia::io::Directory> svc_handle =
      GetSvcFromChildArgs(create_child_args);
  EXPECT_TRUE(svc_handle);

  // Spin the loop to allow the Open() of the "svc" directory to be processed.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(HasPeerClosedHandle(svc_handle));
}

}  // namespace

}  // namespace fuchsia_component_support
