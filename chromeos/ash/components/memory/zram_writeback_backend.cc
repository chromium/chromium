// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/memory/zram_writeback_backend.h"

#include <cstdint>
#include <limits>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/memory/page_size.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/system/sys_info.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "chromeos/ash/components/dbus/debug_daemon/debug_daemon_client.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/re2/src/re2/re2.h"

namespace ash::memory {

namespace {

constexpr char kSavedWritebackSizeFile[] = "/tmp/saved-writeback-size";
constexpr char kZramDev[] = "/sys/block/zram0";
constexpr char kZramBackingDevFile[] = "backing_dev";
constexpr char kZramDiskSize[] = "disksize";
constexpr char kZramWritebackLimitFile[] = "writeback_limit";

int64_t ReadFileAsInt64(const base::FilePath& path) {
  std::string str;
  if (!base::ReadFileToStringNonBlocking(path, &str)) {
    return std::numeric_limits<int64_t>::min();
  }

  if (!str.empty() && str.back() == '\n') {
    str.resize(str.size() - 1);
  }

  int64_t val;
  if (!base::StringToInt64(str, &val)) {
    return std::numeric_limits<int64_t>::min();
  }

  return val;
}

void SaveWritebackSize(uint64_t size_mb) {
  const base::FilePath file_path(kSavedWritebackSizeFile);
  DCHECK(!base::PathExists(file_path));
  if (base::WriteFile(file_path, base::NumberToString(size_mb))) {
    base::SetPosixFilePermissions(
        file_path, base::FilePermissionBits::FILE_PERMISSION_READ_BY_USER);
  }
}

void ReadWritebackSize(int64_t* v) {
  DCHECK(v);
  const base::FilePath file_path(kSavedWritebackSizeFile);
  *v = ReadFileAsInt64(file_path);
}

// The backend is responsible for the "how" things happens.
class ZramWritebackBackendImpl : public ZramWritebackBackend {
 public:
  ~ZramWritebackBackendImpl() override = default;

  void OnEnableWritebackResponse(IntCallback cb,
                                 int64_t size_mb,
                                 absl::optional<std::string> res) {
    if (!res) {
      // If we did not receive a response, it's likely that debugd is not up
      // yet. Let's try again in 30 seconds.
      base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&ZramWritebackBackendImpl::EnableWriteback,
                         weak_factory_.GetWeakPtr(), size_mb, std::move(cb)),
          base::Seconds(30));
      return;
    }

    /* let's parse the response */
    std::string resp = *res;
    int size;
    if (RE2::PartialMatch(resp, "SUCCESS: Enabled writeback with size (\\d+)MB",
                          &size)) {
      base::ThreadPool::PostTask(FROM_HERE, {base::MayBlock()},
                                 base::BindOnce(&SaveWritebackSize, size));

      std::move(cb).Run(true, size);
      return;
    } else {
      if (resp.find("ERROR") != std::string::npos) {
        LOG(ERROR) << "Error configuring zram writeback: " << resp;
      } else {
        LOG(WARNING) << "Unexpected response from debugd: " << resp;
      }
    }

    std::move(cb).Run(false, 0);
  }

  void EnableWriteback(uint64_t size_mb, IntCallback cb) override {
    DebugDaemonClient* debugd_client = DebugDaemonClient::Get();
    CHECK(debugd_client);

    debugd_client->SwapZramEnableWriteback(
        size_mb,
        base::BindOnce(&ZramWritebackBackendImpl::OnEnableWritebackResponse,
                       weak_factory_.GetWeakPtr(), std::move(cb), size_mb));
  }

  void OnMarkIdle(Callback cb, absl::optional<std::string> resp) {
    if (!resp) {
      std::move(cb).Run(false);
      return;
    }

    if ((*resp).find("SUCCESS") == std::string::npos) {
      LOG(ERROR) << "Zram mark idle returned: " << *resp;
      std::move(cb).Run(false);
      return;
    }

    std::move(cb).Run(true);
  }

  void MarkIdle(base::TimeDelta age, Callback cb) override {
    DebugDaemonClient* debugd_client = DebugDaemonClient::Get();
    CHECK(debugd_client);

    debugd_client->SwapZramMarkIdle(
        age.InSeconds(),
        base::BindOnce(&ZramWritebackBackendImpl::OnMarkIdle,
                       weak_factory_.GetWeakPtr(), std::move(cb)));
  }

  int64_t GetZramDiskSizeBytes() override {
    return ReadFileAsInt64(kZramPath.Append(kZramDiskSize));
  }

  int64_t GetCurrentWritebackLimitPages() override {
    return ReadFileAsInt64(kZramPath.Append(kZramWritebackLimitFile));
  }

  void OnSetWritebackLimitResponse(IntCallback cb,
                                   absl::optional<std::string> resp) {
    if (!resp) {
      std::move(cb).Run(false, 0);
      return;
    }

    if ((*resp).find("SUCCESS") == std::string::npos) {
      LOG(ERROR) << "Zram Set Writeback Limit returned: " << *resp;
      std::move(cb).Run(false, 0);
      return;
    }

    std::move(cb).Run(true, GetCurrentWritebackLimitPages());
  }

  void SetWritebackLimit(uint64_t size_pages, IntCallback cb) override {
    DebugDaemonClient* debugd_client = DebugDaemonClient::Get();
    CHECK(debugd_client);

    debugd_client->SwapZramSetWritebackLimit(
        size_pages,
        base::BindOnce(&ZramWritebackBackendImpl::OnSetWritebackLimitResponse,
                       weak_factory_.GetWeakPtr(), std::move(cb)));
  }

  void OnInitiateWritebackResponse(Callback cb,
                                   absl::optional<std::string> resp) {
    if (!resp) {
      std::move(cb).Run(false);
      return;
    }

    if ((*resp).find("SUCCESS") == std::string::npos) {
      // I/O Errors (-EIO) result when we've hit our writeback limit, and thus
      // are not considered an error, although we won't return true we will not
      // log an error here.
      bool io_error =
          ((*resp).find("Error 5 (Input/output error)") != std::string::npos);
      LOG_IF(ERROR, !io_error) << "Zram initiate writeback returned: " << *resp;
      std::move(cb).Run(false);
      return;
    }

    std::move(cb).Run(true);
  }

  void InitiateWriteback(ZramWritebackMode mode, Callback cb) override {
    DebugDaemonClient* debugd_client = DebugDaemonClient::Get();
    CHECK(debugd_client);

    debugd_client->InitiateSwapZramWriteback(
        static_cast<debugd::ZramWritebackMode>(mode),
        base::BindOnce(&ZramWritebackBackendImpl::OnInitiateWritebackResponse,
                       weak_factory_.GetWeakPtr(), std::move(cb)));
  }

  bool WritebackAlreadyEnabled() override {
    std::string contents;
    if (base::ReadFileToStringNonBlocking(kZramPath.Append(kZramBackingDevFile),
                                          &contents)) {
      const std::string kNone("none");
      // For whatever reason the backing file in zram will tack on a \n, let's
      // remove it.
      if (!contents.empty() && contents.at(contents.length() - 1) == '\n')
        contents = contents.substr(0, contents.length() - 1);
      if (contents != kNone) {
        return true;
      }
    }

    return false;
  }

  void GetCurrentBackingDevSize(IntCallback cb) override {
    // We need to read on the thread pool and then reply back to the callback.
    int64_t* val = new int64_t(-1);
    base::ThreadPool::PostTaskAndReply(
        FROM_HERE, {base::MayBlock()}, base::BindOnce(&ReadWritebackSize, val),
        base::BindOnce(
            [](decltype(cb) cb, decltype(val) v) {
              std::move(cb).Run(*v != std::numeric_limits<int64_t>::min(), *v);
            },
            std::move(cb), base::Owned(val)));
  }

  static const base::FilePath kZramPath;

 private:
  base::WeakPtrFactory<ZramWritebackBackendImpl> weak_factory_{this};
};

const base::FilePath ZramWritebackBackendImpl::kZramPath =
    base::FilePath(kZramDev);

}  // namespace

// static
std::unique_ptr<ZramWritebackBackend> COMPONENT_EXPORT(ASH_MEMORY)
    ZramWritebackBackend::Get() {
  return std::make_unique<ZramWritebackBackendImpl>();
}

// static
bool COMPONENT_EXPORT(ASH_MEMORY) ZramWritebackBackend::IsSupported() {
  // zram writeback is only on CrOS 5.x+ kernels.
  int32_t major_version = 0;
  int32_t minor_version = 0;
  int32_t micro_version = 0;
  base::SysInfo::OperatingSystemVersionNumbers(&major_version, &minor_version,
                                               &micro_version);
  if (major_version < 5) {
    return false;
  }

  // Was the kernel built with writeback support?
  base::FilePath backing_dev =
      ZramWritebackBackendImpl::kZramPath.Append(kZramBackingDevFile);
  if (!base::PathExists(backing_dev)) {
    return false;
  }

  return true;
}

}  // namespace ash::memory
