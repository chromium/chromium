// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_watcher/extended_crash_reporting.h"

#include <windows.h>

#include <memory>

#include "base/debug/activity_tracker.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/persistent_memory_allocator.h"
#include "base/strings/string_piece.h"
#include "base/time/time.h"
#include "base/win/pe_image.h"
#include "build/build_config.h"
#include "components/browser_watcher/activity_data_names.h"
#include "components/browser_watcher/activity_report.pb.h"
#include "components/browser_watcher/activity_tracker_annotation.h"
#include "components/browser_watcher/extended_crash_reporting_metrics.h"
#include "components/browser_watcher/features.h"

#if BUILDFLAG(IS_WIN)
// https://devblogs.microsoft.com/oldnewthing/20041025-00/?p=37483.
extern "C" IMAGE_DOS_HEADER __ImageBase;
#endif

namespace browser_watcher {

namespace {

ExtendedCrashReporting* g_instance = nullptr;

uintptr_t GetProgramCounter(const CONTEXT& context) {
#if defined(ARCH_CPU_X86)
  return context.Eip;
#elif defined(ARCH_CPU_X86_64)
  return context.Rip;
#elif defined(ARCH_CPU_ARM64)
  return context.Pc;
#endif
}

LONG CALLBACK VectoredExceptionHandler(EXCEPTION_POINTERS* exception_pointers) {
  base::debug::GlobalActivityTracker* tracker =
      base::debug::GlobalActivityTracker::Get();
  if (tracker) {
    EXCEPTION_RECORD* record = exception_pointers->ExceptionRecord;
    uintptr_t pc = GetProgramCounter(*exception_pointers->ContextRecord);
    tracker->RecordException(reinterpret_cast<void*>(pc),
                             record->ExceptionAddress, record->ExceptionCode);
  }

  return EXCEPTION_CONTINUE_SEARCH;  // Continue to the next handler.
}

// Record information about the chrome module.
void RecordChromeModuleInfo(
    base::debug::GlobalActivityTracker* global_tracker) {
  DCHECK(global_tracker);

  base::debug::GlobalActivityTracker::ModuleInfo module;
  module.is_loaded = true;
  module.address = reinterpret_cast<uintptr_t>(&__ImageBase);

  base::win::PEImage pe(&__ImageBase);
  PIMAGE_NT_HEADERS headers = pe.GetNTHeaders();
  CHECK(headers);
  module.size = headers->OptionalHeader.SizeOfImage;
  module.timestamp = headers->FileHeader.TimeDateStamp;

  GUID guid;
  DWORD age;
  LPCSTR pdb_filename = nullptr;
  size_t pdb_filename_length = 0;
  if (pe.GetDebugId(&guid, &age, &pdb_filename, &pdb_filename_length)) {
    module.age = age;
    static_assert(sizeof(module.identifier) >= sizeof(guid),
                  "Identifier field must be able to contain a GUID.");
    memcpy(module.identifier, &guid, sizeof(guid));
  } else {
    memset(module.identifier, 0, sizeof(module.identifier));
  }

  module.file = "chrome.dll";
  module.debug_file =
      std::string(base::StringPiece(pdb_filename, pdb_filename_length));

  global_tracker->RecordModuleInfo(module);
}

}  // namespace

ExtendedCrashReporting::ExtendedCrashReporting(
    base::debug::GlobalActivityTracker* tracker)
    : tracker_(tracker) {}

ExtendedCrashReporting::~ExtendedCrashReporting() {
  if (veh_handle_)
    ::RemoveVectoredExceptionHandler(veh_handle_);
}

ExtendedCrashReporting* ExtendedCrashReporting::SetUpIfEnabled(
    ProcessType process_type) {
  DCHECK_EQ(nullptr, g_instance);
  if (!base::FeatureList::IsEnabled(kExtendedCrashReportingFeature)) {
    return nullptr;
  }

  return SetUpImpl(process_type);
}

ExtendedCrashReporting* ExtendedCrashReporting::GetInstance() {
  return g_instance;
}

void ExtendedCrashReporting::SetProductStrings(
    const std::u16string& product_name,
    const std::u16string& product_version,
    const std::u16string& channel_name,
    const std::u16string& special_build) {
  base::debug::ActivityUserData& proc_data = tracker_->process_data();
  proc_data.SetString(kActivityProduct, product_name);
  proc_data.SetString(kActivityVersion, product_version);
  proc_data.SetString(kActivityChannel, channel_name);
  proc_data.SetString(kActivitySpecialBuild, special_build);
}

void ExtendedCrashReporting::SetBool(base::StringPiece name, bool value) {
  tracker_->process_data().SetBool(name, value);
}

void ExtendedCrashReporting::SetInt(base::StringPiece name, int64_t value) {
  tracker_->process_data().SetInt(name, value);
}

void ExtendedCrashReporting::SetDataBool(base::StringPiece name, bool value) {
  if (g_instance)
    g_instance->SetBool(name, value);
}

void ExtendedCrashReporting::SetDataInt(base::StringPiece name, int64_t value) {
  if (g_instance)
    g_instance->SetInt(name, value);
}

void ExtendedCrashReporting::RegisterVEH() {
#if defined(ADDRESS_SANITIZER)
  // ASAN on windows x64 is dynamically allocating the shadow memory on a
  // memory access violation by setting up an vector exception handler.
  // When instrumented with ASAN, this code may trigger an exception by
  // accessing unallocated shadow memory, which is causing an infinite
  // recursion (i.e. infinite memory access violation).
  (void)&VectoredExceptionHandler;
#else
  DCHECK_EQ(nullptr, veh_handle_);
  // Register a vectored exception handler and request it be first. Note that
  // subsequent registrations may also request to be first, in which case this
  // one will be bumped.
  // TODO(manzagop): Depending on observations, it may be necessary to
  // consider refreshing the registration, either periodically or at opportune
  // (e.g. risky) times.
  veh_handle_ = ::AddVectoredExceptionHandler(1, &VectoredExceptionHandler);
  DCHECK(veh_handle_);
#endif  // ADDRESS_SANITIZER
}

void ExtendedCrashReporting::SetUpForTesting() {
  ExtendedCrashReporting::SetUpImpl(kBrowserProcess);
}

void ExtendedCrashReporting::TearDownForTesting() {
  if (g_instance) {
    ExtendedCrashReporting* instance_to_delete = g_instance;
    g_instance = nullptr;
    delete instance_to_delete;
  }

  // Clear the crash annotation.
  ActivityTrackerAnnotation::GetInstance()->Clear();
}

ExtendedCrashReporting* ExtendedCrashReporting::SetUpImpl(
    ProcessType process_type) {
  DCHECK_EQ(nullptr, g_instance);

  // TODO(https://crbug.com/1044707): Adjust these numbers once there is real
  // data to show just how much of an arena is necessary.
  const size_t kMemorySize = 1 << 20;  // 1 MiB
  const int kStackDepth = 4;
  const uint64_t kAllocatorId = 0;

  base::debug::GlobalActivityTracker::CreateWithAllocator(
      std::make_unique<base::LocalPersistentMemoryAllocator>(
          kMemorySize, kAllocatorId, kExtendedCrashReportingFeature.name),
      kStackDepth, 0);

  // Track code activities (such as posting task, blocking on locks, and
  // joining threads) that can cause hanging threads and general instability
  base::debug::GlobalActivityTracker* global_tracker =
      base::debug::GlobalActivityTracker::Get();
  DCHECK(global_tracker);

  // Construct the instance with the new global tracker, this object is
  // intentionally leaked.
  std::unique_ptr<ExtendedCrashReporting> new_instance =
      base::WrapUnique(new ExtendedCrashReporting(global_tracker));
  new_instance->Initialize(process_type);
  g_instance = new_instance.release();
  return g_instance;
}

void ExtendedCrashReporting::Initialize(ProcessType process_type) {
  // Record the location and size of the tracker memory range in a Crashpad
  // annotation to allow the handler to retrieve it on crash.
  // Record the buffer size and location for the annotation beacon.
  auto* allocator = tracker_->allocator();
  ActivityTrackerAnnotation::GetInstance()->SetValue(allocator->data(),
                                                     allocator->size());

  // Record the main DLL module info for easier symbolization.
  RecordChromeModuleInfo(tracker_);

  LogActivityRecordEvent(ActivityRecordEvent::kGotTracker);

  base::debug::ActivityUserData& proc_data = tracker_->process_data();
#if defined(ARCH_CPU_X86)
  proc_data.SetString(kActivityPlatform, "Win32");
#elif defined(ARCH_CPU_X86_64)
  proc_data.SetString(kActivityPlatform, "Win64");
#endif
  proc_data.SetInt(
      kActivityStartTimestamp,
      base::Time::Now().ToDeltaSinceWindowsEpoch().InMicroseconds());

  if (process_type == kBrowserProcess)
    proc_data.SetInt(kActivityProcessType, ProcessState::BROWSER_PROCESS);

  RegisterVEH();
}

}  // namespace browser_watcher
