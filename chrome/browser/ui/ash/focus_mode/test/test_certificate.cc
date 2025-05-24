// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/focus_mode/test/test_certificate.h"

#include "base/check.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/threading/thread_restrictions.h"

namespace ash {

std::string ReadSha1TestCertificate() {
  base::FilePath base_path;
  CHECK(base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &base_path));
  base::FilePath file_path = base_path.AppendASCII("chrome")
                                 .AppendASCII("browser")
                                 .AppendASCII("ui")
                                 .AppendASCII("ash")
                                 .AppendASCII("focus_mode")
                                 .AppendASCII("test")
                                 .AppendASCII("data")
                                 .AppendASCII("certificate_sha1.pem");
  std::string certificate_string;
  {
    base::ScopedAllowBlockingForTesting allow_io;
    CHECK(base::PathExists(file_path)) << file_path.MaybeAsASCII();
    CHECK(base::ReadFileToString(file_path, &certificate_string));
  }
  return certificate_string;
}

}  // namespace ash
