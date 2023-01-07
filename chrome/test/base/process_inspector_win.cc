// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/process_inspector_win.h"

#include <winternl.h>

#include "base/logging.h"
#include "base/process/process.h"
#include "base/win/windows_version.h"

namespace {

// Certain Windows types that depend on the word size of the OS (rather than the
// size of the current process) are defined here.

// PROCESS_BASIC_INFORMATION.
template <class Traits>
struct ProcessInformation {
  using RemotePointer = typename Traits::RemotePointer;

  DWORD exit_status() const { return static_cast<DWORD>(exit_status_); }
  RemotePointer peb_base_address() const { return peb_base_address_; }
  RemotePointer affinity_mask() const { return affinity_mask_; }
  int base_priority() const { return static_cast<int>(base_priority_); }
  DWORD unique_process_id() const { return static_cast<DWORD>(unique_pid_); }
  DWORD inherited_from_unique_process_id() const {
    return static_cast<DWORD>(inherited_from_unique_process_id_);
  }

 private:
  RemotePointer exit_status_;
  RemotePointer peb_base_address_;
  RemotePointer affinity_mask_;
  RemotePointer base_priority_;
  RemotePointer unique_pid_;
  RemotePointer inherited_from_unique_process_id_;
};

// A subset of a process's environment block.
template <class Traits>
struct ProcessExecutionBlock {
  using RemotePointer = typename Traits::RemotePointer;

  uint8_t InheritedAddressSpace;
  uint8_t ReadImageFileExecOptions;
  uint8_t BeingDebugged;
  uint8_t ProcessFlags;
  uint8_t Padding[4];
  RemotePointer Mutant;
  RemotePointer ImageBaseAddress;
  RemotePointer Ldr;
  RemotePointer ProcessParameters;  // RtlUserProcessParameters
};

// UNICODE_STRING.
template <class Traits>
struct UnicodeString {
  using RemotePointer = typename Traits::RemotePointer;

  uint16_t Length;
  uint16_t MaximumLength;
  RemotePointer Buffer;
};

// CURDIR.
template <class Traits>
struct CurDir {
  using RemotePointer = typename Traits::RemotePointer;

  UnicodeString<Traits> DosPath;
  RemotePointer Handle;
};

// RTL_USER_PROCESS_PARAMETERS.
template <class Traits>
struct RtlUserProcessParameters {
  using RemotePointer = typename Traits::RemotePointer;

  uint32_t MaximumLength;
  uint32_t Length;
  uint32_t Flags;
  uint32_t DebugFlags;
  RemotePointer ConsoleHandle;
  uint32_t ConsoleFlags;
  RemotePointer StandardInput;
  RemotePointer StandardOutput;
  RemotePointer StandardError;
  CurDir<Traits> CurrentDirectory;
  UnicodeString<Traits> DllPath;
  UnicodeString<Traits> ImagePathName;
  UnicodeString<Traits> CommandLine;
};

// A concrete ProcessInspector that can read from another process based on the
// architecture. |Traits| specifies traits based on the OS architecture.
template <class Traits>
class Inspector : public ProcessInspector {
 public:
  Inspector();
  Inspector(const Inspector&) = delete;
  Inspector& operator=(const Inspector&) = delete;

  // ProcessInspector:
  DWORD GetParentPid() const override;
  const std::wstring& command_line() const override;

 private:
  // ProcessInspector:
  bool Inspect(const base::Process& process) override;

  ProcessInformation<Traits> process_basic_information_;
  ProcessExecutionBlock<Traits> peb_;
  RtlUserProcessParameters<Traits> process_parameters_;
  std::wstring command_line_;
};

#if !defined(_WIN64)
// Traits for a 32-bit process running in WoW.
struct Wow64Traits {
  // The name of the ntdll function to query process information.
  static const char kQueryProcessInformationFunctionName[];

  // The type of a pointer to the read process memory function.
  using ReadMemoryFn =
      NTSTATUS(NTAPI*)(HANDLE, uint64_t, void*, uint64_t, uint64_t*);

  // An unsigned integer type matching the size of a pointer in the remote
  // process.
  using RemotePointer = uint64_t;

  // Returns the function to read memory from a remote process.
  static ReadMemoryFn GetReadMemoryFn() {
    return reinterpret_cast<ReadMemoryFn>(::GetProcAddress(
        ::GetModuleHandle(L"ntdll.dll"), "NtWow64ReadVirtualMemory64"));
  }

  // Reads |buffer_size| bytes from |handle|'s process at |address| into
  // |buffer| using |fn|. Returns true on success.
  static bool ReadMemory(ReadMemoryFn fn,
                         HANDLE handle,
                         RemotePointer address,
                         void* buffer,
                         RemotePointer buffer_size) {
    NTSTATUS status = fn(handle, address, buffer, buffer_size, nullptr);
    if (NT_SUCCESS(status))
      return true;
    LOG(ERROR) << "Failed to read process memory with status " << std::hex
               << status << ".";
    return false;
  }
};

// static
constexpr char Wow64Traits::kQueryProcessInformationFunctionName[] =
    "NtWow64QueryInformationProcess64";

#endif

// Traits for a 32-bit process running on 32-bit Windows, or a 64-bit process
// running on 64-bit Windows.
struct NormalTraits {
  // The name of the ntdll function to query process information.
  static const char kQueryProcessInformationFunctionName[];

  // The type of a pointer to the read process memory function.
  using ReadMemoryFn = decltype(&::ReadProcessMemory);

  // An unsigned integer type matching the size of a pointer in the remote
  // process.
  using RemotePointer = uintptr_t;

  // Returns the function to read memory from a remote process.
  static ReadMemoryFn GetReadMemoryFn() { return &::ReadProcessMemory; }

  // Reads |buffer_size| bytes from |handle|'s process at |address| into
  // |buffer| using |fn|. Returns true on success.
  static bool ReadMemory(ReadMemoryFn fn,
                         HANDLE handle,
                         RemotePointer address,
                         void* buffer,
                         RemotePointer buffer_size) {
    BOOL result = fn(handle, reinterpret_cast<const void*>(address), buffer,
                     buffer_size, nullptr);
    if (result)
      return true;
    PLOG(ERROR) << "Failed to read process memory";
    return false;
  }
};

// static
constexpr char NormalTraits::kQueryProcessInformationFunctionName[] =
    "NtQueryInformationProcess";

template <class Traits>
Inspector<Traits>::Inspector() = default;

template <class Traits>
DWORD Inspector<Traits>::GetParentPid() const {
  return process_basic_information_.inherited_from_unique_process_id();
}

template <class Traits>
const std::wstring& Inspector<Traits>::command_line() const {
  return command_line_;
}

template <class Traits>
bool Inspector<Traits>::Inspect(const base::Process& process) {
  auto query_information_process_fn =
      reinterpret_cast<decltype(&::NtQueryInformationProcess)>(
          ::GetProcAddress(::GetModuleHandle(L"ntdll.dll"),
                           Traits::kQueryProcessInformationFunctionName));
  typename Traits::ReadMemoryFn read_memory_fn = Traits::GetReadMemoryFn();

  if (!query_information_process_fn)
    return false;
  ULONG in_len = sizeof(process_basic_information_);
  ULONG out_len = 0;
  NTSTATUS status = query_information_process_fn(
      process.Handle(), ProcessBasicInformation, &process_basic_information_,
      in_len, &out_len);
  if (NT_ERROR(status) || out_len != in_len)
    return false;

  if (!Traits::ReadMemory(read_memory_fn, process.Handle(),
                          process_basic_information_.peb_base_address(), &peb_,
                          sizeof(peb_))) {
    return false;
  }
  if (!Traits::ReadMemory(read_memory_fn, process.Handle(),
                          peb_.ProcessParameters, &process_parameters_,
                          sizeof(process_parameters_))) {
    return false;
  }
  if (process_parameters_.CommandLine.Length) {
    command_line_.resize(process_parameters_.CommandLine.Length /
                         sizeof(wchar_t));
    if (!Traits::ReadMemory(read_memory_fn, process.Handle(),
                            process_parameters_.CommandLine.Buffer,
                            &command_line_[0],
                            process_parameters_.CommandLine.Length)) {
      command_line_.clear();
      return false;
    }
  }
  return true;
}

}  // namespace

// static
std::unique_ptr<ProcessInspector> ProcessInspector::Create(
    const base::Process& process) {
  std::unique_ptr<ProcessInspector> inspector;
#if !defined(_WIN64)
  using base::win::OSInfo;
  if (OSInfo::GetInstance()->IsWowX86OnAMD64())
    inspector = std::make_unique<Inspector<Wow64Traits>>();
#endif
  if (!inspector)
    inspector = std::make_unique<Inspector<NormalTraits>>();
  if (!inspector->Inspect(process))
    inspector.reset();
  return inspector;
}
