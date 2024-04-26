// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/base/process_utils.h"

#include <errno.h>
#include <stddef.h>
#include <stdio.h>

#include "base/logging.h"
#include "base/posix/safe_strerror.h"
#include "base/strings/string_util.h"

namespace chromecast {

bool GetAppOutput(const std::vector<std::string>& argv, std::string* output) {
  DCHECK(output);

  // Join the args into one string, creating the command.
  std::string command = base::JoinString(argv, " ");

  // Open the process.
  FILE* fp = popen(command.c_str(), "r");
  if (!fp) {
    PLOG(ERROR) << "popen (" << command << ") failed";
    return false;
  }

  // Fill |output| with the stdout from the process.
  output->clear();
  while (!feof(fp)) {
    char buffer[256];
    size_t bytes_read = fread(buffer, 1, sizeof(buffer), fp);
    if (bytes_read <= 0)
      break;
    output->append(buffer, bytes_read);
  }

  // pclose() function waits for the associated process to terminate and returns
  // the exit status.
  return (pclose(fp) == 0);
}

}  // namespace chromecast
