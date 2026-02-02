// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dbus/xdg/file_transfer_portal.h"

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/threading/thread.h"
#include "components/dbus/utils/read_message.h"
#include "components/dbus/utils/types.h"
#include "components/dbus/utils/write_value.h"
#include "dbus/message.h"
#include "dbus/mock_bus.h"
#include "dbus/mock_object_proxy.h"
#include "dbus/object_path.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Return;

namespace dbus_xdg {

namespace {

constexpr char kPortalServiceName[] = "org.freedesktop.portal.Documents";
constexpr char kPortalObjectPath[] = "/org/freedesktop/portal/documents";
constexpr char kFileTransferInterfaceName[] =
    "org.freedesktop.portal.FileTransfer";

constexpr char kMethodStartTransfer[] = "StartTransfer";
constexpr char kMethodAddFiles[] = "AddFiles";
constexpr char kMethodRetrieveFiles[] = "RetrieveFiles";
constexpr char kMethodStopTransfer[] = "StopTransfer";

}  // namespace

class FileTransferPortalTest : public testing::Test {
 public:
  FileTransferPortalTest() : dbus_thread_("DBusTestThread") {
    feature_list_.InitAndEnableFeature(kXdgFileTransferPortal);
  }

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    // Start a separate thread for DBus operations so that the blocking
    // calls from the main thread don't deadlock.
    base::Thread::Options thread_options;
    thread_options.message_pump_type = base::MessagePumpType::IO;
    ASSERT_TRUE(dbus_thread_.StartWithOptions(std::move(thread_options)));

    dbus::Bus::Options options;
    options.bus_type = dbus::Bus::SESSION;
    mock_bus_ = base::MakeRefCounted<dbus::MockBus>(std::move(options));

    mock_portal_proxy_ = base::MakeRefCounted<dbus::MockObjectProxy>(
        mock_bus_.get(), kPortalServiceName,
        dbus::ObjectPath(kPortalObjectPath));

    EXPECT_CALL(*mock_bus_, GetObjectProxy(kPortalServiceName,
                                           dbus::ObjectPath(kPortalObjectPath)))
        .WillRepeatedly(Return(mock_portal_proxy_.get()));

    EXPECT_CALL(*mock_bus_, GetDBusTaskRunner())
        .WillRepeatedly(Return(dbus_thread_.task_runner().get()));
  }

  void TearDown() override { dbus_thread_.Stop(); }

 protected:
  std::string CreateTempFile(std::string_view name) {
    base::FilePath path = temp_dir_.GetPath().AppendASCII(name);
    base::WriteFile(path, "test");
    return path.value();
  }

  base::test::TaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list_;
  base::Thread dbus_thread_;
  base::ScopedTempDir temp_dir_;
  scoped_refptr<dbus::MockBus> mock_bus_;
  scoped_refptr<dbus::MockObjectProxy> mock_portal_proxy_;
};

TEST_F(FileTransferPortalTest, IsAvailableFeatureDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(kXdgFileTransferPortal);

  // Should return false immediately without checking the bus.
  EXPECT_FALSE(FileTransferPortal::IsAvailableSync(mock_bus_.get()));
}

TEST_F(FileTransferPortalTest, IsAvailable) {
  auto mock_bus_proxy = base::MakeRefCounted<dbus::MockObjectProxy>(
      mock_bus_.get(), "org.freedesktop.DBus",
      dbus::ObjectPath("/org/freedesktop/DBus"));

  EXPECT_CALL(*mock_bus_,
              GetObjectProxy("org.freedesktop.DBus",
                             dbus::ObjectPath("/org/freedesktop/DBus")))
      .WillRepeatedly(Return(mock_bus_proxy.get()));

  EXPECT_CALL(*mock_bus_proxy, CallMethodAndBlock(_, _))
      .WillOnce(
          [](dbus::MethodCall* method_call, int timeout_ms)
              -> base::expected<std::unique_ptr<dbus::Response>, dbus::Error> {
            EXPECT_EQ(method_call->GetInterface(), "org.freedesktop.DBus");
            EXPECT_EQ(method_call->GetMember(), "GetNameOwner");

            auto response = dbus::Response::CreateEmpty();
            dbus::MessageWriter writer(response.get());
            writer.AppendString("unique-name");
            return base::ok(std::move(response));
          });

  EXPECT_TRUE(FileTransferPortal::IsAvailableSync(mock_bus_.get()));

  // Second call should use cached value and not call CallMethodAndBlock.
  EXPECT_TRUE(FileTransferPortal::IsAvailableSync(mock_bus_.get()));
}

TEST_F(FileTransferPortalTest, RetrieveFilesSuccess) {
  const std::string kFakeKey = "fake-transfer-key";
  const std::string kExpectedPath1 = "/fake/path/1";
  const std::string kExpectedPath2 = "/fake/path/2";

  EXPECT_CALL(*mock_portal_proxy_, CallMethodAndBlock(_, _))
      .WillOnce([&](dbus::MethodCall* method_call, int timeout_ms)
                    -> base::expected<std::unique_ptr<dbus::Response>,
                                      dbus::Error> {
        EXPECT_EQ(method_call->GetInterface(), kFileTransferInterfaceName);
        EXPECT_EQ(method_call->GetMember(), kMethodRetrieveFiles);

        using ArgsTuple =
            std::tuple<std::string, std::map<std::string, dbus_utils::Variant>>;
        auto args = dbus_utils::internal::ReadMessage<ArgsTuple>(*method_call);
        EXPECT_TRUE(args.has_value());
        EXPECT_EQ(std::get<0>(*args), kFakeKey);

        auto response = dbus::Response::CreateEmpty();
        dbus::MessageWriter writer(response.get());
        std::vector<std::string> paths = {kExpectedPath1, kExpectedPath2};
        dbus_utils::WriteValue(writer, paths);

        return base::ok(std::move(response));
      });

  std::vector<std::string> paths =
      FileTransferPortal::RetrieveFilesSync(kFakeKey, mock_bus_.get());

  ASSERT_EQ(paths.size(), 2u);
  EXPECT_EQ(paths[0], kExpectedPath1);
  EXPECT_EQ(paths[1], kExpectedPath2);
}

TEST_F(FileTransferPortalTest, RetrieveFilesFailure) {
  EXPECT_CALL(*mock_portal_proxy_, CallMethodAndBlock(_, _))
      .WillOnce(Return(base::unexpected(dbus::Error())));

  std::vector<std::string> paths = FileTransferPortal::RetrieveFilesSync(
      "fake-transfer-key", mock_bus_.get());

  EXPECT_TRUE(paths.empty());
}

TEST_F(FileTransferPortalTest, RegisterFilesSuccess) {
  const std::string kFakeKey = "fake-transfer-key";
  std::vector<std::string> files = {CreateTempFile("file1.txt"),
                                    CreateTempFile("file2.txt")};

  EXPECT_CALL(*mock_portal_proxy_, CallMethodAndBlock(_, _))
      .WillOnce(
          [&](dbus::MethodCall* method_call, int timeout_ms)
              -> base::expected<std::unique_ptr<dbus::Response>, dbus::Error> {
            EXPECT_EQ(method_call->GetInterface(), kFileTransferInterfaceName);
            EXPECT_EQ(method_call->GetMember(), kMethodStartTransfer);

            auto args = dbus_utils::internal::ReadMessage<
                std::tuple<std::map<std::string, dbus_utils::Variant>>>(
                *method_call);
            EXPECT_TRUE(args.has_value());
            auto options = std::move(std::get<0>(*args));

            auto it_writable = options.find("writable");
            EXPECT_TRUE(it_writable != options.end());
            EXPECT_EQ(std::move(it_writable->second).Take<bool>(), false);

            auto it_autostop = options.find("autostop");
            EXPECT_TRUE(it_autostop != options.end());
            EXPECT_EQ(std::move(it_autostop->second).Take<bool>(), true);

            auto response = dbus::Response::CreateEmpty();
            dbus::MessageWriter writer(response.get());
            dbus_utils::WriteValue(writer, kFakeKey);

            return base::ok(std::move(response));
          })
      .WillOnce(
          [&](dbus::MethodCall* method_call, int timeout_ms)
              -> base::expected<std::unique_ptr<dbus::Response>, dbus::Error> {
            EXPECT_EQ(method_call->GetInterface(), kFileTransferInterfaceName);
            EXPECT_EQ(method_call->GetMember(), kMethodAddFiles);

            using ArgsTuple =
                std::tuple<std::string, std::vector<base::ScopedFD>,
                           std::map<std::string, dbus_utils::Variant>>;
            auto args =
                dbus_utils::internal::ReadMessage<ArgsTuple>(*method_call);
            EXPECT_TRUE(args.has_value());

            const auto& [key, fds, options] = *args;
            EXPECT_EQ(key, kFakeKey);
            EXPECT_EQ(fds.size(), 2u);
            if (fds.size() != 2u) {
              return base::unexpected(dbus::Error());
            }
            EXPECT_TRUE(fds[0].is_valid());
            EXPECT_TRUE(fds[1].is_valid());
            EXPECT_TRUE(options.empty());

            auto response = dbus::Response::CreateEmpty();
            return base::ok(std::move(response));
          });

  std::string key =
      FileTransferPortal::RegisterFilesSync(files, mock_bus_.get());

  EXPECT_EQ(key, kFakeKey);
}

TEST_F(FileTransferPortalTest, RegisterFilesBatching) {
  const std::string kFakeKey = "fake-transfer-key";

  // Create 18 files, which should result in two AddFiles calls (16 + 2).
  std::vector<std::string> files;
  for (int i = 0; i < 18; ++i) {
    files.push_back(CreateTempFile("file" + base::NumberToString(i) + ".txt"));
  }

  // StartTransfer call
  EXPECT_CALL(*mock_portal_proxy_, CallMethodAndBlock(_, _))
      .WillOnce(
          [&](dbus::MethodCall* method_call, int timeout_ms)
              -> base::expected<std::unique_ptr<dbus::Response>, dbus::Error> {
            EXPECT_EQ(method_call->GetMember(), kMethodStartTransfer);
            auto response = dbus::Response::CreateEmpty();
            dbus::MessageWriter writer(response.get());
            dbus_utils::WriteValue(writer, kFakeKey);
            return base::ok(std::move(response));
          })
      // First batch (16 files)
      .WillOnce(
          [&](dbus::MethodCall* method_call, int timeout_ms)
              -> base::expected<std::unique_ptr<dbus::Response>, dbus::Error> {
            EXPECT_EQ(method_call->GetMember(), kMethodAddFiles);

            using ArgsTuple =
                std::tuple<std::string, std::vector<base::ScopedFD>,
                           std::map<std::string, dbus_utils::Variant>>;
            auto args =
                dbus_utils::internal::ReadMessage<ArgsTuple>(*method_call);
            EXPECT_TRUE(args.has_value());

            EXPECT_EQ(std::get<0>(*args), kFakeKey);
            EXPECT_EQ(std::get<1>(*args).size(), 16u);

            auto response = dbus::Response::CreateEmpty();
            return base::ok(std::move(response));
          })
      // Second batch (2 files)
      .WillOnce(
          [&](dbus::MethodCall* method_call, int timeout_ms)
              -> base::expected<std::unique_ptr<dbus::Response>, dbus::Error> {
            EXPECT_EQ(method_call->GetMember(), kMethodAddFiles);

            using ArgsTuple =
                std::tuple<std::string, std::vector<base::ScopedFD>,
                           std::map<std::string, dbus_utils::Variant>>;
            auto args =
                dbus_utils::internal::ReadMessage<ArgsTuple>(*method_call);
            EXPECT_TRUE(args.has_value());

            EXPECT_EQ(std::get<0>(*args), kFakeKey);
            EXPECT_EQ(std::get<1>(*args).size(), 2u);

            auto response = dbus::Response::CreateEmpty();
            return base::ok(std::move(response));
          });

  std::string key =
      FileTransferPortal::RegisterFilesSync(files, mock_bus_.get());

  EXPECT_EQ(key, kFakeKey);
}

TEST_F(FileTransferPortalTest, RegisterFilesAddFilesFailure) {
  const std::string kFakeKey = "fake-transfer-key";
  std::vector<std::string> files = {CreateTempFile("file1.txt")};

  // 1. StartTransfer - Success
  // 2. AddFiles - Failure
  EXPECT_CALL(*mock_portal_proxy_, CallMethodAndBlock(_, _))
      .WillOnce(
          [&](dbus::MethodCall* method_call, int timeout_ms)
              -> base::expected<std::unique_ptr<dbus::Response>, dbus::Error> {
            EXPECT_EQ(method_call->GetMember(), kMethodStartTransfer);
            auto response = dbus::Response::CreateEmpty();
            dbus::MessageWriter writer(response.get());
            dbus_utils::WriteValue(writer, kFakeKey);
            return base::ok(std::move(response));
          })
      .WillOnce(Return(base::unexpected(dbus::Error())));

  // 3. StopTransfer - Success (async)
  EXPECT_CALL(*mock_portal_proxy_, CallMethodWithErrorResponse(_, _, _))
      .WillOnce([&](dbus::MethodCall* method_call, int timeout_ms,
                    dbus::ObjectProxy::ResponseOrErrorCallback callback) {
        EXPECT_EQ(method_call->GetInterface(), kFileTransferInterfaceName);
        EXPECT_EQ(method_call->GetMember(), kMethodStopTransfer);

        using ArgsTuple = std::tuple<std::string>;
        auto args = dbus_utils::internal::ReadMessage<ArgsTuple>(*method_call);
        EXPECT_TRUE(args.has_value());
        EXPECT_EQ(std::get<0>(*args), kFakeKey);

        auto response = dbus::Response::CreateEmpty();
        std::move(callback).Run(response.get(), nullptr);
      });

  std::string key =
      FileTransferPortal::RegisterFilesSync(files, mock_bus_.get());

  EXPECT_TRUE(key.empty());
}

TEST_F(FileTransferPortalTest, RegisterFilesAllFilesOpenFailure) {
  const std::string kFakeKey = "fake-transfer-key";
  std::vector<std::string> files = {"/non/existent/file.txt"};

  // 1. StartTransfer - Success
  EXPECT_CALL(*mock_portal_proxy_, CallMethodAndBlock(_, _))
      .WillOnce(
          [&](dbus::MethodCall* method_call, int timeout_ms)
              -> base::expected<std::unique_ptr<dbus::Response>, dbus::Error> {
            EXPECT_EQ(method_call->GetMember(), kMethodStartTransfer);
            auto response = dbus::Response::CreateEmpty();
            dbus::MessageWriter writer(response.get());
            dbus_utils::WriteValue(writer, kFakeKey);
            return base::ok(std::move(response));
          });

  // 2. StopTransfer - Success (async) because no files could be opened
  EXPECT_CALL(*mock_portal_proxy_, CallMethodWithErrorResponse(_, _, _))
      .WillOnce([&](dbus::MethodCall* method_call, int timeout_ms,
                    dbus::ObjectProxy::ResponseOrErrorCallback callback) {
        EXPECT_EQ(method_call->GetMember(), kMethodStopTransfer);
        auto response = dbus::Response::CreateEmpty();
        std::move(callback).Run(response.get(), nullptr);
      });

  std::string key =
      FileTransferPortal::RegisterFilesSync(files, mock_bus_.get());

  EXPECT_TRUE(key.empty());
}

}  // namespace dbus_xdg
