// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/media/cdm_host_file_path.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/stl_util.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_version.h"

#if defined(OS_MACOSX)
#include "base/mac/bundle_locations.h"
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

#if defined(OS_WIN)

  static const base::FilePath::CharType* const kUnversionedFiles[] = {
      chrome::kBrowserProcessExecutableName};

  static const base::FilePath::CharType* const kVersionedFiles[] = {
#if defined(CHROME_MULTIPLE_DLL)
    chrome::kBrowserResourcesDll,
    chrome::kChildDll
#else
    chrome::kBrowserResourcesDll
#endif  // defined(CHROME_MULTIPLE_DLL)
  };

  // Find where chrome.exe is installed.
  base::FilePath chrome_exe_dir;
  if (!base::PathService::Get(base::DIR_EXE, &chrome_exe_dir))
    NOTREACHED();
  base::FilePath version_dir(chrome_exe_dir.AppendASCII(CHROME_VERSION_STRING));

  cdm_host_file_paths->reserve(base::size(kUnversionedFiles) +
                               base::size(kVersionedFiles));

  // Signature files are always in the version directory.
  for (size_t i = 0; i < base::size(kUnversionedFiles); ++i) {
    base::FilePath file_path = chrome_exe_dir.Append(kUnversionedFiles[i]);
    base::FilePath sig_path =
        GetSigFilePath(version_dir.Append(kUnversionedFiles[i]));
    DVLOG(2) << __func__ << ": unversioned file " << i << " at "
             << file_path.value() << ", signature file " << sig_path.value();
    cdm_host_file_paths->emplace_back(file_path, sig_path);
  }

  for (size_t i = 0; i < base::size(kVersionedFiles); ++i) {
    base::FilePath file_path = version_dir.Append(kVersionedFiles[i]);
    DVLOG(2) << __func__ << ": versioned file " << i << " at "
             << file_path.value();
    cdm_host_file_paths->emplace_back(file_path, GetSigFilePath(file_path));
  }

#elif defined(OS_MACOSX)

  base::FilePath framework_dir = base::mac::FrameworkBundlePath();
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

#elif defined(OS_LINUX)

  base::FilePath chrome_exe_dir;
  if (!base::PathService::Get(base::DIR_EXE, &chrome_exe_dir))
    NOTREACHED();

  base::FilePath chrome_path =
      chrome_exe_dir.Append(FILE_PATH_LITERAL("chrome"));
  DVLOG(2) << __func__ << ": chrome_path=" << chrome_path.value();
  cdm_host_file_paths->emplace_back(chrome_path, GetSigFilePath(chrome_path));

#endif  // defined(OS_WIN)
}

#else  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

void AddCdmHostFilePaths(
    std::vector<media::CdmHostFilePath>* cdm_host_file_paths) {
  NOTIMPLEMENTED() << "CDM host file paths need to be provided for the CDM to "
                      "verify the host.";
}

#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
