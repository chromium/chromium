// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/media/cdm_host_file_path.h"

#include "base/check.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/path_service.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_version.h"

#if BUILDFLAG(IS_MAC)
#include "base/apple/bundle_locations.h"
#endif

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)

namespace {

// TODO(xhwang): Move this to a common place if needed.
const base::FilePath::CharType kSignatureFileExtension[] =
    FILE_PATH_LITERAL(".sig");

// Returns the signature file path given the |file_path|. This function should
// only be used when the signature file and the file are located in the same
// directory.
base::FilePath GetSigFilePath(const base::FilePath& file_path) {
  return file_path.AddExtension(kSignatureFileExtension);
}

}  // namespace

void AddCdmHostFilePaths(
    std::vector<media::CdmHostFilePath>* cdm_host_file_paths) {
  DVLOG(1) << __func__;
  DCHECK(cdm_host_file_paths);
  DCHECK(cdm_host_file_paths->empty());

#if BUILDFLAG(IS_WIN)

  // Find where kBrowserProcessExecutableName is installed. Signature file is
  // in the assets directory. FILE_EXE may not be kBrowserProcessExecutableName,
  // e.g. browser_tests.exe, which is fine since we don't verify those
  // signatures in tests.
  base::FilePath dir_exe;
  CHECK(base::PathService::Get(base::DIR_EXE, &dir_exe));
  base::FilePath chrome_exe =
      dir_exe.Append(chrome::kBrowserProcessExecutableName);

  // Signature files and kBrowserResourcesDll are typically in a
  // separate versioned directory, but may be the same directory as
  // kBrowserProcessExecutableName (e.g. for a local build of Chrome).
  // DIR_ASSETS sorts this out for us.
  base::FilePath chrome_assets_dir;
  CHECK(base::PathService::Get(base::DIR_ASSETS, &chrome_assets_dir));
  const auto chrome_exe_sig = GetSigFilePath(
      chrome_assets_dir.Append(chrome::kBrowserProcessExecutableName));
  DVLOG(2) << __func__ << ":" << chrome_exe.value() << ", signature file "
           << chrome_exe_sig.value();
  cdm_host_file_paths->emplace_back(chrome_exe, chrome_exe_sig);

  // kBrowserResourcesDll and it's signature file are in the assets directory.
  const auto chrome_dll =
      chrome_assets_dir.Append(chrome::kBrowserResourcesDll);
  const auto chrome_dll_sig = GetSigFilePath(chrome_dll);
  DVLOG(2) << __func__ << ":" << chrome_dll.value() << ", signature file "
           << chrome_dll_sig.value();
  cdm_host_file_paths->emplace_back(chrome_dll, chrome_dll_sig);

#elif BUILDFLAG(IS_MAC)

  base::FilePath framework_dir = base::apple::FrameworkBundlePath();
  base::FilePath chrome_framework_path =
      framework_dir.Append(chrome::kFrameworkExecutableName);
  // The signature file lives inside
  // Google Chrome Framework.framework/Versions/X/Resources/.
  base::FilePath widevine_signature_path = framework_dir.Append("Resources");
  base::FilePath chrome_framework_sig_path = GetSigFilePath(
      widevine_signature_path.Append(chrome::kFrameworkExecutableName));

  DVLOG(2) << __func__
           << ": chrome_framework_path=" << chrome_framework_path.value()
           << ", signature_path=" << chrome_framework_sig_path.value();
  cdm_host_file_paths->emplace_back(chrome_framework_path,
                                    chrome_framework_sig_path);

#elif BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

  base::FilePath chrome_exe_dir;
  if (!base::PathService::Get(base::DIR_EXE, &chrome_exe_dir))
    NOTREACHED_IN_MIGRATION();

  base::FilePath chrome_path =
      chrome_exe_dir.Append(FILE_PATH_LITERAL("chrome"));
  DVLOG(2) << __func__ << ": chrome_path=" << chrome_path.value();
  cdm_host_file_paths->emplace_back(chrome_path, GetSigFilePath(chrome_path));

#endif  // BUILDFLAG(IS_WIN)
}

#else  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

void AddCdmHostFilePaths(
    std::vector<media::CdmHostFilePath>* cdm_host_file_paths) {
  NOTIMPLEMENTED() << "CDM host file paths need to be provided for the CDM to "
                      "verify the host.";
}

#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
