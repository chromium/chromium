// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/nearby/platform/bidirectional_stream.h"

#include "base/functional/bind.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/services/sharing/nearby/platform/input_stream_impl.h"
#include "chrome/services/sharing/nearby/platform/output_stream_impl.h"

namespace nearby::chrome {

BidirectionalStream::BidirectionalStream(
    connections::mojom::Medium medium,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    mojo::ScopedDataPipeConsumerHandle receive_stream,
    mojo::ScopedDataPipeProducerHandle send_stream)
    : medium_(medium), task_runner_(std::move(task_runner)) {
  base::WaitableEvent task_run_waitable_event;
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&BidirectionalStream::CreateStreams,
                     base::Unretained(this), std::move(receive_stream),
                     std::move(send_stream), &task_run_waitable_event));
  task_run_waitable_event.Wait();
}

BidirectionalStream::~BidirectionalStream() {
  base::WaitableEvent task_run_waitable_event;
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&BidirectionalStream::DestroyStreams,
                     base::Unretained(this), &task_run_waitable_event));
  task_run_waitable_event.Wait();
}

InputStream* BidirectionalStream::GetInputStream() {
  return input_stream_.get();
}

OutputStream* BidirectionalStream::GetOutputStream() {
  return output_stream_.get();
}

Exception BidirectionalStream::Close() {
  Exception input_exception = input_stream_->Close();
  Exception output_exception = output_stream_->Close();

  if (input_exception.Ok() && output_exception.Ok())
    return {Exception::kSuccess};
  if (!input_exception.Ok())
    return input_exception;
  if (!output_exception.Ok())
    return output_exception;
  NOTREACHED_IN_MIGRATION();
  return {Exception::kFailed};
}

void BidirectionalStream::CreateStreams(
    mojo::ScopedDataPipeConsumerHandle receive_stream,
    mojo::ScopedDataPipeProducerHandle send_stream,
    base::WaitableEvent* task_run_waitable_event) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  input_stream_ = std::make_unique<InputStreamImpl>(medium_, task_runner_,
                                                    std::move(receive_stream));
  output_stream_ = std::make_unique<OutputStreamImpl>(medium_, task_runner_,
                                                      std::move(send_stream));
  task_run_waitable_event->Signal();
}

void BidirectionalStream::DestroyStreams(
    base::WaitableEvent* task_run_waitable_event) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  input_stream_.reset();
  output_stream_.reset();
  task_run_waitable_event->Signal();
}

}  // namespace nearby::chrome
