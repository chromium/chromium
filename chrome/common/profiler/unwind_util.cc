// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/profiler/unwind_util.h"

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
#include "chrome/common/profiler/process_type.h"
#include "components/sampling_profiler/process_type.h"
#include "components/version_info/channel.h"

#if BUILDFLAG(IS_ANDROID) && defined(ARCH_CPU_ARMEL) && \
    BUILDFLAG(ENABLE_ARM_CFI_TABLE)
#define ANDROID_ARM32_UNWINDING_SUPPORTED 1
#else
#define ANDROID_ARM32_UNWINDING_SUPPORTED 0
#endif

#if BUILDFLAG(IS_ANDROID) && defined(ARCH_CPU_ARM64)
#define ANDROID_ARM64_UNWINDING_SUPPORTED 1
#else
#define ANDROID_ARM64_UNWINDING_SUPPORTED 0
#endif

#if ANDROID_ARM32_UNWINDING_SUPPORTED || ANDROID_ARM64_UNWINDING_SUPPORTED
#define ANDROID_UNWINDING_SUPPORTED 1
#else
#define ANDROID_UNWINDING_SUPPORTED 0
#endif

#if ANDROID_ARM32_UNWINDING_SUPPORTED
#include "base/android/apk_assets.h"
#include "base/android/library_loader/anchor_functions.h"
#include "base/files/memory_mapped_file.h"
#include "base/profiler/chrome_unwinder_android.h"
#endif  // ANDROID_ARM32_UNWINDING_SUPPORTED

#if ANDROID_ARM64_UNWINDING_SUPPORTED
#include "base/profiler/frame_pointer_unwinder.h"
#endif  // ANDROID_ARM64_UNWINDING_SUPPORTED

#if ANDROID_UNWINDING_SUPPORTED
#include "chrome/android/modules/stack_unwinder/public/module.h"
#include "chrome/common/profiler/native_unwinder_android_map_delegate_impl.h"

extern "C" {
// The address of |__executable_start| is the base address of the executable or
// shared library.
extern char __executable_start;
}
#endif  // ANDROID_UNWINDING_SUPPORTED

// See `RequestUnwindPrerequisitesInstallation` below.
BASE_FEATURE(kInstallAndroidUnwindDfm,
             "InstallAndroidUnwindDfm",
             base::FEATURE_DISABLED_BY_DEFAULT);

namespace {

// Encapsulates the setup required to create the Chrome unwinder on 32 bit
// Android.
#if ANDROID_ARM32_UNWINDING_SUPPORTED
class ChromeUnwinderCreator {
 public:
  ChromeUnwinderCreator() {
    constexpr char kCfiFileName[] = "assets/unwind_cfi_32_v2";
    constexpr char kSplitName[] = "stack_unwinder";

    base::MemoryMappedFile::Region cfi_region;
    int fd = base::android::OpenApkAsset(kCfiFileName, kSplitName, &cfi_region);
    CHECK_GE(fd, 0);
    bool mapped_file_ok =
        chrome_cfi_file_.Initialize(base::File(fd), cfi_region);
    CHECK(mapped_file_ok);
  }
  ChromeUnwinderCreator(const ChromeUnwinderCreator&) = delete;
  ChromeUnwinderCreator& operator=(const ChromeUnwinderCreator&) = delete;

  std::unique_ptr<base::Unwinder> Create() {
    return std::make_unique<base::ChromeUnwinderAndroid>(
        base::CreateChromeUnwindInfoAndroid(
            {chrome_cfi_file_.data(), chrome_cfi_file_.length()}),
        /* chrome_module_base_address= */
        reinterpret_cast<uintptr_t>(&__executable_start),
        /* text_section_start_address= */ base::android::kStartOfText);
  }

 private:
  base::MemoryMappedFile chrome_cfi_file_;
};
#endif                                   // ANDROID_ARM32_UNWINDING_SUPPORTED

#if ANDROID_UNWINDING_SUPPORTED
std::vector<std::unique_ptr<base::Unwinder>> CreateLibunwindstackUnwinders(
    stack_unwinder::Module* const stack_unwinder_module) {
  CHECK_NE(getpid(), gettid());
  std::vector<std::unique_ptr<base::Unwinder>> unwinders;
  unwinders.push_back(stack_unwinder_module->CreateLibunwindstackUnwinder());
  return unwinders;
}

std::vector<std::unique_ptr<base::Unwinder>> CreateCoreUnwinders(
    stack_unwinder::Module* const stack_unwinder_module) {
  CHECK_NE(getpid(), gettid());

#if ANDROID_ARM64_UNWINDING_SUPPORTED
  // For now, we only use Libunwindstack on 64 bit (no other unwinders).
  return CreateLibunwindstackUnwinders(stack_unwinder_module);
#else
  static base::NoDestructor<NativeUnwinderAndroidMapDelegateImpl> map_delegate(
      stack_unwinder_module);
  static base::NoDestructor<ChromeUnwinderCreator> chrome_unwinder_creator;

  // Note order matters: the more general unwinder must appear first in the
  // vector.
  std::vector<std::unique_ptr<base::Unwinder>> unwinders;
  unwinders.push_back(stack_unwinder_module->CreateNativeUnwinder(
      map_delegate.get(), reinterpret_cast<uintptr_t>(&__executable_start)));
  unwinders.push_back(chrome_unwinder_creator->Create());

  return unwinders;
#endif
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
#endif  // ANDROID_UNWINDING_SUPPORTED

}  // namespace

void RequestUnwindPrerequisitesInstallation(
    version_info::Channel channel,
    UnwindPrerequisitesDelegate* prerequites_delegate) {
  CHECK_EQ(sampling_profiler::ProfilerProcessType::kBrowser,
           GetProfilerProcessType(*base::CommandLine::ForCurrentProcess()));
  if (AreUnwindPrerequisitesAvailable(channel, prerequites_delegate)) {
    return;
  }
#if ANDROID_UNWINDING_SUPPORTED && defined(OFFICIAL_BUILD) && \
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
#if BUILDFLAG(IS_ANDROID)
#if ANDROID_UNWINDING_SUPPORTED
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
#else   // ANDROID_UNWINDING_SUPPORTED
  return false;
#endif  // ANDROID_UNWINDING_SUPPORTED
#else   // BUILDFLAG(IS_ANDROID)
  return true;
#endif  // BUILDFLAG(IS_ANDROID)
}

#if ANDROID_UNWINDING_SUPPORTED
stack_unwinder::Module* GetOrLoadModule() {
  CHECK(AreUnwindPrerequisitesAvailable(chrome::GetChannel()));
  static base::NoDestructor<std::unique_ptr<stack_unwinder::Module>>
      stack_unwinder_module(stack_unwinder::Module::Load());
  return stack_unwinder_module.get()->get();
}
#endif  // ANDROID_UNWINDING_SUPPORTED

base::StackSamplingProfiler::UnwindersFactory CreateCoreUnwindersFactory() {
  if (!AreUnwindPrerequisitesAvailable(chrome::GetChannel())) {
    return base::StackSamplingProfiler::UnwindersFactory();
  }
#if ANDROID_UNWINDING_SUPPORTED
  return base::BindOnce(CreateCoreUnwinders, GetOrLoadModule());
#else   // ANDROID_UNWINDING_SUPPORTED
  return base::StackSamplingProfiler::UnwindersFactory();
#endif  // ANDROID_UNWINDING_SUPPORTED
}

base::StackSamplingProfiler::UnwindersFactory
CreateLibunwindstackUnwinderFactory() {
  if (!AreUnwindPrerequisitesAvailable(chrome::GetChannel())) {
    return base::StackSamplingProfiler::UnwindersFactory();
  }
#if ANDROID_UNWINDING_SUPPORTED
  return base::BindOnce(CreateLibunwindstackUnwinders, GetOrLoadModule());
#else   // ANDROID_UNWINDING_SUPPORTED
  return base::StackSamplingProfiler::UnwindersFactory();
#endif  // ANDROID_UNWINDING_SUPPORTED
}
