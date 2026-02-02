// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dbus/xdg/file_transfer_portal.h"

#include <fcntl.h>
#include <sys/types.h>

#include <map>

#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/scoped_file.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_runner.h"
#include "base/threading/thread_restrictions.h"
#include "components/dbus/thread_linux/dbus_thread_linux.h"
#include "components/dbus/utils/call_method.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"

namespace dbus_xdg {

BASE_FEATURE(kXdgFileTransferPortal, base::FEATURE_DISABLED_BY_DEFAULT);

namespace {

constexpr char kPortalServiceName[] = "org.freedesktop.portal.Documents";
constexpr char kPortalObjectPath[] = "/org/freedesktop/portal/documents";
constexpr char kFileTransferInterfaceName[] =
    "org.freedesktop.portal.FileTransfer";

constexpr char kMethodStartTransfer[] = "StartTransfer";
constexpr char kMethodAddFiles[] = "AddFiles";
constexpr char kMethodRetrieveFiles[] = "RetrieveFiles";
constexpr char kMethodStopTransfer[] = "StopTransfer";

void CallMethodSyncTask(dbus::ObjectProxy* proxy,
                        dbus::MethodCall* method_call,
                        std::unique_ptr<dbus::Response>* result) {
  base::expected<std::unique_ptr<dbus::Response>, dbus::Error> response =
      proxy->CallMethodAndBlock(method_call,
                                dbus::ObjectProxy::TIMEOUT_USE_DEFAULT);
  if (response.has_value()) {
    *result = std::move(response.value());
  } else {
    *result = nullptr;
  }
}

}  // namespace

// static
std::unique_ptr<dbus::Response> FileTransferPortal::CallMethodSyncImpl(
    dbus::Bus* bus,
    dbus::ObjectProxy* proxy,
    dbus::MethodCall* method_call) {
  if (bus->GetDBusTaskRunner()->RunsTasksInCurrentSequence()) {
    std::unique_ptr<dbus::Response> result;
    CallMethodSyncTask(proxy, method_call, &result);
    return result;
  }

  base::WaitableEvent on_complete;
  std::unique_ptr<dbus::Response> result;
  bus->GetDBusTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&CallMethodSyncTask, base::Unretained(proxy),
                     base::Unretained(method_call), base::Unretained(&result))
          // After the Callback is called, signal `on_complete` to unblock
          // this thread.
          .Then(base::BindOnce(&base::WaitableEvent::Signal,
                               base::Unretained(&on_complete))));

  // https://crbug.com/40398800
  base::ScopedAllowBaseSyncPrimitivesOutsideBlockingScope allow_wait;
  on_complete.Wait();
  return result;
}

// static
bool FileTransferPortal::IsAvailableSync(dbus::Bus* bus) {
  if (!base::FeatureList::IsEnabled(kXdgFileTransferPortal)) {
    return false;
  }

  if (!bus) {
    bus = dbus_thread_linux::GetSharedSessionBus().get();
  }

  if (!bus) {
    return false;
  }

  static bool is_available = false;
  static bool checked = false;
  if (checked) {
    return is_available;
  }

  dbus::ObjectProxy* bus_proxy = bus->GetObjectProxy(
      "org.freedesktop.DBus", dbus::ObjectPath("/org/freedesktop/DBus"));

  // GetNameOwner will return an error if the name does not exist.
  auto response =
      CallMethodSync<"s", "s">(bus, bus_proxy, "org.freedesktop.DBus",
                               "GetNameOwner", std::string(kPortalServiceName));

  is_available = response.has_value();
  checked = true;
  return is_available;
}

// static
std::vector<std::string> FileTransferPortal::RetrieveFilesSync(
    const std::string& raw_key,
    dbus::Bus* bus) {
  if (!bus) {
    bus = dbus_thread_linux::GetSharedSessionBus().get();
  }

  if (!bus) {
    return {};
  }

  // The portal key might be null-terminated.
  std::string key = raw_key;
  size_t null_pos = key.find('\0');
  if (null_pos != std::string::npos) {
    key.resize(null_pos);
  }

  dbus::ObjectProxy* proxy = bus->GetObjectProxy(
      kPortalServiceName, dbus::ObjectPath(kPortalObjectPath));
  if (!proxy) {
    return {};
  }

  std::map<std::string, dbus_utils::Variant> options;
  auto response =
      CallMethodSync<"sa{sv}", "as">(bus, proxy, kFileTransferInterfaceName,
                                     kMethodRetrieveFiles, key, options);

  if (!response.has_value()) {
    LOG(ERROR) << "Failed to call RetrieveFiles on FileTransfer portal.";
    return {};
  }

  return std::get<0>(response.value());
}

// static
std::string FileTransferPortal::RegisterFilesSync(
    const std::vector<std::string>& files,
    dbus::Bus* bus) {
  if (!bus) {
    bus = dbus_thread_linux::GetSharedSessionBus().get();
  }

  if (files.empty() || !bus) {
    return "";
  }

  dbus::ObjectProxy* proxy = bus->GetObjectProxy(
      kPortalServiceName, dbus::ObjectPath(kPortalObjectPath));
  if (!proxy) {
    return "";
  }

  // 1. StartTransfer
  std::string key;
  {
    std::map<std::string, dbus_utils::Variant> options;
    options.emplace("writable", dbus_utils::Variant::Wrap<"b">(false));
    options.emplace("autostop", dbus_utils::Variant::Wrap<"b">(true));

    auto response = CallMethodSync<"a{sv}", "s">(
        bus, proxy, kFileTransferInterfaceName, kMethodStartTransfer, options);

    if (!response.has_value()) {
      LOG(ERROR) << "Failed to StartTransfer.";
      return "";
    }

    key = std::get<0>(response.value());
  }

  // 2. AddFiles
  // We send files in batches of 16 to avoid exceeding the DBus file descriptor
  // limit per message.
  bool any_files_added = false;
  constexpr size_t kBatchSize = 16;
  base::span<const std::string> files_span(files);
  while (!files_span.empty()) {
    const size_t chunk_size = std::min(kBatchSize, files_span.size());
    base::span<const std::string> chunk = files_span.take_first(chunk_size);
    std::vector<base::ScopedFD> fds;
    fds.reserve(chunk.size());

    for (const auto& file : chunk) {
      int fd = HANDLE_EINTR(open(file.c_str(), O_PATH | O_CLOEXEC));
      if (fd < 0) {
        LOG(ERROR) << "Failed to open file for transfer: " << file;
      } else {
        fds.emplace_back(fd);
      }
    }

    if (fds.empty()) {
      continue;
    }

    std::map<std::string, dbus_utils::Variant> options;
    auto response =
        CallMethodSync<"saha{sv}", "">(bus, proxy, kFileTransferInterfaceName,
                                       kMethodAddFiles, key, fds, options);

    if (!response.has_value()) {
      LOG(ERROR) << "Failed to AddFiles to transfer.";
      dbus_utils::CallMethod<"s", "">(proxy, kFileTransferInterfaceName,
                                      kMethodStopTransfer, base::DoNothing(),
                                      key);
      return "";
    }
    any_files_added = true;
  }

  if (!any_files_added) {
    dbus_utils::CallMethod<"s", "">(proxy, kFileTransferInterfaceName,
                                    kMethodStopTransfer, base::DoNothing(),
                                    key);
    return "";
  }

  return key;
}

}  // namespace dbus_xdg
