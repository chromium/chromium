// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_SHARING_NEARBY_PLATFORM_BIDIRECTIONAL_STREAM_H_
#define CHROME_SERVICES_SHARING_NEARBY_PLATFORM_BIDIRECTIONAL_STREAM_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/synchronization/waitable_event.h"
#include "chromeos/ash/services/nearby/public/mojom/nearby_connections_types.mojom.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "third_party/nearby/src/internal/platform/exception.h"

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace nearby {

class InputStream;
class OutputStream;

namespace chrome {

// A wrapper around input and output stream implementations that read/write
// from/to |receive_stream|/|send_stream|. These Mojo DataPipes are passed into
// the constructor by the specified |medium| implementation. This class handles
// the subtleties of creating and destroying the streams on the |task_runner|.
class BidirectionalStream {
 public:
  BidirectionalStream(connections::mojom::Medium medium,
                      scoped_refptr<base::SequencedTaskRunner> task_runner,
                      mojo::ScopedDataPipeConsumerHandle receive_stream,
                      mojo::ScopedDataPipeProducerHandle send_stream);
  ~BidirectionalStream();
  BidirectionalStream(const BidirectionalStream&) = delete;
  BidirectionalStream& operator=(const BidirectionalStream&) = delete;

  InputStream* GetInputStream();
  OutputStream* GetOutputStream();
  Exception Close();

 private:
  // Input/output stream implementations require the streams to be created and
  // destroyed on the |task_runner_|.
  void CreateStreams(mojo::ScopedDataPipeConsumerHandle receive_stream,
                     mojo::ScopedDataPipeProducerHandle send_stream,
                     base::WaitableEvent* task_run_waitable_event);
  void DestroyStreams(base::WaitableEvent* task_run_waitable_event);

  connections::mojom::Medium medium_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  std::unique_ptr<InputStream> input_stream_;
  std::unique_ptr<OutputStream> output_stream_;
};

}  // namespace chrome
}  // namespace nearby

#endif  // CHROME_SERVICES_SHARING_NEARBY_PLATFORM_BIDIRECTIONAL_STREAM_H_
