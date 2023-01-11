// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/crash/linux/minidump_writer.h"

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "chromecast/base/path_utils.h"
#include "chromecast/base/process_utils.h"
#include "chromecast/crash/linux/crash_util.h"
#include "chromecast/crash/linux/dump_info.h"
#include "chromecast/crash/linux/minidump_generator.h"

namespace chromecast {

namespace {

const char kDumpStateSuffix[] = ".txt.gz";

// Fork and run dumpstate, saving results to minidump_name + ".txt.gz".
int DumpState(const std::string& minidump_name) {
  base::FilePath dumpstate_path;
  if (!CrashUtil::CollectDumpstate(base::FilePath(minidump_name),
                                   &dumpstate_path)) {
    return -1;
  }
  return 0;
}

}  // namespace

MinidumpWriter::MinidumpWriter(MinidumpGenerator* minidump_generator,
                               const std::string& minidump_filename,
                               const MinidumpParams& params,
                               DumpStateCallback dump_state_cb,
                               const std::vector<Attachment>* attachments)
    : minidump_generator_(minidump_generator),
      minidump_path_(minidump_filename),
      params_(params),
      attachments_(attachments),
      dump_state_cb_(std::move(dump_state_cb)) {}

MinidumpWriter::MinidumpWriter(MinidumpGenerator* minidump_generator,
                               const std::string& minidump_filename,
                               const MinidumpParams& params,
                               const std::vector<Attachment>* attachments)
    : MinidumpWriter(minidump_generator,
                     minidump_filename,
                     params,
                     base::BindOnce(&DumpState),
                     attachments) {}

MinidumpWriter::~MinidumpWriter() {}

bool MinidumpWriter::DoWork() {
  // If path is not absolute, append it to |dump_path_|.
  if (!minidump_path_.value().empty() && minidump_path_.value()[0] != '/')
    minidump_path_ = dump_path_.Append(minidump_path_);

  // The path should be a file in the |dump_path_| directory.
  if (dump_path_ != minidump_path_.DirName()) {
    LOG(INFO) << "The absolute path: " << minidump_path_.value() << " is not"
              << "in the correct directory: " << dump_path_.value();
    return false;
  }

  // Generate a minidump at the specified |minidump_path_|.
  if (!minidump_generator_->Generate(minidump_path_.value())) {
    LOG(ERROR) << "Generate minidump failed " << minidump_path_.value();
    return false;
  }

  // Run the dumpstate callback.
  DCHECK(dump_state_cb_);
  std::string dumpstate_path;
  if (std::move(dump_state_cb_).Run(minidump_path_.value()) < 0) {
    LOG(ERROR) << "DumpState callback failed.";
  } else {
    dumpstate_path = minidump_path_.value() + kDumpStateSuffix;
  }

  // Add attachments to dumpinfo and copy the temporary attachments to the dump
  // path.
  std::unique_ptr<std::vector<std::string>> attachment_files;
  if (attachments_) {
    attachment_files = std::make_unique<std::vector<std::string>>();
    for (auto& attachment : *attachments_) {
      base::FilePath attachment_path(attachment.file_path);
      if (attachment.is_static || dump_path_ == attachment_path.DirName()) {
        attachment_files->push_back(attachment.file_path);
        continue;
      }

      base::FilePath temporary_path =
          dump_path_.Append(attachment_path.BaseName());
      if (!base::CopyFile(attachment_path, temporary_path)) {
        LOG(WARNING) << "Could not copy attachment " << attachment_path.value()
                     << " to " << temporary_path.value();
      } else {
        attachment_files->push_back(temporary_path.value());
      }
    }
  }

  // Add this entry to the lockfile.
  const DumpInfo info(minidump_path_.value(), dumpstate_path, base::Time::Now(),
                      params_, attachment_files.get());
  if (!AddEntryToLockFile(info)) {
    LOG(ERROR) << "lockfile logging failed";
    return false;
  }

  return true;
}

}  // namespace chromecast
