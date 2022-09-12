// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/crash/fuchsia/cast_crash_storage_impl_fuchsia.h"

#include <fuchsia/feedback/cpp/fidl.h>
#include <fuchsia/feedback/cpp/fidl_test_base.h>
#include <lib/fidl/cpp/clone.h>
#include <lib/fidl/cpp/comparison.h>
#include <lib/sys/cpp/outgoing_directory.h>
#include <lib/sys/cpp/service_directory.h>

#include <memory>

#include "base/fuchsia/scoped_service_binding.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "chromecast/crash/fuchsia/constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromecast {
namespace {

class MockComponentDataRegister
    : public fuchsia::feedback::testing::ComponentDataRegister_TestBase {
 public:
  MockComponentDataRegister(
      fidl::InterfaceRequest<fuchsia::io::Directory> channel) {
    outgoing_directory_ = std::make_unique<sys::OutgoingDirectory>();
    outgoing_directory_->GetOrCreateDirectory("svc")->Serve(
        fuchsia::io::OpenFlags::RIGHT_READABLE |
            fuchsia::io::OpenFlags::RIGHT_WRITABLE,
        channel.TakeChannel());
    binding_ = std::make_unique<
        base::ScopedServiceBinding<fuchsia::feedback::ComponentDataRegister>>(
        outgoing_directory_.get(), this);
  }

  fuchsia::feedback::ComponentData GetLatest() {
    return fidl::Clone(component_data_);
  }

  void Upsert(fuchsia::feedback::ComponentData data,
              UpsertCallback callback) final {
    component_data_ = std::move(data);
    callback();
  }

  void NotImplemented_(const std::string& name) final { ADD_FAILURE(); }

 private:
  std::unique_ptr<sys::OutgoingDirectory> outgoing_directory_;
  std::unique_ptr<
      base::ScopedServiceBinding<fuchsia::feedback::ComponentDataRegister>>
      binding_;

  fuchsia::feedback::ComponentData component_data_;
};

class CastCrashStorageImplFuchsiaTest : public ::testing::Test {
 public:
  CastCrashStorageImplFuchsiaTest() {
    fidl::InterfaceHandle<fuchsia::io::Directory> directory;
    component_data_register_ =
        std::make_unique<MockComponentDataRegister>(directory.NewRequest());
    incoming_directory_ =
        std::make_unique<sys::ServiceDirectory>(std::move(directory));
  }

  void SetUp() final {
    cast_crash_storage_ = std::make_unique<CastCrashStorageImplFuchsia>(
        incoming_directory_.get());
  }

  void TearDown() final { cast_crash_storage_ = nullptr; }

  void VerifyLatestAnnotation(const fuchsia::feedback::Annotation& annotation) {
    fuchsia::feedback::ComponentData latest =
        component_data_register_->GetLatest();
    EXPECT_EQ(latest.namespace_(), crash::kCastNamespace);
    ASSERT_EQ(latest.annotations().size(), 1uL);

    fuchsia::feedback::Annotation latest_annotation =
        fidl::Clone(latest.annotations()[0]);
    EXPECT_EQ(annotation.key, latest_annotation.key);
    EXPECT_EQ(annotation.value, latest_annotation.value);
  }

 protected:
  std::unique_ptr<CastCrashStorageImplFuchsia> cast_crash_storage_;

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::IO};
  std::unique_ptr<MockComponentDataRegister> component_data_register_;
  std::unique_ptr<sys::ServiceDirectory> incoming_directory_;
};

TEST_F(CastCrashStorageImplFuchsiaTest, LastLaunchedApp) {
  fuchsia::feedback::Annotation annotation;
  annotation.key = "app.last-launched";
  annotation.value = "last_launched_app_id";

  cast_crash_storage_->SetLastLaunchedApp("last_launched_app_id");
  base::RunLoop().RunUntilIdle();
  VerifyLatestAnnotation(annotation);
}

TEST_F(CastCrashStorageImplFuchsiaTest, CurrentApp) {
  fuchsia::feedback::Annotation annotation;
  annotation.key = "app.current";
  annotation.value = "current_app_id";

  cast_crash_storage_->SetCurrentApp("current_app_id");
  base::RunLoop().RunUntilIdle();
  VerifyLatestAnnotation(annotation);
}

TEST_F(CastCrashStorageImplFuchsiaTest, PreviousApp) {
  fuchsia::feedback::Annotation annotation;
  annotation.key = "app.previous";
  annotation.value = "previous_app_id";

  cast_crash_storage_->SetPreviousApp("previous_app_id");
  base::RunLoop().RunUntilIdle();
  VerifyLatestAnnotation(annotation);
}

TEST_F(CastCrashStorageImplFuchsiaTest, StadiaSessionId) {
  fuchsia::feedback::Annotation annotation;
  annotation.key = "stadia-session-id";
  annotation.value = "session_id";

  cast_crash_storage_->SetStadiaSessionId("session_id");
  base::RunLoop().RunUntilIdle();
  VerifyLatestAnnotation(annotation);
}

}  // namespace
}  // namespace chromecast
