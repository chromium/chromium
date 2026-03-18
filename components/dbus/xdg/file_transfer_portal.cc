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
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/no_destructor.h"
#include "base/posix/eintr_wrapper.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
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

class FileTransferSession {
 public:
  static void Start(const std::vector<std::string>& files,
                    base::OnceCallback<void(std::string)> callback,
                    dbus::ObjectProxy* proxy) {
    (new FileTransferSession(files, std::move(callback), proxy))->DoStart();
  }

  FileTransferSession(const std::vector<std::string>& files,
                      base::OnceCallback<void(std::string)> callback,
                      dbus::ObjectProxy* proxy)
      : files_(files), callback_(std::move(callback)), proxy_(proxy) {}

 private:
  ~FileTransferSession() = default;

  void DoStart() {
    std::map<std::string, dbus_utils::Variant> options;
    options.emplace("writable", dbus_utils::Variant::Wrap<"b">(false));
    options.emplace("autostop", dbus_utils::Variant::Wrap<"b">(true));

    dbus_utils::CallMethod<"a{sv}", "s">(
        proxy_, kFileTransferInterfaceName, kMethodStartTransfer,
        // base::Unretained(this) is safe because the session deletes itself
        // in `Finish()`.
        base::BindOnce(&FileTransferSession::OnStartResponse,
                       base::Unretained(this)),
        options);
  }

  void OnStartResponse(dbus_utils::CallMethodResultSig<"s"> response) {
    if (!response.has_value()) {
      LOG(ERROR) << "Failed to StartTransfer.";
      Finish("");
      return;
    }

    key_ = std::get<0>(response.value());
    AddNextChunk();
  }

  void AddNextChunk() {
    if (file_index_ >= files_.size()) {
      Finish(key_);
      return;
    }

    constexpr size_t kBatchSize = 16;
    const size_t chunk_size = std::min(kBatchSize, files_.size() - file_index_);
    std::vector<std::string> chunk;
    chunk.reserve(chunk_size);
    for (size_t i = 0; i < chunk_size; ++i) {
      chunk.push_back(files_[file_index_ + i]);
    }
    file_index_ += chunk_size;

    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
        base::BindOnce(&FileTransferSession::OpenFiles, std::move(chunk)),
        // base::Unretained(this) is safe because the session deletes itself
        // in `Finish()`.
        base::BindOnce(&FileTransferSession::OnFilesOpened,
                       base::Unretained(this)));
  }

  static std::vector<base::ScopedFD> OpenFiles(
      const std::vector<std::string>& files) {
    std::vector<base::ScopedFD> fds;
    fds.reserve(files.size());
    for (const auto& file : files) {
      int fd = HANDLE_EINTR(open(file.c_str(), O_PATH | O_CLOEXEC));
      if (fd < 0) {
        LOG(ERROR) << "Failed to open file for transfer: " << file;
      } else {
        fds.emplace_back(fd);
      }
    }
    return fds;
  }

  void OnFilesOpened(std::vector<base::ScopedFD> fds) {
    if (fds.empty()) {
      AddNextChunk();
      return;
    }

    std::map<std::string, dbus_utils::Variant> options;
    dbus_utils::CallMethod<"saha{sv}", "">(
        proxy_, kFileTransferInterfaceName, kMethodAddFiles,
        // base::Unretained(this) is safe because the session deletes itself
        // in `Finish()`.
        base::BindOnce(&FileTransferSession::OnAddFilesResponse,
                       base::Unretained(this)),
        key_, std::move(fds), options);
  }

  void OnAddFilesResponse(dbus_utils::CallMethodResultSig<""> response) {
    if (!response.has_value()) {
      LOG(ERROR) << "Failed to AddFiles to transfer.";
      StopTransferAndFinish();
      return;
    }

    any_files_added_ = true;
    AddNextChunk();
  }

  void StopTransferAndFinish() {
    dbus_utils::CallMethod<"s", "">(proxy_, kFileTransferInterfaceName,
                                    kMethodStopTransfer, base::DoNothing(),
                                    key_);
    Finish("");
  }

  void Finish(std::string key) {
    if (key.empty() || !any_files_added_) {
      if (!key.empty()) {
        dbus_utils::CallMethod<"s", "">(proxy_, kFileTransferInterfaceName,
                                        kMethodStopTransfer, base::DoNothing(),
                                        key);
      }
      std::move(callback_).Run("");
    } else {
      std::move(callback_).Run(key);
    }
    delete this;
  }

  const std::vector<std::string> files_;
  size_t file_index_ = 0;
  std::string key_;
  base::OnceCallback<void(std::string)> callback_;
  const raw_ptr<dbus::ObjectProxy> proxy_;
  bool any_files_added_ = false;
};

}  // namespace

// static
void FileTransferPortal::IsAvailable(base::OnceCallback<void(bool)> callback,
                                     dbus::Bus* bus) {
  if (!base::FeatureList::IsEnabled(kXdgFileTransferPortal)) {
    std::move(callback).Run(false);
    return;
  }

  if (!bus) {
    bus = dbus_thread_linux::GetSharedSessionBus().get();
  }

  if (!bus) {
    std::move(callback).Run(false);
    return;
  }

  static bool is_available = false;
  static bool checked = false;
  static base::NoDestructor<std::vector<base::OnceCallback<void(bool)>>>
      pending_callbacks;

  if (checked) {
    std::move(callback).Run(is_available);
    return;
  }

  pending_callbacks->push_back(std::move(callback));
  if (pending_callbacks->size() > 1) {
    return;
  }

  dbus::ObjectProxy* bus_proxy = bus->GetObjectProxy(
      "org.freedesktop.DBus", dbus::ObjectPath("/org/freedesktop/DBus"));

  auto on_response =
      base::BindOnce([](dbus_utils::CallMethodResultSig<"s"> response) {
        is_available = response.has_value();
        checked = true;
        for (auto& cb : *pending_callbacks) {
          std::move(cb).Run(is_available);
        }
        pending_callbacks->clear();
      });

  dbus_utils::CallMethod<"s", "s">(bus_proxy, "org.freedesktop.DBus",
                                   "GetNameOwner", std::move(on_response),
                                   std::string(kPortalServiceName));
}

// static
void FileTransferPortal::RetrieveFiles(
    const std::string& raw_key,
    base::OnceCallback<void(std::vector<std::string>)> callback,
    dbus::Bus* bus) {
  if (!bus) {
    bus = dbus_thread_linux::GetSharedSessionBus().get();
  }

  if (!bus) {
    std::move(callback).Run({});
    return;
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
    std::move(callback).Run({});
    return;
  }

  std::map<std::string, dbus_utils::Variant> options;
  auto on_response = base::BindOnce(
      [](base::OnceCallback<void(std::vector<std::string>)> callback,
         dbus_utils::CallMethodResultSig<"as"> response) {
        if (!response.has_value()) {
          LOG(ERROR) << "Failed to call RetrieveFiles on FileTransfer portal.";
          std::move(callback).Run({});
          return;
        }
        std::move(callback).Run(std::move(std::get<0>(response.value())));
      },
      std::move(callback));

  dbus_utils::CallMethod<"sa{sv}", "as">(proxy, kFileTransferInterfaceName,
                                         kMethodRetrieveFiles,
                                         std::move(on_response), key, options);
}

// static
void FileTransferPortal::RegisterFiles(
    const std::vector<std::string>& files,
    base::OnceCallback<void(std::string)> callback,
    dbus::Bus* bus) {
  if (!bus) {
    bus = dbus_thread_linux::GetSharedSessionBus().get();
  }

  if (files.empty() || !bus) {
    std::move(callback).Run("");
    return;
  }

  dbus::ObjectProxy* proxy = bus->GetObjectProxy(
      kPortalServiceName, dbus::ObjectPath(kPortalObjectPath));
  if (!proxy) {
    std::move(callback).Run("");
    return;
  }

  FileTransferSession::Start(files, std::move(callback), proxy);
}

}  // namespace dbus_xdg
