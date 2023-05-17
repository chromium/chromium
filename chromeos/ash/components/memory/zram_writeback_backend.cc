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
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/system/sys_info.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "chromeos/ash/components/dbus/swap_management/swap_management_client.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/re2/src/re2/re2.h"

namespace ash::memory {

namespace {

constexpr char kZramDev[] = "/sys/block/zram0";
constexpr char kZramBackingDevFile[] = "backing_dev";
constexpr char kZramDiskSize[] = "disksize";
constexpr char kZramWritebackLimitFile[] = "writeback_limit";

std::string ReadFileAsString(const base::FilePath& path) {
  std::string str;
  if (!base::ReadFileToStringNonBlocking(path, &str)) {
    return std::string();
  }

  if (!str.empty() && str.back() == '\n') {
    str.resize(str.size() - 1);
  }

  return str;
}

int64_t ReadFileAsInt64(const base::FilePath& path) {
  std::string str = ReadFileAsString(path);

  int64_t val;
  if (!base::StringToInt64(str, &val)) {
    return std::numeric_limits<int64_t>::min();
  }

  return val;
}

void ReadWritebackSize(int64_t* v) {
  DCHECK(v);
  std::string backing_dev =
      ReadFileAsString(base::FilePath(kZramDev).Append(kZramBackingDevFile));
  // Since swap_management minijail would be time out and terminated, the read
  // |backing_dev| could be "/dev/<device_name>" or "/<device_name>". We should
  // only take the device name after the last slash.
  backing_dev = base::SplitString(backing_dev, "/", base::TRIM_WHITESPACE,
                                  base::SPLIT_WANT_NONEMPTY)
                    .back();
  base::FilePath size_file =
      base::FilePath("/sys/class/block").Append(backing_dev).Append("size");
  // The unit of the content in |size_file| is number of sectors. Convert it to
  // MiB.
  *v = ReadFileAsInt64(size_file) * 512 / 1024 / 1024;
}

// The backend is responsible for the "how" things happens.
class ZramWritebackBackendImpl : public ZramWritebackBackend {
 public:
  ~ZramWritebackBackendImpl() override = default;

  void OnEnableWriteback(IntCallback cb, bool res) {
    if (!res) {
      LOG(ERROR) << "Error enabling zram writeback.";
      return;
    }

    // Read actual writeback device size and run callback.
    GetCurrentBackingDevSize(std::move(cb));
  }

  void EnableWriteback(uint64_t size, IntCallback cb) override {
    SwapManagementClient* swap_management_client = SwapManagementClient::Get();
    CHECK(swap_management_client);

    swap_management_client->SwapZramEnableWriteback(
        size, base::BindOnce(&ZramWritebackBackendImpl::OnEnableWriteback,
                             weak_factory_.GetWeakPtr(), std::move(cb)));
  }

  void OnMarkIdle(BoolCallback cb, bool res) {
    LOG_IF(ERROR, !res) << "Error marking zram idle.";
    std::move(cb).Run(res);
  }

  void MarkIdle(base::TimeDelta age, BoolCallback cb) override {
    SwapManagementClient* swap_management_client = SwapManagementClient::Get();
    CHECK(swap_management_client);

    swap_management_client->SwapZramMarkIdle(
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

  void OnSetWritebackLimitResponse(BoolIntCallback cb, bool res) {
    LOG_IF(ERROR, !res) << "Error setting zram writeback Limit.";
    std::move(cb).Run(res, res ? GetCurrentWritebackLimitPages() : 0);
  }

  void SetWritebackLimit(uint64_t size_pages, BoolIntCallback cb) override {
    SwapManagementClient* swap_management_client = SwapManagementClient::Get();
    CHECK(swap_management_client);

    swap_management_client->SwapZramSetWritebackLimit(
        size_pages,
        base::BindOnce(&ZramWritebackBackendImpl::OnSetWritebackLimitResponse,
                       weak_factory_.GetWeakPtr(), std::move(cb)));
  }

  void OnInitiateWritebackResponse(BoolCallback cb, bool res) {
    LOG_IF(ERROR, !res) << "Error initiate zram writeback.";
    std::move(cb).Run(res);
  }

  void InitiateWriteback(ZramWritebackMode mode, BoolCallback cb) override {
    SwapManagementClient* swap_management_client = SwapManagementClient::Get();
    CHECK(swap_management_client);

    swap_management_client->InitiateSwapZramWriteback(
        static_cast<swap_management::ZramWritebackMode>(mode),
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
      if (!contents.empty() && contents.at(contents.length() - 1) == '\n') {
        contents = contents.substr(0, contents.length() - 1);
      }
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
              if (*v == std::numeric_limits<int64_t>::min()) {
                LOG(ERROR) << "Unable to read zram writeback device size.";
              } else {
                std::move(cb).Run(*v);
              }
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
