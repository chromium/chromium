// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/safe_browsing/rar_analyzer.h"

#include <memory>
#include <string>
#include "base/files/file_path.h"
#include "base/i18n/streaming_utf8_validator.h"
#include "base/metrics/histogram_macros.h"
#include "build/build_config.h"
#include "chrome/common/safe_browsing/archive_analyzer_results.h"
#include "chrome/common/safe_browsing/download_type_util.h"
#include "chrome/common/safe_browsing/file_type_policies.h"
#include "third_party/unrar/src/unrar_wrapper.h"

namespace safe_browsing {
namespace rar_analyzer {

void AnalyzeRarFile(base::File rar_file,
                    ArchiveAnalyzerResults* results) {
  auto archive = std::make_unique<third_party_unrar::Archive>();
  archive->SetFileHandle(rar_file.GetPlatformFile());

  bool open_success = archive->Open(L"dummy.rar");
  UMA_HISTOGRAM_BOOLEAN("SBClientDownload.RarOpenSuccess", open_success);
  if (!open_success) {
    results->success = false;
    DLOG(ERROR) << __FUNCTION__
                << ": Unable to open rar_file: " << rar_file.GetPlatformFile();
    return;
  }

  bool is_valid_archive = archive->IsArchive(/*EnableBroken=*/true);
  UMA_HISTOGRAM_BOOLEAN("SBClientDownload.RarValidArchive", is_valid_archive);
  if (!is_valid_archive) {
    results->success = false;
    DLOG(ERROR) << __FUNCTION__
                << ": !IsArchive: rar_file: " << rar_file.GetPlatformFile();
    return;
  }

  results->success = true;
  std::set<base::FilePath> archived_archive_filenames;
  for (archive->ViewComment();
       archive->ReadHeader() > 0 &&
       archive->GetHeaderType() != third_party_unrar::kUnrarEndarcHead;
       archive->SeekToNext()) {
    std::wstring wide_filename(archive->FileHead.FileName);
#if defined(OS_WIN)
    base::FilePath file_path(wide_filename);
#else
    std::string filename(wide_filename.begin(), wide_filename.end());
    base::FilePath file_path(filename);
#endif  // OS_WIN

    bool is_executable =
        FileTypePolicies::GetInstance()->IsCheckedBinaryFile(file_path);
    bool is_archive = FileTypePolicies::GetInstance()->IsArchiveFile(file_path);
    int64 unpacked_size =
        archive->FileHead.UnpSize;  // Read from header, may not be accurate.
    // TODO(vakh): Log UMA if |unpacked_size| < 0.

    base::FilePath basename = file_path.BaseName();
    std::string basename_utf8(basename.AsUTF8Unsafe());
    bool is_utf8_valid_basename =
        base::StreamingUtf8Validator::Validate(basename_utf8);

    if (is_archive) {
      results->has_archive = true;
      archived_archive_filenames.insert(basename);
      ClientDownloadRequest::ArchivedBinary* archived_archive =
          results->archived_binary.Add();
      if (is_utf8_valid_basename)
        archived_archive->set_file_basename(basename_utf8);
      archived_archive->set_download_type(ClientDownloadRequest::ARCHIVE);
      archived_archive->set_length(unpacked_size);
    } else if (is_executable) {
      results->has_executable = true;
      ClientDownloadRequest::ArchivedBinary* archived_binary =
          results->archived_binary.Add();
      if (is_utf8_valid_basename)
        archived_binary->set_file_basename(basename_utf8);
      archived_binary->set_download_type(
          download_type_util::GetDownloadType(file_path));
      archived_binary->set_length(unpacked_size);
    }
    results->archived_archive_filenames.assign(
        archived_archive_filenames.begin(), archived_archive_filenames.end());
  }
}

}  // namespace rar_analyzer
}  // namespace safe_browsing
