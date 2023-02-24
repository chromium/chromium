// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Simple tool that uses the SignatureUtil class to extract signature
// information from an executable.  The output is an encoded
// ClientDownloadRequest_SignatureInfo protocol buffer.
//
// Example usage: sb_sigutil --executable=blah.exe --output=siginfo.pb

#include <string>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "chrome/common/safe_browsing/binary_feature_extractor.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"

// Command-line switch for the executable to extract a signature from.
static const char kExecutable[] = "executable";

// File to write the output protocol buffer to.
static const char kOutputFile[] = "output";

int main(int argc, char* argv[]) {
  base::CommandLine::Init(argc, argv);

  base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
  if (!cmd_line->HasSwitch(kExecutable)) {
    LOG(ERROR) << "Must specify executable to open with --executable";
    return 1;
  }
  if (!cmd_line->HasSwitch(kOutputFile)) {
    LOG(ERROR) << "Must specify output file with --output";
    return 1;
  }

  scoped_refptr<safe_browsing::BinaryFeatureExtractor> extractor(
      new safe_browsing::BinaryFeatureExtractor());
  safe_browsing::ClientDownloadRequest_SignatureInfo signature_info;
  extractor->CheckSignature(cmd_line->GetSwitchValuePath(kExecutable),
                            &signature_info);

  std::string serialized_info = signature_info.SerializeAsString();
  if (!base::WriteFile(cmd_line->GetSwitchValuePath(kOutputFile),
                       serialized_info)) {
    LOG(ERROR) << "Error writing output file";
    return 1;
  }

  return 0;
}
