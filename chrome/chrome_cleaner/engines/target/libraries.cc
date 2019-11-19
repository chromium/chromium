// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/engines/target/libraries.h"

#include <windows.h>

#include <delayimp.h>
#include <stdint.h>
#include <string.h>

#include <map>
#include <unordered_map>
#include <utility>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/native_library.h"
#include "base/strings/string16.h"
#include "base/strings/string_piece.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/chrome_cleaner/buildflags.h"
#include "chrome/chrome_cleaner/engines/common/engine_resources.h"
#include "chrome/chrome_cleaner/os/digest_verifier.h"
#include "chrome/chrome_cleaner/os/file_path_sanitization.h"
#include "chrome/chrome_cleaner/os/resource_util.h"
#include "chrome/chrome_cleaner/os/system_util.h"
#include "chrome/chrome_cleaner/settings/settings.h"
#include "sandbox/win/src/sandbox_factory.h"

namespace chrome_cleaner {

namespace {

internal::LibraryPostExtractionCallback* g_post_extraction_callback = nullptr;

// Extracts engine DLLs from the embedded resources and writes them on disk.
// Should be called before any call to an engine library.
bool ExtractEmbeddedLibraries(Engine::Name engine,
                              const base::FilePath& extraction_dir) {
  std::unordered_map<base::string16, int> resource_names_to_id =
      GetEmbeddedLibraryResourceIds(engine);
  for (const auto& name_id : resource_names_to_id) {
    LOG(INFO) << "Extracting " << name_id.first << " to "
              << extraction_dir.value();
    base::StringPiece library_data;
    bool success =
        LoadResourceOfKind(name_id.second, L"LIBRARY", &library_data);
    if (!success) {
      LOG(ERROR) << "Failed to load " << name_id.first << " from resources";
      return false;
    }
    if (base::WriteFile(extraction_dir.Append(name_id.first),
                        library_data.data(), library_data.size()) < 0) {
      PLOG(ERROR) << "Failed to write " << name_id.first;
      return false;
    }
  }

  if (g_post_extraction_callback)
    g_post_extraction_callback->Run(extraction_dir);

  return true;
}

void VerifyEngineLibraryAllowed(Engine::Name engine,
                                const base::string16& requested_library) {
#if !BUILDFLAG(IS_OFFICIAL_CHROME_CLEANER_BUILD)
  if (Settings::GetInstance()->run_without_sandbox_for_testing())
    return;
#endif

  if (GetLibrariesToLoad(engine).count(requested_library) &&
      !sandbox::SandboxFactory::GetTargetServices()) {
    // Deny the load.
    LOG(FATAL) << "Aborting due to attempt to load " << requested_library
               << " outside sandbox";
  }
}

void VerifyRunningInSandbox() {
#if !BUILDFLAG(IS_OFFICIAL_CHROME_CLEANER_BUILD)
  if (Settings::GetInstance()->run_without_sandbox_for_testing())
    return;
#endif

  CHECK(sandbox::SandboxFactory::GetTargetServices())
      << "Attempt to load engine libraries outside of the sandbox process";
}

extern "C" FARPROC WINAPI DllLoadHook(unsigned dliNotify, PDelayLoadInfo pdli) {
  switch (dliNotify) {
    case dliNotePreLoadLibrary: {
      const base::string16 requested_library = base::ASCIIToUTF16(pdli->szDll);
      const Engine::Name engine = Settings::GetInstance()->engine();

      VerifyEngineLibraryAllowed(engine, requested_library);

#if !BUILDFLAG(IS_OFFICIAL_CHROME_CLEANER_BUILD)
      const std::unordered_map<base::string16, base::string16>
          library_replacements = GetLibraryTestReplacements(engine);
      if (library_replacements.count(requested_library)) {
        // Try loading the original DLL first, then try the replacement.
        HMODULE library = ::LoadLibrary(requested_library.c_str());
        if (library == nullptr) {
          const base::string16& fallback_library =
              library_replacements.find(requested_library)->second;
          PLOG(WARNING) << "Could not load " << requested_library
                        << "; falling back to " << fallback_library;
          library = ::LoadLibrary(fallback_library.c_str());
          PLOG_IF(FATAL, library == nullptr)
              << "Failed to load " << fallback_library;
        }
        return reinterpret_cast<FARPROC>(library);
      }
#endif
      return nullptr;
    }
    default:
      return nullptr;
  }
}

extern "C" const PfnDliHook __pfnDliNotifyHook2 = DllLoadHook;

}  // namespace

namespace internal {

void SetLibraryPostExtractionCallbackForTesting(
    LibraryPostExtractionCallback post_extraction_callback) {
  ClearLibraryPostExtractionCallbackForTesting();
  g_post_extraction_callback = new LibraryPostExtractionCallback();
  *g_post_extraction_callback = std::move(post_extraction_callback);
}

void ClearLibraryPostExtractionCallbackForTesting() {
  delete g_post_extraction_callback;
  g_post_extraction_callback = nullptr;
}

}  // namespace internal

bool LoadAndValidateLibraries(Engine::Name engine,
                              const base::FilePath& extraction_dir) {
  // This function is invoked in the sandbox target process before LowerToken is
  // called, so the process will have read access to the DLL files. For
  // libraries using DELAYLOAD, DllLoadHook will also be called some time later
  // (when the first attempt is made to access a symbol defined in the library.)
  // This should happen after LowerToken.
  //
  // Calling LoadLibrary here will just load the library into memory, but not
  // update the DELAYLOAD thunks to point to its symbols, so it won't be used
  // for anything. But it does mean that DllLoadHook will succeed in "loading"
  // the library even if the sandbox would usually block access (because it
  // just gets another handle to the library already in memory.) DllLoadHooks
  // will then update the thunks so that the library is actually used.
  //
  // Some engine libraries are dynamically loaded by the engine, not through
  // DELAYLOAD. Again calling LoadLibrary here will allow the engine to access
  // them despite the sandbox. Also it will mark the file in use, ensuring that
  // the version that is validated here is not overwritten before it is loaded.
  VerifyRunningInSandbox();

  const std::set<base::string16> libraries_to_load = GetLibrariesToLoad(engine);
  if (libraries_to_load.empty())
    return true;

  // Proceed even if the library extraction fails. It happens to the elevated
  // cleaner process as it is executed in the same directory as the non-elevated
  // one, and the directory already has libraries on the disk. If the libraries
  // are corrupted, DigestVerifier will catch it below.
  ExtractEmbeddedLibraries(engine, extraction_dir);

  // If modules might be replaced for testing, disable validation and do not
  // fail on loading errors. The real DLL's may not be present, so don't abort
  // just because they can't be loaded.
  const bool enable_validation = GetLibraryTestReplacements(engine).empty();

#if BUILDFLAG(IS_OFFICIAL_CHROME_CLEANER_BUILD)
  CHECK(enable_validation)
      << "Library validation should not be disabled in official build";
#endif

  scoped_refptr<DigestVerifier> digest_verifier =
      DigestVerifier::CreateFromResource(GetLibrariesDigestResourcesId(engine));
  CHECK(digest_verifier);

  // Load all libraries and validate them if required.
  for (const base::string16& library_name : libraries_to_load) {
    base::FilePath dll_path = extraction_dir.Append(library_name);

    // Open a handle to the DLL before verifying it.
    base::NativeLibraryLoadError error;
    base::LoadNativeLibrary(dll_path, &error);  // Return value leaks.
    if (error.code && enable_validation) {
      LOG(ERROR) << "Error loading library " << SanitizePath(dll_path) << ": "
                 << error.ToString();
      return false;
    }

    if (enable_validation && !digest_verifier->IsKnownFile(dll_path)) {
      LOG(ERROR) << SanitizePath(dll_path) << " does not have a valid digest";
      return false;
    }
  }

  return true;
}

}  // namespace chrome_cleaner
