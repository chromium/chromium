// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MINIDUMP_UPLOADER_REWRITE_MINIDUMPS_AS_MIMES_H_
#define COMPONENTS_MINIDUMP_UPLOADER_REWRITE_MINIDUMPS_AS_MIMES_H_

#include <sys/types.h>

#include "third_party/crashpad/crashpad/client/crash_report_database.h"

namespace crashpad {
class HTTPMultipartBuilder;
class HTTPBodyStream;
class FileWriterInterface;
}  // namespace crashpad

namespace minidump_uploader {

// Re-encodes report as a MIME and places the result in http_multipart_builder.
// pid is set to the process ID extracted from the report. Returns `true` on
// success.
bool MimeifyReport(const crashpad::CrashReportDatabase::UploadReport& report,
                   crashpad::HTTPMultipartBuilder* http_multipart_builder,
                   pid_t* pid);

// Consumes all bytes from body and writes them to writer. Returns `true` on
// success.
bool WriteBodyToFile(crashpad::HTTPBodyStream* body,
                     crashpad::FileWriterInterface* writer);

void WriteAnrAsMime(crashpad::FileReader* anr_reader,
                    crashpad::FileWriterInterface* writer,
                    const std::string& version_number,
                    const std::string& build_id,
                    const std::string& anr_file_name);
}  // namespace minidump_uploader

#endif  // COMPONENTS_MINIDUMP_UPLOADER_REWRITE_MINIDUMPS_AS_MIMES_H_
