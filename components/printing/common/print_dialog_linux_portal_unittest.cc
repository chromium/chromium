// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/printing/common/print_dialog_linux_portal.h"

#include <memory>
#include <string>
#include <vector>

#include "base/files/file.h"
#include "base/files/file_descriptor_watcher_posix.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/scoped_refptr.h"
#include "base/posix/eintr_wrapper.h"
#include "base/run_loop.h"
#include "base/test/run_until.h"
#include "base/test/task_environment.h"
#include "components/dbus/utils/variant.h"
#include "components/dbus/utils/write_value.h"
#include "components/dbus/xdg/portal.h"
#include "dbus/mock_bus.h"
#include "dbus/mock_object_proxy.h"
#include "printing/metafile.h"
#include "printing/printing_context_linux.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/resource/resource_bundle.h"

using ::testing::_;
using ::testing::Invoke;
using ::testing::Return;

namespace printing {

namespace {

class MockPrintingContextDelegate : public PrintingContext::Delegate {
 public:
  MOCK_METHOD(gfx::NativeView, GetParentView, (), (override));
  MOCK_METHOD(std::string, GetAppLocale, (), (override));
};

class MockMetafile : public MetafilePlayer {
 public:
  MOCK_CONST_METHOD1(GetDataAsVector, bool(std::vector<char>* buffer));
  MOCK_CONST_METHOD0(GetDataAsSharedMemoryRegion, base::MappedReadOnlyRegion());
  MOCK_CONST_METHOD0(ShouldCopySharedMemoryRegionData, bool());
  MOCK_CONST_METHOD0(GetDataType, mojom::MetafileDataType());
  MOCK_CONST_METHOD1(SaveTo, bool(base::File* file));
};

}  // namespace

class PrintDialogLinuxPortalTest : public testing::Test {
 public:
  void SetUp() override {
    if (!ui::ResourceBundle::HasSharedInstance()) {
      ui::ResourceBundle::InitSharedInstanceWithLocale(
          "en-US", nullptr, ui::ResourceBundle::DO_NOT_LOAD_COMMON_RESOURCES);
      resource_bundle_initialized_ = true;
    }

    mock_bus_ = base::MakeRefCounted<dbus::MockBus>(dbus::Bus::Options());
    mock_proxy_ = base::MakeRefCounted<dbus::MockObjectProxy>(
        mock_bus_.get(), "org.freedesktop.portal.Desktop",
        dbus::ObjectPath("/org/freedesktop/portal/desktop"));

    EXPECT_CALL(*mock_bus_, GetConnectionName())
        .WillRepeatedly(Return(":1.23"));

    EXPECT_CALL(*mock_bus_, IsConnected()).WillRepeatedly(Return(true));

    EXPECT_CALL(*mock_bus_, GetObjectProxy(_, _))
        .WillRepeatedly(Return(mock_proxy_.get()));

    EXPECT_CALL(*mock_bus_, AssertOnOriginThread()).WillRepeatedly([] {});
    EXPECT_CALL(*mock_bus_, GetDBusTaskRunner())
        .WillRepeatedly(
            Return(task_environment_.GetMainThreadTaskRunner().get()));
    EXPECT_CALL(*mock_bus_, GetOriginTaskRunner())
        .WillRepeatedly(
            Return(task_environment_.GetMainThreadTaskRunner().get()));

    delegate_ = std::make_unique<MockPrintingContextDelegate>();
    context_ = std::make_unique<PrintingContextLinux>(
        delegate_.get(), PrintingContext::OutOfProcessBehavior::kDisabled);
  }

  void TearDown() override {
    dialog_ = nullptr;
    context_.reset();
    dbus_xdg::SetPortalStateForTesting(dbus_xdg::PortalRegistrarState::kIdle);
    if (resource_bundle_initialized_) {
      ui::ResourceBundle::CleanupSharedInstance();
    }
  }

 protected:
  bool resource_bundle_initialized_ = false;
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::IO};
  scoped_refptr<dbus::MockBus> mock_bus_;
  scoped_refptr<dbus::MockObjectProxy> mock_proxy_;
  std::unique_ptr<MockPrintingContextDelegate> delegate_;
  std::unique_ptr<PrintingContextLinux> context_;
  std::unique_ptr<PrintDialogLinuxPortal> dialog_;
};

TEST_F(PrintDialogLinuxPortalTest, ShowDialog_PortalAvailable) {
  dbus_xdg::SetPortalStateForTesting(dbus_xdg::PortalRegistrarState::kSuccess);

  dialog_ = std::make_unique<PrintDialogLinuxPortal>(context_.get(), mock_bus_);

  dbus::ObjectProxy::ResponseOrErrorCallback prepare_print_callback;
  dbus::ObjectProxy::SignalCallback signal_callback_captured;

  // Expect PreparePrint call
  EXPECT_CALL(*mock_proxy_,
              CallMethodWithErrorResponse(
                  testing::ResultOf(
                      [](dbus::MethodCall* call) { return call->GetMember(); },
                      "PreparePrint"),
                  _, _))
      .WillOnce([&prepare_print_callback](
                    dbus::MethodCall* method_call, int timeout_ms,
                    dbus::ObjectProxy::ResponseOrErrorCallback callback) {
        prepare_print_callback = std::move(callback);
      });

  // Expect Request Response signal connection
  EXPECT_CALL(*mock_proxy_, ConnectToSignal("org.freedesktop.portal.Request",
                                            "Response", _, _))
      .WillRepeatedly(
          [&signal_callback_captured](
              const std::string& interface_name, const std::string& signal_name,
              dbus::ObjectProxy::SignalCallback signal_callback,
              dbus::ObjectProxy::OnConnectedCallback on_connected_callback) {
            signal_callback_captured = signal_callback;
            std::move(on_connected_callback)
                .Run(interface_name, signal_name, true);
          });

  base::RunLoop run_loop;
  dialog_->ShowDialog(
      nullptr, false,
      base::BindOnce(
          [](base::RunLoop* run_loop, mojom::ResultCode result) {
            EXPECT_EQ(result, mojom::ResultCode::kSuccess);
            run_loop->Quit();
          },
          &run_loop));

  ASSERT_TRUE(prepare_print_callback);
  auto response = dbus::Response::CreateEmpty();
  dbus::MessageWriter writer(response.get());
  writer.AppendObjectPath(
      dbus::ObjectPath("/org/freedesktop/portal/request/1"));
  std::move(prepare_print_callback).Run(response.get(), nullptr);

  // Now OnMethodResponse should have run, updated path, and connected signal
  // again.
  ASSERT_TRUE(signal_callback_captured);

  // Send success signal
  dbus::Signal signal("org.freedesktop.portal.Request", "Response");
  dbus::MessageWriter signal_writer(&signal);
  signal_writer.AppendUint32(0);  // Response success
  dbus_xdg::Dictionary results;
  results["token"] = dbus_utils::Variant::Wrap<"u">(12345);
  dbus_utils::WriteValue(signal_writer, results);
  signal_callback_captured.Run(&signal);

  run_loop.Run();
}

TEST_F(PrintDialogLinuxPortalTest, ShowDialog_PortalCancelled) {
  dbus_xdg::SetPortalStateForTesting(dbus_xdg::PortalRegistrarState::kSuccess);

  dialog_ = std::make_unique<PrintDialogLinuxPortal>(context_.get(), mock_bus_);

  dbus::ObjectProxy::ResponseOrErrorCallback prepare_print_callback;
  dbus::ObjectProxy::SignalCallback signal_callback_captured;

  // Expect PreparePrint call
  EXPECT_CALL(*mock_proxy_, CallMethodWithErrorResponse(_, _, _))
      .WillOnce([&prepare_print_callback](
                    dbus::MethodCall* method_call, int timeout_ms,
                    dbus::ObjectProxy::ResponseOrErrorCallback callback) {
        prepare_print_callback = std::move(callback);
      });

  // Expect Response signal - cancellation (1)
  EXPECT_CALL(*mock_proxy_, ConnectToSignal(_, _, _, _))
      .WillRepeatedly(
          [&signal_callback_captured](
              const std::string& interface_name, const std::string& signal_name,
              dbus::ObjectProxy::SignalCallback signal_callback,
              dbus::ObjectProxy::OnConnectedCallback on_connected_callback) {
            signal_callback_captured = signal_callback;
            std::move(on_connected_callback)
                .Run(interface_name, signal_name, true);
          });

  base::RunLoop run_loop;
  dialog_->ShowDialog(
      nullptr, false,
      base::BindOnce(
          [](base::RunLoop* run_loop, mojom::ResultCode result) {
            EXPECT_EQ(result, mojom::ResultCode::kCanceled);
            run_loop->Quit();
          },
          &run_loop));

  ASSERT_TRUE(prepare_print_callback);
  auto response = dbus::Response::CreateEmpty();
  dbus::MessageWriter writer(response.get());
  writer.AppendObjectPath(
      dbus::ObjectPath("/org/freedesktop/portal/request/2"));
  std::move(prepare_print_callback).Run(response.get(), nullptr);

  ASSERT_TRUE(signal_callback_captured);
  dbus::Signal signal("org.freedesktop.portal.Request", "Response");
  dbus::MessageWriter signal_writer(&signal);
  signal_writer.AppendUint32(1);  // Cancelled
  dbus_utils::WriteValue(signal_writer, dbus_xdg::Dictionary());
  signal_callback_captured.Run(&signal);

  run_loop.Run();
}

TEST_F(PrintDialogLinuxPortalTest, PrintDocument_WriteToPipe) {
  dialog_ = std::make_unique<PrintDialogLinuxPortal>(context_.get(), mock_bus_);

  MockMetafile metafile;
  std::string_view kData = "test data";
  EXPECT_CALL(metafile, GetDataAsVector(_))
      .WillOnce([&kData](std::vector<char>* buffer) {
        buffer->assign(kData.begin(), kData.end());
        return true;
      });

  base::ScopedFD read_fd;  // To be captured from DBus call

  // Expect Print call
  EXPECT_CALL(*mock_proxy_,
              CallMethodWithErrorResponse(
                  testing::ResultOf(
                      [](dbus::MethodCall* call) { return call->GetMember(); },
                      "Print"),
                  _, _))
      .WillOnce(
          [&read_fd](dbus::MethodCall* method_call, int timeout_ms,
                     dbus::ObjectProxy::ResponseOrErrorCallback callback) {
            // Retrieve FD from message
            dbus::MessageReader reader(method_call);
            std::string parent_handle;
            reader.PopString(&parent_handle);
            std::string title;
            reader.PopString(&title);
            reader.PopFileDescriptor(&read_fd);
          });

  dialog_->PrintDocument(metafile, u"title");

  // Wait for the Print DBus call to populate read_fd.
  EXPECT_TRUE(base::test::RunUntil([&]() { return read_fd.is_valid(); }));
  ASSERT_TRUE(read_fd.is_valid());

  // Wait for data to be available in the pipe.
  base::RunLoop read_loop;
  auto controller = base::FileDescriptorWatcher::WatchReadable(
      read_fd.get(), read_loop.QuitClosure());
  read_loop.Run();

  // Read from read_fd
  char buffer[256];
  ssize_t bytes_read =
      HANDLE_EINTR(read(read_fd.get(), buffer, sizeof(buffer)));
  ASSERT_GT(bytes_read, 0);
  std::string_view read_data(buffer, bytes_read);
  EXPECT_EQ(read_data, kData);
}

}  // namespace printing
