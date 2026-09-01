// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file defines the unpack function needed to unpack compressed and
// uncompressed archives in setup.exe.

#include "chrome/installer/setup/unpack_archive.h"

#include <windows.h>

#include <ios>
#include <string>
#include <type_traits>

#include "base/check.h"
#include "base/check_op.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/memory/page_size.h"
#include "base/scoped_native_library.h"
#include "base/strings/string_util.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "base/win/pe_image_reader.h"
#include "chrome/installer/setup/installer_state.h"
#include "chrome/installer/setup/setup_constants.h"
#include "chrome/installer/setup/setup_util.h"
#include "chrome/installer/util/installer_util_strings.h"
#include "chrome/installer/util/lzma_util.h"
#include "chrome/installer/util/util_constants.h"

namespace installer {

namespace {

// Returns true if `resource_type` is "B7".
bool IsCompressedResourceType(const std::wstring& resource_type) {
  // Resource types are case-insensitive.
  return base::EqualsCaseInsensitiveASCII(resource_type, kLZMAResourceType);
}

// Returns EXCEPTION_EXECUTE_HANDLER and populates `status` with the underlying
// NTSTATUS code for paging errors encountered while accessing `buffer`.
// Otherwise, returns EXCEPTION_CONTINUE_SEARCH.
DWORD FilterPageError(base::span<const uint8_t> buffer,
                      DWORD exception_code,
                      const EXCEPTION_POINTERS* info,
                      int32_t& status) {
  if (exception_code != EXCEPTION_IN_PAGE_ERROR) {
    return EXCEPTION_CONTINUE_SEARCH;
  }

  const EXCEPTION_RECORD* exception_record = info->ExceptionRecord;
  // The exception record for a EXCEPTION_IN_PAGE_ERROR has:
  // ExceptionInformation[0] : 0 for read, 1 for write, 8 for DEP violation.
  // ExceptionInformation[1] : virtual address attempted.
  // ExceptionInformation[2] : underlying cause NTSTATUS.
  const auto address =
      static_cast<uintptr_t>(exception_record->ExceptionInformation[1]);
  if (reinterpret_cast<uintptr_t>(buffer.data()) <= address &&
      address < reinterpret_cast<uintptr_t>(buffer.data()) + buffer.size()) {
    status = static_cast<int32_t>(exception_record->ExceptionInformation[2]);
    return EXCEPTION_EXECUTE_HANDLER;
  }

  return EXCEPTION_CONTINUE_SEARCH;
}

// Returns a span covering the valid region of memory for `module`, which may be
// a PE image loaded as a data file, an image resource, or for execution in the
// process. Returns an error if `module` does not appear to be a well-formed PE
// image.
base::expected<base::span<const uint8_t>, UnPackStatus> GetModuleSpanImpl(
    HMODULE module) {
  // Mask off the two low-order bits, which encode whether the module was loaded
  // as a data file (bit 0) or as an image resource (bit 1).
  const auto* base_address = reinterpret_cast<const uint8_t*>(
      reinterpret_cast<uintptr_t>(module) & ~uintptr_t{3});

  // The LDR_IS_DATAFILE check for a module loaded as a data file (no section
  // alignment expansion):
  if ((reinterpret_cast<uintptr_t>(module) & 1) != 0) {
    // The entire file is mapped into memory.
    MEMORY_BASIC_INFORMATION mbi = {};
    if (!::VirtualQuery(base_address, &mbi, sizeof(mbi))) {
      PLOG(ERROR) << "VirtualQuery failed for mini_installer";
      return base::unexpected(UNPACK_ARCHIVE_CANNOT_OPEN);
    }
    // SAFETY: mbi.RegionSize is the size of the mapped file in memory.
    return UNSAFE_BUFFERS(base::span(base_address, mbi.RegionSize));
  }

  // Loaded as an image mapping: SizeOfImage from the PE header covers the full
  // virtual address space of the image. Use `PeImageReader` to determine the
  // size, as it supports cross-bitness images.
  base::win::PeImageReader pe_reader;
  // SAFETY: The OS loader maps at least one page for the module's headers.
  if (!pe_reader.Initialize(
          UNSAFE_BUFFERS(base::span(base_address, base::GetPageSize())))) {
    LOG(ERROR) << "mini_installer is not a valid PE image.";
    return base::unexpected(UNPACK_ARCHIVE_NOT_FOUND);
  }
  // SAFETY: SizeOfImage covers the entire memory-mapped image.
  return UNSAFE_BUFFERS(base::span(base_address, pe_reader.GetSizeOfImage()));
}

// Returns the span of memory occupied by the module image in memory.
base::expected<base::span<const uint8_t>, UnPackStatus> GetModuleSpan(
    HMODULE module) {
  // LoadLibraryExW with LOAD_LIBRARY_AS_IMAGE_RESOURCE or
  // LOAD_LIBRARY_AS_DATAFILE tags the low bits of the HMODULE handle. Mask off
  // the low bits to get the actual mapped base address.
  const auto* base_address = reinterpret_cast<const uint8_t*>(
      reinterpret_cast<uintptr_t>(module) & ~uintptr_t{3});

  // The DOS header and NT headers reside in the first page of the mapped
  // module. SAFETY: The OS loader maps at least one page for the module's
  // headers.
  auto header_span =
      UNSAFE_BUFFERS(base::span(base_address, base::GetPageSize()));

  int32_t ntstatus = 0;
  __try {
    return GetModuleSpanImpl(module);
  } __except (FilterPageError(header_span, GetExceptionCode(),
                              GetExceptionInformation(), ntstatus)) {
    LOG(ERROR) << "EXCEPTION_IN_PAGE_ERROR while reading module headers; "
                  "NTSTATUS = 0x"
               << std::hex << static_cast<uint32_t>(ntstatus);
    return base::unexpected(UNPACK_IO_DEVICE_ERROR);
  }
}

// Returns a span over the resource in `module` identified by `name` of type
// `type`, or an empty span if not found or empty.
base::span<const uint8_t> GetResourceSpan(HMODULE module,
                                          const std::wstring& name,
                                          const std::wstring& type) {
  HRSRC res_info = ::FindResourceW(module, name.c_str(), type.c_str());
  if (!res_info) {
    PLOG(ERROR) << "Failed to find archive resource " << name << " of type "
                << type;
    return {};
  }

  HGLOBAL res_data = ::LoadResource(module, res_info);
  if (!res_data) {
    PLOG(ERROR) << "Failed to load archive resource " << name;
    return {};
  }

  const DWORD size = ::SizeofResource(module, res_info);
  if (!size) {
    LOG(ERROR) << "Empty archive resource " << name;
    return {};
  }

  const auto* data = static_cast<const uint8_t*>(::LockResource(res_data));
  if (!data) {
    LOG(ERROR) << "Failed to lock archive resource " << name;
    return {};
  }

  // SAFETY: The bounds of the resource are provided by the OS above.
  return UNSAFE_BUFFERS(base::span(data, size));
}

// Extracts the contents of `source` (a path to a file or a range of memory)
// into `destination`. On success, `last_output_file` is populated with the path
// to the last (or only) file extracted if it is non-null.
template <typename ArchiveSource>
UnPackStatus Unpack(UnPackConsumer consumer,
                    const ArchiveSource& source,
                    const base::FilePath& destination,
                    base::FilePath* last_output_file) {
  // UMA tells us the following about the time required to uncompress the
  // archive from a file as of M75:
  // --- Foreground (<10%) ---
  //   Full archive: 7.5s (50%ile) / 52s (99%ile)
  //   Archive patch: <2s (50%ile) / 10-20s (99%ile)
  // --- Background (>90%) ---
  //   Full archive: 22s (50%ile) / >3m (99%ile)
  //   Archive patch: ~2s (50%ile) / 1.5m - >3m (99%ile)
  //
  // The top unpack failure result with 28 days aggregation (>=0.01%)
  // Setup.Install.LzmaUnPackResult_CompressedChromeArchive
  // 13.50% DISK_FULL
  // 0.67% ERROR_NO_SYSTEM_RESOURCES
  // 0.12% ERROR_IO_DEVICE
  // 0.05% INVALID_HANDLE
  // 0.01% INVALID_LEVEL
  // 0.01% FILE_NOT_FOUND
  // 0.01% LOCK_VIOLATION
  // 0.01% ACCESS_DENIED

  // UMA tells us the following about the time required to unpack from an
  // uncompressed archive file as of M75:
  // --- Foreground ---
  //   <2.7s (50%ile) / 45s (99%ile)
  // --- Background ---
  //   ~14s (50%ile) / >3m (99%ile)
  //
  // The top unpack failure result with 28 days aggregation (>=0.01%)
  // Setup.Install.LzmaUnPackResult_UncompressedChromeArchive
  // 0.66% DISK_FULL
  // 0.04% ACCESS_DENIED
  // 0.01% INVALID_HANDLE
  // 0.01% ERROR_NO_SYSTEM_RESOURCES
  // 0.01% PATH_NOT_FOUND
  // 0.01% ERROR_IO_DEVICE
  UnPackStatus unpack_status = UNPACK_NO_ERROR;
  if constexpr (std::is_same_v<ArchiveSource, base::FilePath>) {
    unpack_status = UnPackArchive(source, destination, last_output_file);
  } else {
    unpack_status = UnPackArchiveBuffer(source, destination, last_output_file);
  }
  // Record success or failure on completion. Note additional calls elsewhere
  // when extracting from a resource (e.g., handling page errors).
  RecordUnPackMetrics(unpack_status, consumer);
  return unpack_status;
}

// Workhorse for unpacking an archive from a loaded mini_installer module.
UnPackStatus UnpackFromMiniInstallerImpl(
    HMODULE module,
    const std::wstring& archive_resource_name,
    const std::wstring& archive_resource_type,
    const base::FilePath& unpack_path,
    base::FilePath* uncompressed_archive) {
  base::span<const uint8_t> archive_span =
      GetResourceSpan(module, archive_resource_name, archive_resource_type);
  const bool is_compressed_archive =
      IsCompressedResourceType(archive_resource_type);
  const UnPackConsumer consumer =
      is_compressed_archive ? UnPackConsumer::COMPRESSED_CHROME_ARCHIVE
                            : UnPackConsumer::UNCOMPRESSED_CHROME_ARCHIVE;
  if (archive_span.empty()) {
    RecordUnPackMetrics(UNPACK_ARCHIVE_NOT_FOUND, consumer);
    return UNPACK_ARCHIVE_NOT_FOUND;
  }

  return Unpack(consumer, archive_span, unpack_path,
                is_compressed_archive ? uncompressed_archive : nullptr);
}

// Calls UnpackFromMiniInstallerImpl within an SEH handler to catch in-page
// errors while accessing the mapped module.
UnPackStatus UnpackFromMiniInstallerWithSEH(
    base::span<const uint8_t> module_span,
    HMODULE module,
    const std::wstring& archive_resource_name,
    const std::wstring& archive_resource_type,
    const base::FilePath& unpack_path,
    base::FilePath* uncompressed_archive) {
  int32_t ntstatus = 0;
  __try {
    return UnpackFromMiniInstallerImpl(module, archive_resource_name,
                                       archive_resource_type, unpack_path,
                                       uncompressed_archive);
  } __except (FilterPageError(module_span, GetExceptionCode(),
                              GetExceptionInformation(), ntstatus)) {
    LOG(ERROR) << "EXCEPTION_IN_PAGE_ERROR while accessing mapped module; "
                  "NTSTATUS = 0x"
               << std::hex << static_cast<uint32_t>(ntstatus);
    RecordUnPackMetrics(UNPACK_IO_DEVICE_ERROR,
                        IsCompressedResourceType(archive_resource_type)
                            ? UnPackConsumer::COMPRESSED_CHROME_ARCHIVE
                            : UnPackConsumer::UNCOMPRESSED_CHROME_ARCHIVE);
    return UNPACK_IO_DEVICE_ERROR;
  }
}

// Extracts the archive resource from `mini_installer_path`. Populates
// `uncompressed_archive` with the path to the uncompressed archive on success
// if `archive_resource_type` is "B7".
UnPackStatus UnpackFromMiniInstaller(const base::FilePath& mini_installer_path,
                                     const std::wstring& archive_resource_name,
                                     const std::wstring& archive_resource_type,
                                     const base::FilePath& unpack_path,
                                     base::FilePath& uncompressed_archive) {
  // --archive-resource-name and --archive-resource-type are required.
  CHECK(!archive_resource_name.empty() && !archive_resource_type.empty());

  const bool is_compressed_archive =
      IsCompressedResourceType(archive_resource_type);
  const UnPackConsumer consumer =
      is_compressed_archive ? UnPackConsumer::COMPRESSED_CHROME_ARCHIVE
                            : UnPackConsumer::UNCOMPRESSED_CHROME_ARCHIVE;

  if (is_compressed_archive) {
    VLOG(1) << "Installing Chrome from compressed archive resource "
            << archive_resource_name;
  } else {
    VLOG(1) << "Installing Chrome from uncompressed archive resource "
            << archive_resource_name;
  }

  // Load mini_installer.exe and unpack the archive resource from it. Since the
  // parent process is this same mini_installer.exe, the file is already mapped
  // as an image for that process. Doing so here reuses the same section object,
  // page cache, etc. Any pages that have already been read by the parent
  // process will be available here without extra I/O.
  base::ScopedNativeLibrary module(::LoadLibraryExW(
      mini_installer_path.value().c_str(), nullptr,
      LOAD_LIBRARY_AS_DATAFILE | LOAD_LIBRARY_AS_IMAGE_RESOURCE));
  if (!module.is_valid()) {
    const auto error = ::GetLastError();
    PLOG(ERROR) << "Failed to load " << mini_installer_path;
    UnPackStatus status =
        (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND)
            ? UNPACK_ARCHIVE_NOT_FOUND
            : UNPACK_ARCHIVE_CANNOT_OPEN;
    RecordUnPackMetrics(status, consumer);
    return status;
  }

  ASSIGN_OR_RETURN(auto module_span, GetModuleSpan(module.get()),
                   [consumer](UnPackStatus status) {
                     RecordUnPackMetrics(status, consumer);
                     return status;
                   });

  return UnpackFromMiniInstallerWithSEH(
      module_span, module.get(), archive_resource_name, archive_resource_type,
      unpack_path, &uncompressed_archive);
}

base::expected<void, InstallStatus> UnpackChromeArchiveImpl(
    const base::FilePath& unpack_path,
    const base::FilePath& setup_exe,
    const base::CommandLine& cmd_line,
    const InstallerState& installer_state) {
  base::FilePath mini_installer_path =
      cmd_line.GetSwitchValuePath(switches::kMiniInstallerPath);
  base::FilePath install_archive =
      cmd_line.GetSwitchValuePath(switches::kInstallArchive);
  base::FilePath uncompressed_archive =
      cmd_line.GetSwitchValuePath(switches::kUncompressedArchive);

  if (!mini_installer_path.empty()) {
    // Mode 1: Resource embedded in mini_installer.exe.
    // --install-archive and --uncompressed-archive are incompatible with
    // --mini-installer-path.
    CHECK(install_archive.empty() && uncompressed_archive.empty());

    std::wstring resource_type =
        cmd_line.GetSwitchValueNative(switches::kArchiveResourceType);
    const bool is_compressed_archive = IsCompressedResourceType(resource_type);
    installer_state.SetStage(is_compressed_archive ? UNCOMPRESSING : UNPACKING);

    if (UnpackFromMiniInstaller(
            mini_installer_path,
            cmd_line.GetSwitchValueNative(switches::kArchiveResourceName),
            resource_type, unpack_path,
            uncompressed_archive) != UNPACK_NO_ERROR) {
      return base::unexpected(is_compressed_archive ? UNCOMPRESSION_FAILED
                                                    : UNPACKING_FAILED);
    }

    if (uncompressed_archive.empty()) {
      if (is_compressed_archive) {
        LOG(ERROR) << "Failed to uncompress an archive from resource "
                   << cmd_line.GetSwitchValueNative(
                          switches::kArchiveResourceName)
                   << " in file " << mini_installer_path;
        return base::unexpected(INVALID_ARCHIVE);
      }
      return base::ok();  // Directly unpacked uncompressed resource.
    }
  } else {
    // Mode 2: Archive files on disk.
    // At most one of --install-archive and --uncompressed-archive may be
    // provided.
    CHECK(install_archive.empty() || uncompressed_archive.empty());

    if (!install_archive.empty()) {
      // --install-archive was given: uncompress then unpack.
      installer_state.SetStage(UNCOMPRESSING);

      VLOG(1) << "Installing Chrome from compressed archive "
              << install_archive;
      if (Unpack(UnPackConsumer::COMPRESSED_CHROME_ARCHIVE, install_archive,
                 unpack_path, &uncompressed_archive) != UNPACK_NO_ERROR) {
        return base::unexpected(UNCOMPRESSION_FAILED);
      }
      if (uncompressed_archive.empty()) {
        LOG(ERROR) << "Failed to uncompress an archive from "
                   << install_archive;
        return base::unexpected(INVALID_ARCHIVE);
      }
    } else if (uncompressed_archive.empty()) {
      // Neither --install-archive nor --uncompressed-archive was given. Try
      // unpacking chrome.7z next to this executable.
      installer_state.SetStage(UNPACKING);
      UnPackStatus status = Unpack(UnPackConsumer::UNCOMPRESSED_CHROME_ARCHIVE,
                                   setup_exe.DirName().Append(kChromeArchive),
                                   unpack_path, nullptr);
      if (status == UNPACK_NO_ERROR) {
        return base::ok();  // Success.
      }
      if (status != UNPACK_ARCHIVE_NOT_FOUND) {
        return base::unexpected(UNPACKING_FAILED);
      }
    }  // else --uncompressed-archive was given.

    if (uncompressed_archive.empty()) {
      // Neither --install-archive nor --uncompressed-archive was given and
      // chrome.7z wasn't found. Try uncompressing chrome.packed.7z next to this
      // executable.
      installer_state.SetStage(UNCOMPRESSING);
      if (Unpack(UnPackConsumer::COMPRESSED_CHROME_ARCHIVE,
                 setup_exe.DirName().Append(kChromeCompressedArchive),
                 unpack_path, &uncompressed_archive) != UNPACK_NO_ERROR) {
        return base::unexpected(UNCOMPRESSION_FAILED);
      }
      if (uncompressed_archive.empty()) {
        LOG(ERROR) << "Failed to uncompress an archive from "
                   << setup_exe.DirName().Append(kChromeCompressedArchive);
        return base::unexpected(INVALID_ARCHIVE);
      }
    }
  }

  if (uncompressed_archive.empty()) {
    LOG(ERROR) << "Cannot install Chrome without an uncompressed archive.";
    return base::unexpected(INVALID_ARCHIVE);
  }

  installer_state.SetStage(UNPACKING);
  if (Unpack(UnPackConsumer::UNCOMPRESSED_CHROME_ARCHIVE, uncompressed_archive,
             unpack_path, nullptr) != UNPACK_NO_ERROR) {
    return base::unexpected(UNPACKING_FAILED);
  }

  return base::ok();
}

}  // namespace

base::expected<void, InstallStatus> UnpackChromeArchive(
    const base::FilePath& unpack_path,
    const base::FilePath& setup_exe,
    const base::CommandLine& cmd_line,
    const InstallerState& installer_state) {
  RETURN_IF_ERROR(UnpackChromeArchiveImpl(unpack_path, setup_exe, cmd_line,
                                          installer_state),
                  [&installer_state](InstallStatus install_status) {
                    installer_state.WriteInstallerResult(
                        install_status,
                        install_status == INVALID_ARCHIVE
                            ? IDS_INSTALL_INVALID_ARCHIVE_BASE
                            : IDS_INSTALL_UNCOMPRESSION_FAILED_BASE,
                        nullptr);
                    return install_status;
                  });
  return base::ok();
}

}  // namespace installer
