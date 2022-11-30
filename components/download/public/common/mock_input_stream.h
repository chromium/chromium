// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_PUBLIC_COMMON_MOCK_INPUT_STREAM_H_
#define COMPONENTS_DOWNLOAD_PUBLIC_COMMON_MOCK_INPUT_STREAM_H_

#include "components/download/public/common/input_stream.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace download {

class MockInputStream : public InputStream {
 public:
  MockInputStream();
  ~MockInputStream() override;

  // InputStream functions
  MOCK_METHOD0(IsEmpty, bool());
  MOCK_METHOD1(RegisterDataReadyCallback,
               void(const mojo::SimpleWatcher::ReadyCallback&));
  MOCK_METHOD0(ClearDataReadyCallback, void());
  MOCK_METHOD2(Read,
               InputStream::StreamState(scoped_refptr<net::IOBuffer>*,
                                        size_t*));
  MOCK_METHOD0(GetCompletionStatus, DownloadInterruptReason());
};

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_PUBLIC_COMMON_MOCK_INPUT_STREAM_H_
