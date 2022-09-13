// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UPDATE_CLIENT_COMPONENT_PATCHER_UNITTEST_H_
#define COMPONENTS_UPDATE_CLIENT_COMPONENT_PATCHER_UNITTEST_H_

#include "base/files/scoped_temp_dir.h"
#include "base/memory/ref_counted.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/task_environment.h"
#include "courgette/courgette.h"
#include "courgette/third_party/bsdiff/bsdiff.h"
#include "testing/gtest/include/gtest/gtest.h"

// TODO(crbug.com/1349158): Remove this file once Puffin patches are fully
// implemented.

namespace update_client {

class ReadOnlyTestInstaller;

const char binary_output_hash[] =
    "599aba6d15a7da390621ef1bacb66601ed6aed04dadc1f9b445dcfe31296142a";

class ComponentPatcherOperationTest : public testing::Test {
 public:
  ComponentPatcherOperationTest();
  ~ComponentPatcherOperationTest() override;

 protected:
  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir input_dir_;
  base::ScopedTempDir installed_dir_;
  base::ScopedTempDir unpack_dir_;
  scoped_refptr<ReadOnlyTestInstaller> installer_;
};

}  // namespace update_client

#endif  // COMPONENTS_UPDATE_CLIENT_COMPONENT_PATCHER_UNITTEST_H_
