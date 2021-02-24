// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cronet/native/upload_data_sink.h"

#include <inttypes.h>
#include <utility>

#include "base/bind.h"
#include "base/check_op.h"
#include "base/macros.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/cronet/cronet_upload_data_stream.h"
#include "components/cronet/native/engine.h"
#include "components/cronet/native/generated/cronet.idl_impl_struct.h"
#include "components/cronet/native/include/cronet_c.h"
#include "components/cronet/native/io_buffer_with_cronet_buffer.h"
#include "components/cronet/native/runnables.h"
#include "components/cronet/native/url_request.h"
#include "net/base/io_buffer.h"

namespace cronet {

// This class is called by Cronet's network stack as an implementation of
// CronetUploadDataStream::Delegate, and forwards the calls along to
// Cronet_UploadDataSinkImpl on the embedder's executor.
// This class is always called on the network thread and is destroyed in
// OnUploadDataStreamDestroyed() callback.
class Cronet_UploadDataSinkImpl::NetworkTasks
    : public CronetUploadDataStream::Delegate {
 public:
  NetworkTasks(Cronet_UploadDataSinkImpl* upload_data_sink,
               Cronet_Executor* upload_data_provider_executor);
  ~NetworkTasks() override;

 private:
  // CronetUploadDataStream::Delegate implementation:
  void InitializeOnNetworkThread(
      base::WeakPtr<CronetUploadDataStream> upload_data_stream) override;
  void Read(scoped_refptr<net::IOBuffer> buffer, int buf_len) override;
  void Rewind() override;
  void OnUploadDataStreamDestroyed() override;

  // Post |task| to client executor.
  void PostTaskToExecutor(base::OnceClosure task);

  // The upload data sink that is owned by url request and always accessed on
  // the client thread. It always outlives |this| callback.
  Cronet_UploadDataSinkImpl* const upload_data_sink_ = nullptr;

  // Executor for provider callback, used, but not owned, by |this|. Always
  // outlives |this| callback.
  Cronet_ExecutorPtr const upload_data_provider_executor_ = nullptr;

  THREAD_CHECKER(network_thread_checker_);
  DISALLOW_COPY_AND_ASSIGN(NetworkTasks);
};

Cronet_UploadDataSinkImpl::NetworkTasks::NetworkTasks(
    Cronet_UploadDataSinkImpl* upload_data_sink,
    Cronet_Executor* upload_data_provider_executor)
    : upload_data_sink_(upload_data_sink),
      upload_data_provider_executor_(upload_data_provider_executor) {
  DETACH_FROM_THREAD(network_thread_checker_);
}

Cronet_UploadDataSinkImpl::NetworkTasks::~NetworkTasks() = default;

Cronet_UploadDataSinkImpl::Cronet_UploadDataSinkImpl(
    Cronet_UrlRequestImpl* url_request,
    Cronet_UploadDataProvider* upload_data_provider,
    Cronet_Executor* upload_data_provider_executor)
    : url_request_(url_request),
      upload_data_provider_executor_(upload_data_provider_executor),
      upload_data_provider_(upload_data_provider) {}

Cronet_UploadDataSinkImpl::~Cronet_UploadDataSinkImpl() = default;

void Cronet_UploadDataSinkImpl::InitRequest(CronetURLRequest* request) {
  int64_t length = upload_data_provider_->GetLength();
  if (length == -1) {
    is_chunked_ = true;
  } else {
    CHECK_GE(length, 0);
    length_ = static_cast<uint64_t>(length);
    remaining_length_ = length_;
  }

  request->SetUpload(std::make_unique<CronetUploadDataStream>(
      new NetworkTasks(this, upload_data_provider_executor_), length));
}

void Cronet_UploadDataSinkImpl::OnReadSucceeded(uint64_t bytes_read,
                                                bool final_chunk) {
  {
    base::AutoLock lock(lock_);
    CheckState(READ);
    in_which_user_callback_ = NOT_IN_CALLBACK;
    if (!upload_data_provider_)
      return;
  }
  if (url_request_->IsDone())
    return;
  if (close_when_not_in_callback_) {
    PostCloseToExecutor();
    return;
  }
  CHECK(bytes_read > 0 || (final_chunk && bytes_read == 0));
  // Bytes read exceeds buffer length.
  CHECK_LE(static_cast<size_t>(bytes_read), buffer_->io_buffer_len());
  if (!is_chunked_) {
    // Only chunked upload can have the final chunk.
    CHECK(!final_chunk);
    // Read upload data length exceeds specified length.
    if (bytes_read > remaining_length_) {
      PostCloseToExecutor();
      std::string error_message =
          base::StringPrintf("Read upload data length %" PRIu64
                             " exceeds expected length %" PRIu64,
                             length_ - remaining_length_ + bytes_read, length_);
      url_request_->OnUploadDataProviderError(error_message.c_str());
      return;
    }
    remaining_length_ -= bytes_read;
  }
  network_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&CronetUploadDataStream::OnReadSuccess,
                                upload_data_stream_, bytes_read, final_chunk));
}

void Cronet_UploadDataSinkImpl::OnReadError(Cronet_String error_message) {
  {
    base::AutoLock lock(lock_);
    CheckState(READ);
    in_which_user_callback_ = NOT_IN_CALLBACK;
    if (!upload_data_provider_)
      return;
  }
  if (url_request_->IsDone())
    return;
  PostCloseToExecutor();
  url_request_->OnUploadDataProviderError(error_message);
}

void Cronet_UploadDataSinkImpl::OnRewindSucceeded() {
  {
    base::AutoLock lock(lock_);
    CheckState(REWIND);
    in_which_user_callback_ = NOT_IN_CALLBACK;
    if (!upload_data_provider_)
      return;
  }
  remaining_length_ = length_;
  if (url_request_->IsDone())
    return;
  if (close_when_not_in_callback_) {
    PostCloseToExecutor();
    return;
  }
  network_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&CronetUploadDataStream::OnRewindSuccess,
                                upload_data_stream_));
}

void Cronet_UploadDataSinkImpl::OnRewindError(Cronet_String error_message) {
  {
    base::AutoLock lock(lock_);
    CheckState(REWIND);
    in_which_user_callback_ = NOT_IN_CALLBACK;
    if (!upload_data_provider_)
      return;
  }
  if (url_request_->IsDone())
    return;
  PostCloseToExecutor();
  url_request_->OnUploadDataProviderError(error_message);
}

void Cronet_UploadDataSinkImpl::InitializeUploadDataStream(
    base::WeakPtr<CronetUploadDataStream> upload_data_stream,
    scoped_refptr<base::SingleThreadTaskRunner> network_task_runner) {
  DCHECK(!upload_data_stream_);
  DCHECK(!network_task_runner_.get());
  upload_data_stream_ = upload_data_stream;
  network_task_runner_ = network_task_runner;
}

void Cronet_UploadDataSinkImpl::PostCloseToExecutor() {
  Cronet_RunnablePtr runnable = new cronet::OnceClosureRunnable(base::BindOnce(
      &Cronet_UploadDataSinkImpl::Close, base::Unretained(this)));
  // |runnable| is passed to executor, which destroys it after execution.
  Cronet_Executor_Execute(upload_data_provider_executor_, runnable);
}

void Cronet_UploadDataSinkImpl::Read(scoped_refptr<net::IOBuffer> buffer,
                                     int buf_len) {
  if (url_request_->IsDone())
    return;
  Cronet_UploadDataProviderPtr upload_data_provider = nullptr;
  {
    base::AutoLock lock(lock_);
    if (!upload_data_provider_)
      return;
    CheckState(NOT_IN_CALLBACK);
    in_which_user_callback_ = READ;
    upload_data_provider = upload_data_provider_;
  }
  buffer_ =
      std::make_unique<Cronet_BufferWithIOBuffer>(std::move(buffer), buf_len);
  Cronet_UploadDataProvider_Read(upload_data_provider, this,
                                 buffer_->cronet_buffer());
}

void Cronet_UploadDataSinkImpl::Rewind() {
  if (url_request_->IsDone())
    return;
  Cronet_UploadDataProviderPtr upload_data_provider = nullptr;
  {
    base::AutoLock lock(lock_);
    if (!upload_data_provider_)
      return;
    CheckState(NOT_IN_CALLBACK);
    in_which_user_callback_ = REWIND;
    upload_data_provider = upload_data_provider_;
  }
  Cronet_UploadDataProvider_Rewind(upload_data_provider, this);
}

void Cronet_UploadDataSinkImpl::Close() {
  Cronet_UploadDataProviderPtr upload_data_provider = nullptr;
  {
    base::AutoLock lock(lock_);
    // If |upload_data_provider_| is already closed from OnResponseStarted(),
    // don't close it again from OnError() or OnCanceled().
    if (!upload_data_provider_)
      return;
    if (in_which_user_callback_ != NOT_IN_CALLBACK) {
      // If currently in the callback, then wait until return from callback
      // before closing.
      close_when_not_in_callback_ = true;
      return;
    }
    upload_data_provider = upload_data_provider_;
    upload_data_provider_ = nullptr;
  }
  Cronet_UploadDataProvider_Close(upload_data_provider);
}

void Cronet_UploadDataSinkImpl::CheckState(UserCallback expected_state) {
  lock_.AssertAcquired();
  CHECK(in_which_user_callback_ == expected_state);
}

void Cronet_UploadDataSinkImpl::NetworkTasks::InitializeOnNetworkThread(
    base::WeakPtr<CronetUploadDataStream> upload_data_stream) {
  DCHECK_CALLED_ON_VALID_THREAD(network_thread_checker_);
  PostTaskToExecutor(
      base::BindOnce(&Cronet_UploadDataSinkImpl::InitializeUploadDataStream,
                     base::Unretained(upload_data_sink_), upload_data_stream,
                     base::ThreadTaskRunnerHandle::Get()));
}

void Cronet_UploadDataSinkImpl::NetworkTasks::Read(
    scoped_refptr<net::IOBuffer> buffer,
    int buf_len) {
  DCHECK_CALLED_ON_VALID_THREAD(network_thread_checker_);
  PostTaskToExecutor(base::BindOnce(&Cronet_UploadDataSinkImpl::Read,
                                    base::Unretained(upload_data_sink_),
                                    std::move(buffer), buf_len));
}

void Cronet_UploadDataSinkImpl::NetworkTasks::Rewind() {
  DCHECK_CALLED_ON_VALID_THREAD(network_thread_checker_);
  PostTaskToExecutor(base::BindOnce(&Cronet_UploadDataSinkImpl::Rewind,
                                    base::Unretained(upload_data_sink_)));
}

void Cronet_UploadDataSinkImpl::NetworkTasks::OnUploadDataStreamDestroyed() {
  DCHECK_CALLED_ON_VALID_THREAD(network_thread_checker_);
  delete this;
}

void Cronet_UploadDataSinkImpl::NetworkTasks::PostTaskToExecutor(
    base::OnceClosure task) {
  Cronet_RunnablePtr runnable =
      new cronet::OnceClosureRunnable(std::move(task));
  // |runnable| is passed to executor, which destroys it after execution.
  Cronet_Executor_Execute(upload_data_provider_executor_, runnable);
}

}  // namespace cronet
