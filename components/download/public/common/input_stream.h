// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_PUBLIC_COMMON_INPUT_STREAM_H_
#define COMPONENTS_DOWNLOAD_PUBLIC_COMMON_INPUT_STREAM_H_

#include "components/download/public/common/download_export.h"
#include "components/download/public/common/download_interrupt_reasons.h"
#include "mojo/public/cpp/system/simple_watcher.h"
#include "net/base/io_buffer.h"

namespace download {

// Input stream for the download system to read after network response is
// received.
class COMPONENTS_DOWNLOAD_EXPORT InputStream {
 public:
  // Results for reading the InputStream.
  enum StreamState {
    EMPTY = 0,
    HAS_DATA,
    WAIT_FOR_COMPLETION,  // No more data to read, waiting for completion status
    COMPLETE,
  };

  virtual ~InputStream();

  // Initializes the inputStream object.
  virtual void Initialize();

  // Returns true if the input stream contains no data, or false otherwise.
  virtual bool IsEmpty();

  // Register/clear callbacks when data become available.
  virtual void RegisterDataReadyCallback(
      const mojo::SimpleWatcher::ReadyCallback& callback);
  virtual void ClearDataReadyCallback();

  // Registers stream completion callback if needed.
  virtual void RegisterCompletionCallback(base::OnceClosure callback);

  // Reads data from the stream into |data|, |length| is the number of bytes
  // returned.
  virtual StreamState Read(scoped_refptr<net::IOBuffer>* data, size_t* length);

  // Returns the completion status.
  virtual DownloadInterruptReason GetCompletionStatus();
};

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_PUBLIC_COMMON_INPUT_STREAM_H_
