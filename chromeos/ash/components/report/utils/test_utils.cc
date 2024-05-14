// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/report/utils/test_utils.h"

#include "base/files/file_util.h"
#include "base/logging.h"

namespace ash::report::utils {

namespace {

// PrivateSetMembership regression tests maximum file size is 4MB.
const size_t kMaxFileSizeInBytes = 4 * (1 << 20);

}  // namespace

bool ParseProtoFromFile(const base::FilePath& file_path,
                        google::protobuf::MessageLite* out_proto) {
  if (!out_proto) {
    LOG(ERROR) << "Target proto is undefined.";
    return false;
  }

  std::string file_content;
  if (!base::ReadFileToStringWithMaxSize(file_path, &file_content,
                                         kMaxFileSizeInBytes)) {
    LOG(ERROR) << "Error reading filepath: " << file_path;
    return false;
  }

  return out_proto->ParseFromString(file_content);
}

private_computing::PrivateComputingClientRegressionTestData::TestCase
GetPreservedFileTestCase(
    private_computing::PrivateComputingClientRegressionTestData* test_data,
    private_computing::PrivateComputingClientRegressionTestData::TestName
        test_name) {
  for (const auto& test : test_data->test_cases()) {
    if (test.name() == test_name) {
      return test;
    }
  }

  LOG(ERROR) << "Error finding test_name "
             << private_computing::PrivateComputingClientRegressionTestData::
                    TestName_Name(test_name);
  NOTREACHED_IN_MIGRATION();
  return private_computing::PrivateComputingClientRegressionTestData::
      TestCase();
}

}  // namespace ash::report::utils
