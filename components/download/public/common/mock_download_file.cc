// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/public/common/mock_download_file.h"

#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"
#include "testing/gmock/include/gmock/gmock.h"

using ::testing::_;
using ::testing::Return;

namespace download {
namespace {

ACTION_P(PostSuccessRun, task_runner) {
  task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(arg0), DOWNLOAD_INTERRUPT_REASON_NONE, 0));
}

}  // namespace

MockDownloadFile::MockDownloadFile() {
  // This is here because |Initialize()| is normally called right after
  // construction.
  ON_CALL(*this, Initialize(_, _, _))
      .WillByDefault(
          PostSuccessRun(base::SingleThreadTaskRunner::GetCurrentDefault()));
}

MockDownloadFile::~MockDownloadFile() = default;

void MockDownloadFile::AddInputStream(std::unique_ptr<InputStream> input_stream,
                                      int64_t offset) {
  // Gmock currently can't mock method that takes move-only parameters,
  // delegate the EXPECT_CALL count to |DoAddByteStream|.
  DoAddInputStream(input_stream.get(), offset);
}

}  // namespace download
