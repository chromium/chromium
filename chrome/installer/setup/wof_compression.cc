// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/setup/wof_compression.h"

#include <windows.h>

#include <wofapi.h>

#include <string>

#include "base/files/file.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/native_library.h"
#include "base/strings/string_util.h"
#include "base/win/registry.h"
#include "chrome/installer/util/callback_work_item.h"
#include "chrome/installer/util/work_item.h"
#include "chrome/installer/util/work_item_list.h"

namespace installer {

namespace {

// The wofutil.dll entry points used here, or null if the OS does not ship the
// library. Resolved dynamically so that setup.exe does not take a load-time
// dependency on a library that some SKUs omit.
struct WofApi {
  decltype(&::WofIsExternalFile) is_external_file = nullptr;
  decltype(&::WofSetFileDataLocation) set_file_data_location = nullptr;
};

const WofApi& GetWofApi() {
  static const WofApi api = [] {
    WofApi api;
    if (HMODULE module = base::PinSystemLibrary(L"wofutil.dll")) {
      api.is_external_file = reinterpret_cast<decltype(api.is_external_file)>(
          ::GetProcAddress(module, "WofIsExternalFile"));
      api.set_file_data_location =
          reinterpret_cast<decltype(api.set_file_data_location)>(
              ::GetProcAddress(module, "WofSetFileDataLocation"));
    }
    return api;
  }();
  return api;
}

// Returns true if the OS ships wofutil.dll and the entry points used here
// resolved. Checked once before any work is queued, so the operations below
// can call through the pointers without testing them again.
bool IsWofAvailable() {
  const WofApi& api = GetWofApi();
  return api.is_external_file && api.set_file_data_location;
}

// Returns true if `hr` means that WOF left the file alone for a legitimate
// reason rather than because something went wrong. WOF reports a failure when
// compressing a file would not shrink it, which is a normal outcome.
bool IsBenignResult(HRESULT hr) {
  // Some down level kernels do not translate the error, hence the second check.
  return SUCCEEDED(hr) ||
         hr == HRESULT_FROM_WIN32(ERROR_COMPRESSION_NOT_BENEFICIAL) ||
         hr == HRESULT_FROM_WIN32(ERROR_MR_MID_NOT_FOUND);
}

// Returns true if WOF, rather than the file's own data stream, holds the data
// for `path`.
bool IsExternalFile(const base::FilePath& path) {
  BOOL is_external = FALSE;
  ULONG provider = 0;
  return SUCCEEDED(GetWofApi().is_external_file(path.value().c_str(),
                                                &is_external, &provider,
                                                nullptr, nullptr)) &&
         is_external && provider == WOF_PROVIDER_FILE;
}

// Hands `path` to WOF for LZX compression. Returns true if the file's data is
// now held by WOF or if WOF declined to compress it.
bool WofCompressFile(const base::FilePath& path) {
  // WOF opens the file itself, so this handle only serves to name it. Ask for
  // no access rights at all, which cannot lose a sharing race.
  base::File file(path, base::File::FLAG_OPEN);
  if (!file.IsValid()) {
    return false;
  }

  WOF_FILE_COMPRESSION_INFO_V1 info = {};
  info.Algorithm = FILE_PROVIDER_COMPRESSION_LZX;
  info.Flags = 0;
  return IsBenignResult(GetWofApi().set_file_data_location(
      file.GetPlatformFile(), WOF_PROVIDER_FILE, &info, sizeof(info)));
}

bool CompressLocalePaks(const base::FilePath& version_dir,
                        const CallbackWorkItem&) {
  // The subdirectory of a version directory holding the locale packs, and the
  // pattern matching all of them.
  static constexpr base::FilePath::CharType kLocalesDir[] =
      FILE_PATH_LITERAL("Locales");
  static constexpr base::FilePath::CharType kPakPattern[] =
      FILE_PATH_LITERAL("*.pak");

  base::FileEnumerator paks(version_dir.Append(kLocalesDir),
                            /*recursive=*/false, base::FileEnumerator::FILES,
                            kPakPattern);
  for (base::FilePath pak = paks.Next(); !pak.empty(); pak = paks.Next()) {
    // Repairs and overinstalls can find the packs already compressed, in which
    // case there is nothing to do.
    if (!IsExternalFile(pak)) {
      WofCompressFile(pak);
    }
  }
  return true;
}

// Whether setup scheduled compression of the locale packs and, if it did not,
// why. These values are persisted to logs. Entries should not be renumbered
// and numeric values should never be reused.
// LINT.IfChange(WofLocalePakCompressionDecision)
enum class WofLocalePakCompressionDecision {
  // The compression work item is best effort, so this does not mean that
  // compression succeeded.
  kScheduled = 0,
  kWindowsSetupIncomplete = 1,
  kImageStateMissing = 2,
  kImageStateUnreadable = 3,
  kMaxValue = kImageStateUnreadable,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/installer/enums.xml:WofLocalePakCompressionDecision)

void RecordDecision(WofLocalePakCompressionDecision decision) {
  base::UmaHistogramEnumeration("Setup.Install.WofLocalePakCompressionDecision",
                                decision);
}

// Returns true if Windows setup, OOBE included, has finished on this machine,
// and so setup may compress the locale packs it installs. Records the decision.
bool ShouldCompressLocalePaks() {
  // Where Windows records how far it has got through setting up the machine,
  // and the value it holds there once setup, OOBE included, has finished.
  static constexpr wchar_t kSetupStateKey[] =
      L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Setup\\State";
  static constexpr wchar_t kImageStateValue[] = L"ImageState";
  static constexpr wchar_t kImageStateComplete[] = L"IMAGE_STATE_COMPLETE";

  std::wstring image_state;
  base::win::RegKey key;
  // The value lives in the native registry view, which 32-bit setup on 64-bit
  // Windows does not see by default.
  LONG result = key.Open(HKEY_LOCAL_MACHINE, kSetupStateKey,
                         KEY_QUERY_VALUE | KEY_WOW64_64KEY);
  if (result == ERROR_SUCCESS) {
    result = key.ReadValue(kImageStateValue, &image_state);
  }

  // Without the key or value there is nothing to say that Windows setup has
  // finished, so the packs are left alone. That is recorded apart from a
  // machine that reports setup still running: were there a configuration that
  // never writes the value, it would never compress, and it should not be
  // mistaken for machines genuinely in audit mode or OOBE.
  if (result == ERROR_FILE_NOT_FOUND) {
    VLOG(1) << "Not compressing locale packs; the machine has no ImageState.";
    RecordDecision(WofLocalePakCompressionDecision::kImageStateMissing);
    return false;
  }

  // Any other error is reported apart as well: no machine should hit one, and
  // a regression that stopped setup reading the value would otherwise silently
  // turn compression off everywhere.
  if (result != ERROR_SUCCESS) {
    LOG(ERROR) << "Not compressing locale packs; failed to read ImageState: "
               << ::logging::SystemErrorCodeToString(result);
    base::UmaHistogramSparse("Setup.Install.WofLocalePakImageStateError",
                             result);
    RecordDecision(WofLocalePakCompressionDecision::kImageStateUnreadable);
    return false;
  }

  // Windows reports IMAGE_STATE_COMPLETE only once OOBE has finished on a boot
  // that is not into audit mode.
  if (!base::EqualsCaseInsensitiveASCII(image_state, kImageStateComplete)) {
    VLOG(1) << "Not compressing locale packs; Windows setup has not finished"
            << " (ImageState \"" << image_state << "\").";
    RecordDecision(WofLocalePakCompressionDecision::kWindowsSetupIncomplete);
    return false;
  }

  RecordDecision(WofLocalePakCompressionDecision::kScheduled);
  return true;
}

}  // namespace

bool IsFileWofCompressed(const base::FilePath& path) {
  return IsWofAvailable() && IsExternalFile(path);
}

void AddWofCompressionWorkItems(const base::FilePath& version_dir,
                                WorkItemList* list) {
  // Some SKUs do not ship wofutil.dll, in which case there is nothing to do.
  if (!IsWofAvailable()) {
    return;
  }

  if (!ShouldCompressLocalePaks()) {
    return;
  }

  WorkItem* item = list->AddCallbackWorkItem(
      base::BindOnce(&CompressLocalePaks, version_dir), base::DoNothing());
  item->set_best_effort(true);
  item->set_rollback_enabled(false);
}

}  // namespace installer
