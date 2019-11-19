// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/crash_collector/arc_crash_collector_bridge.h"

#include <sysexits.h>
#include <unistd.h>

#include <utility>

#include "base/bind.h"
#include "base/files/scoped_file.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "base/process/launch.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "components/arc/arc_browser_context_keyed_service_factory_base.h"
#include "components/arc/session/arc_bridge_service.h"
#include "mojo/public/cpp/system/platform_handle.h"

namespace {

const char kCrashReporterPath[] = "/sbin/crash_reporter";

// Runs crash_reporter to save the crash info provided via the pipe.
void RunCrashReporter(const std::string& crash_type,
                      const std::string& device,
                      const std::string& board,
                      const std::string& cpu_abi,
                      base::ScopedFD pipe) {
  base::LaunchOptions options;
  options.fds_to_remap.emplace_back(pipe.get(), STDIN_FILENO);
  auto process =
      base::LaunchProcess({kCrashReporterPath, "--arc_java_crash=" + crash_type,
                           "--arc_device=" + device, "--arc_board=" + board,
                           "--arc_cpu_abi=" + cpu_abi},
                          options);

  int exit_code = 0;
  if (!process.WaitForExit(&exit_code)) {
    LOG(ERROR) << "Failed to wait for " << kCrashReporterPath;
  } else if (exit_code != EX_OK) {
    LOG(ERROR) << kCrashReporterPath << " failed with exit code " << exit_code;
  }
}

}  // namespace

namespace arc {
namespace {

// Singleton factory for ArcCrashCollectorBridge.
class ArcCrashCollectorBridgeFactory
    : public internal::ArcBrowserContextKeyedServiceFactoryBase<
          ArcCrashCollectorBridge,
          ArcCrashCollectorBridgeFactory> {
 public:
  // Factory name used by ArcBrowserContextKeyedServiceFactoryBase.
  static constexpr const char* kName = "ArcCrashCollectorBridgeFactory";

  static ArcCrashCollectorBridgeFactory* GetInstance() {
    return base::Singleton<ArcCrashCollectorBridgeFactory>::get();
  }

 private:
  friend base::DefaultSingletonTraits<ArcCrashCollectorBridgeFactory>;
  ArcCrashCollectorBridgeFactory() = default;
  ~ArcCrashCollectorBridgeFactory() override = default;
};

}  // namespace

// static
ArcCrashCollectorBridge* ArcCrashCollectorBridge::GetForBrowserContext(
    content::BrowserContext* context) {
  return ArcCrashCollectorBridgeFactory::GetForBrowserContext(context);
}

ArcCrashCollectorBridge::ArcCrashCollectorBridge(
    content::BrowserContext* context,
    ArcBridgeService* bridge_service)
    : arc_bridge_service_(bridge_service) {
  arc_bridge_service_->crash_collector()->SetHost(this);
}

ArcCrashCollectorBridge::~ArcCrashCollectorBridge() {
  arc_bridge_service_->crash_collector()->SetHost(nullptr);
}

void ArcCrashCollectorBridge::DumpCrash(const std::string& type,
                                        mojo::ScopedHandle pipe) {
  base::PostTask(
      FROM_HERE, {base::ThreadPool(), base::WithBaseSyncPrimitives()},
      base::BindOnce(&RunCrashReporter, type, device_, board_, cpu_abi_,
                     mojo::UnwrapPlatformHandle(std::move(pipe)).TakeFD()));
}

void ArcCrashCollectorBridge::SetBuildProperties(const std::string& device,
                                                 const std::string& board,
                                                 const std::string& cpu_abi) {
  device_ = device;
  board_ = board;
  cpu_abi_ = cpu_abi;
}

}  // namespace arc
