// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/common/profiler/core_unwinders.h"

#include <memory>
#include <type_traits>
#include <vector>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/no_destructor.h"
#include "base/profiler/profiler_buildflags.h"
#include "base/profiler/stack_sampling_profiler.h"
#include "base/profiler/unwinder.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/profiler/native_unwinder_android_map_delegate_impl.h"
#include "chrome/common/profiler/process_type.h"
#include "components/sampling_profiler/process_type.h"
#include "components/version_info/channel.h"

#if defined(ARCH_CPU_ARMEL) && BUILDFLAG(ENABLE_ARM_CFI_TABLE)
#define ARM32_UNWINDING_SUPPORTED 1
#else
#define ARM32_UNWINDING_SUPPORTED 0
#endif

#if defined(ARCH_CPU_ARM64)
#define ARM64_UNWINDING_SUPPORTED 1
#else
#define ARM64_UNWINDING_SUPPORTED 0
#endif

#if ARM32_UNWINDING_SUPPORTED || ARM64_UNWINDING_SUPPORTED
#define UNWINDING_SUPPORTED 1
#else
#define UNWINDING_SUPPORTED 0
#endif

#if ARM32_UNWINDING_SUPPORTED
#include "base/android/apk_assets.h"
#include "base/android/library_loader/anchor_functions.h"
#include "base/files/memory_mapped_file.h"
#include "base/profiler/chrome_unwinder_android_32.h"
#endif  // ARM32_UNWINDING_SUPPORTED

#if ARM64_UNWINDING_SUPPORTED
#include "base/profiler/frame_pointer_unwinder.h"
#endif  // ARM64_UNWINDING_SUPPORTED

#if UNWINDING_SUPPORTED
#include "base/profiler/libunwindstack_unwinder_android.h"
#include "base/profiler/native_unwinder_android.h"
#include "chrome/android/modules/stack_unwinder/public/module.h"
#include "chrome/common/profiler/native_unwinder_android_map_delegate_impl.h"

extern "C" {
// The address of |__executable_start| is the base address of the executable or
// shared library.
extern char __executable_start;
}
#endif  // UNWINDING_SUPPORTED

// See `RequestUnwindPrerequisitesInstallation` below.
BASE_FEATURE(kInstallAndroidUnwindDfm,
             "InstallAndroidUnwindDfm",
             base::FEATURE_DISABLED_BY_DEFAULT);

namespace {

// Encapsulates the setup required to create the Chrome unwinder on 32 bit
// Android.
#if ARM32_UNWINDING_SUPPORTED
class ChromeUnwinderAndroid32Creator {
 public:
  ChromeUnwinderAndroid32Creator() {
    constexpr char kCfiFileName[] = "assets/unwind_cfi_32_v2";
    constexpr char kSplitName[] = "stack_unwinder";

    base::MemoryMappedFile::Region cfi_region;
    int fd = base::android::OpenApkAsset(kCfiFileName, kSplitName, &cfi_region);
    CHECK_GE(fd, 0);
    bool mapped_file_ok =
        chrome_cfi_file_.Initialize(base::File(fd), cfi_region);
    CHECK(mapped_file_ok);
  }
  ChromeUnwinderAndroid32Creator(const ChromeUnwinderAndroid32Creator&) =
      delete;
  ChromeUnwinderAndroid32Creator& operator=(
      const ChromeUnwinderAndroid32Creator&) = delete;

  std::unique_ptr<base::Unwinder> Create() {
    return std::make_unique<base::ChromeUnwinderAndroid32>(
        base::CreateChromeUnwindInfoAndroid32(
            {chrome_cfi_file_.data(), chrome_cfi_file_.length()}),
        /* chrome_module_base_address= */
        reinterpret_cast<uintptr_t>(&__executable_start),
        /* text_section_start_address= */ base::android::kStartOfText);
  }

 private:
  base::MemoryMappedFile chrome_cfi_file_;
};
#endif  // ARM32_UNWINDING_SUPPORTED

#if UNWINDING_SUPPORTED
std::vector<std::unique_ptr<base::Unwinder>> CreateLibunwindstackUnwinders() {
  // Ensure that the unwinder initialization occurs off the main thread, since
  // it involves some additional latency.
  CHECK_NE(getpid(), gettid());
  std::vector<std::unique_ptr<base::Unwinder>> unwinders;
  unwinders.push_back(std::make_unique<base::LibunwindstackUnwinderAndroid>());
  return unwinders;
}

std::vector<std::unique_ptr<base::Unwinder>> CreateCoreUnwinders() {
  // Ensure that the unwinder initialization occurs off the main thread, since
  // it involves some additional latency.
  CHECK_NE(getpid(), gettid());

  static base::NoDestructor<NativeUnwinderAndroidMapDelegateImpl> map_delegate;
#if ARM32_UNWINDING_SUPPORTED
  static base::NoDestructor<ChromeUnwinderAndroid32Creator>
      chrome_unwinder_android_32_creator;
#endif

  // Note order matters: the more general unwinder must appear first in the
  // vector.
  std::vector<std::unique_ptr<base::Unwinder>> unwinders;
  unwinders.push_back(std::make_unique<base::NativeUnwinderAndroid>(
      reinterpret_cast<uintptr_t>(&__executable_start), map_delegate.get()));
#if ARM32_UNWINDING_SUPPORTED
  // ARM32 requires our custom Chrome unwinder.
  unwinders.push_back(chrome_unwinder_android_32_creator->Create());
#else
  // ARM64 builds with frame pointers so we can use FramePointerUnwinder there.
  unwinders.push_back(std::make_unique<base::FramePointerUnwinder>(
      base::BindRepeating([](const base::Frame& current_frame) {
        return current_frame.module &&
               current_frame.module->GetBaseAddress() ==
                   reinterpret_cast<uintptr_t>(&__executable_start);
      }),
      /*is_system_unwinder=*/false));
#endif
  return unwinders;
}

// Manages installation of the module prerequisite for unwinding. Android, in
// particular, requires a dynamic feature module to provide the native unwinder.
class ModuleUnwindPrerequisitesDelegate : public UnwindPrerequisitesDelegate {
 public:
  void RequestInstallation(version_info::Channel /* unused */) override {
    stack_unwinder::Module::RequestInstallation();
  }

  bool AreAvailable(version_info::Channel channel) override {
    return stack_unwinder::Module::IsInstalled();
  }
};
#endif  // UNWINDING_SUPPORTED

}  // namespace

void RequestUnwindPrerequisitesInstallation(
    version_info::Channel channel,
    UnwindPrerequisitesDelegate* prerequites_delegate) {
  CHECK_EQ(sampling_profiler::ProfilerProcessType::kBrowser,
           GetProfilerProcessType(*base::CommandLine::ForCurrentProcess()));
  if (AreUnwindPrerequisitesAvailable(channel, prerequites_delegate)) {
    return;
  }
#if UNWINDING_SUPPORTED && defined(OFFICIAL_BUILD) && \
    BUILDFLAG(GOOGLE_CHROME_BRANDING)
  ModuleUnwindPrerequisitesDelegate default_delegate;
  if (prerequites_delegate == nullptr) {
    prerequites_delegate = &default_delegate;
  }
  // We only want to incur the cost of universally downloading the module in
  // early channels, where profiling will occur over substantially all of
  // the population. When supporting later channels in the future we will
  // enable profiling for only a fraction of users and only download for
  // those users.
  //
  // The install occurs asynchronously, with the module available at the first
  // run of Chrome following install.
  if (base::FeatureList::IsEnabled(kInstallAndroidUnwindDfm)) {
    prerequites_delegate->RequestInstallation(channel);
  }
#endif
}

bool AreUnwindPrerequisitesAvailable(
    version_info::Channel channel,
    UnwindPrerequisitesDelegate* prerequites_delegate) {
// While non-Android platforms do not need any specific prerequisites beyond
// what is already bundled and available with Chrome for their platform-specific
// unwinders to work, Android, in particular, requires a DFM to be installed.
//
// Therefore, unwind prerequisites for non-supported Android platforms are not
// considered to be available by default, but prerequisites for non-Android
// platforms are considered to be available by default.
//
// This is also why we do not need to check `prerequites_delegate` for
// non-Android platforms. Regardless of the provided delegate, unwind
// prerequisites are always considered to be available for non-Android
// platforms.
#if UNWINDING_SUPPORTED
#if defined(OFFICIAL_BUILD) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
  // Sometimes, DFMs can be installed even if not requested by Chrome
  // explicitly (for instance, in some app stores). Therefore, even if the
  // unwinder module is installed, we only consider it to be available for
  // specific channels.
  if (!(channel == version_info::Channel::CANARY ||
        channel == version_info::Channel::DEV ||
        channel == version_info::Channel::BETA)) {
    return false;
  }
#endif  // defined(OFFICIAL_BUILD) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
  ModuleUnwindPrerequisitesDelegate default_delegate;
  if (prerequites_delegate == nullptr) {
    prerequites_delegate = &default_delegate;
  }
  return prerequites_delegate->AreAvailable(channel);
#else   // UNWINDING_SUPPORTED
  return false;
#endif  // UNWINDING_SUPPORTED
}

#if UNWINDING_SUPPORTED
void LoadModule() {
  CHECK(AreUnwindPrerequisitesAvailable(chrome::GetChannel()));
  static base::NoDestructor<std::unique_ptr<stack_unwinder::Module>>
      stack_unwinder_module(stack_unwinder::Module::Load());
}
#endif  // UNWINDING_SUPPORTED

base::StackSamplingProfiler::UnwindersFactory CreateCoreUnwindersFactory() {
  if (!AreUnwindPrerequisitesAvailable(chrome::GetChannel())) {
    return base::StackSamplingProfiler::UnwindersFactory();
  }
#if UNWINDING_SUPPORTED
#if ARM32_UNWINDING_SUPPORTED
  LoadModule();
  return base::BindOnce(CreateCoreUnwinders);
#else   // ARM32_UNWINDING_SUPPORTED
  // On ARM64 for now, mimic the existing support for browser main thread, which
  // uses the libunwindstack unwinder.
  // TODO(crbug.com/380487894): determine if we can avoid this special case and
  // just use the core unwinders, based on observed data quality.
  if (GetProfilerProcessType(*base::CommandLine::ForCurrentProcess()) ==
          sampling_profiler::ProfilerProcessType::kBrowser &&
      getpid() == gettid()) {
    return CreateLibunwindstackUnwinderFactory();
  }
  return base::BindOnce(CreateCoreUnwinders);
#endif  // ARM32_UNWINDING_SUPPORTED
#else   // UNWINDING_SUPPORTED
  return base::StackSamplingProfiler::UnwindersFactory();
#endif  // UNWINDING_SUPPORTED
}

base::StackSamplingProfiler::UnwindersFactory
CreateLibunwindstackUnwinderFactory() {
  if (!AreUnwindPrerequisitesAvailable(chrome::GetChannel())) {
    return base::StackSamplingProfiler::UnwindersFactory();
  }
#if UNWINDING_SUPPORTED
  return base::BindOnce(CreateLibunwindstackUnwinders);
#else   // UNWINDING_SUPPORTED
  return base::StackSamplingProfiler::UnwindersFactory();
#endif  // UNWINDING_SUPPORTED
}
