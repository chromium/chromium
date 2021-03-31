// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRONET_NATIVE_UPLOAD_DATA_SINK_H_
#define COMPONENTS_CRONET_NATIVE_UPLOAD_DATA_SINK_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "components/cronet/cronet_upload_data_stream.h"
#include "components/cronet/cronet_url_request.h"
#include "components/cronet/cronet_url_request_context.h"
#include "components/cronet/native/generated/cronet.idl_impl_interface.h"

namespace cronet {

class Cronet_UrlRequestImpl;
class Cronet_BufferWithIOBuffer;

// Implementation of Cronet_UploadDataSink that uses CronetUploadDataStream.
// Always accessed on client executor.
class Cronet_UploadDataSinkImpl : public Cronet_UploadDataSink {
 public:
  Cronet_UploadDataSinkImpl(Cronet_UrlRequestImpl* url_request,
                            Cronet_UploadDataProvider* upload_data_provider,
                            Cronet_Executor* upload_data_provider_executor);
  ~Cronet_UploadDataSinkImpl() override;

  // Initialize length and attach upload to request. Called on client thread.
  void InitRequest(CronetURLRequest* request);

  // Mark stream as closed and post |Close()| callback to consumer.
  void PostCloseToExecutor();

 private:
  class NetworkTasks;
  enum UserCallback { READ, REWIND, GET_LENGTH, NOT_IN_CALLBACK };

  // Cronet_UploadDataSink
  void OnReadSucceeded(uint64_t bytes_read, bool final_chunk) override;
  void OnReadError(Cronet_String error_message) override;
  void OnRewindSucceeded() override;
  void OnRewindError(Cronet_String error_message) override;

  // CronetUploadDataStream::Delegate methods posted from the network thread.
  void InitializeUploadDataStream(
      base::WeakPtr<CronetUploadDataStream> upload_data_stream,
      scoped_refptr<base::SingleThreadTaskRunner> network_task_runner);
  void Read(scoped_refptr<net::IOBuffer> buffer, int buf_len);
  void Rewind();
  void Close();

  void CheckState(UserCallback expected_state);

  // Cronet objects not owned by |this| and accessed on client thread.

  // The request, which owns |this|.
  Cronet_UrlRequestImpl* const url_request_ = nullptr;
  // Executor for provider callback, used, but not owned, by |this|. Always
  // outlives |this| callback.
  Cronet_ExecutorPtr const upload_data_provider_executor_ = nullptr;

  // These are initialized in InitializeUploadDataStream(), so are safe to
  // access during client callbacks, which all happen after initialization.
  scoped_refptr<base::SingleThreadTaskRunner> network_task_runner_;
  base::WeakPtr<CronetUploadDataStream> upload_data_stream_;

  bool is_chunked_ = false;
  uint64_t length_ = 0;
  uint64_t remaining_length_ = 0;

  // Synchronize access to |buffer_| and other objects below from different
  // threads.
  base::Lock lock_;
  // Data provider callback interface, used, but not owned, by |this|.
  // Set to nullptr when data provider is closed.
  Cronet_UploadDataProviderPtr upload_data_provider_ = nullptr;

  UserCallback in_which_user_callback_ = NOT_IN_CALLBACK;
  // Close data provider once it returns from the callback.
  bool close_when_not_in_callback_ = false;
  // Keeps the net::IOBuffer and Cronet ByteBuffer alive until the next Read().
  std::unique_ptr<Cronet_BufferWithIOBuffer> buffer_;

  DISALLOW_COPY_AND_ASSIGN(Cronet_UploadDataSinkImpl);
};

}  // namespace cronet

#endif  // COMPONENTS_CRONET_NATIVE_UPLOAD_DATA_SINK_H_
