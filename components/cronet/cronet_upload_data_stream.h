// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRONET_CRONET_UPLOAD_DATA_STREAM_H_
#define COMPONENTS_CRONET_CRONET_UPLOAD_DATA_STREAM_H_

#include <stdint.h>

#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "net/base/upload_data_stream.h"

namespace net {
class IOBuffer;
}  // namespace net

namespace cronet {

// The CronetUploadDataStream is created on a client thread, but afterwards,
// lives and is deleted on the network thread. It's responsible for ensuring
// only one read/rewind request sent to client is outstanding at a time.
// The main complexity is around Reset/Initialize calls while there's a pending
// read or rewind.
class CronetUploadDataStream : public net::UploadDataStream {
 public:
  class Delegate {
   public:
    // Called once during initial setup on the network thread, called before
    // all other methods.
    virtual void InitializeOnNetworkThread(
        base::WeakPtr<CronetUploadDataStream> upload_data_stream) = 0;

    // Called for each read request. Delegate must respond by calling
    // OnReadSuccess on the network thread asynchronous, or failing the request.
    // Only called when there's no other pending read or rewind operation.
    virtual void Read(net::IOBuffer* buffer, int buf_len) = 0;

    // Called to rewind the stream. Not called when already at the start of the
    // stream. The delegate must respond by calling OnRewindSuccess
    // asynchronously on the network thread, or failing the request. Only called
    // when there's no other pending read or rewind operation.
    virtual void Rewind() = 0;

    // Called when the CronetUploadDataStream is destroyed. The Delegate is then
    // responsible for destroying itself. May be called when there's a pending
    // read or rewind operation.
    virtual void OnUploadDataStreamDestroyed() = 0;

   protected:
    Delegate() {}
    virtual ~Delegate() {}

   private:
    DISALLOW_COPY_AND_ASSIGN(Delegate);
  };

  CronetUploadDataStream(Delegate* delegate, int64_t size);
  ~CronetUploadDataStream() override;

  // Failure is handled at the Java layer. These two success callbacks are
  // invoked by client UploadDataSink upon completion of the operation.
  void OnReadSuccess(int bytes_read, bool final_chunk);
  void OnRewindSuccess();

 private:
  // net::UploadDataStream implementation:
  int InitInternal(const net::NetLogWithSource& net_log) override;
  int ReadInternal(net::IOBuffer* buf, int buf_len) override;
  void ResetInternal() override;

  // Starts rewinding the stream. Only called when not already at the front of
  // the stream, and no operation is pending. Completes asynchronously.
  void StartRewind();

  // Size of the upload. -1 if chunked.
  const int64_t size_;

  // True if ReadInternal has been called, the read hasn't completed, and there
  // hasn't been a ResetInternal call yet.
  bool waiting_on_read_;
  // True if there's a read operation in progress. This will always be true
  // when |waiting_on_read_| is true. This will only be set to false once it
  // completes, even though ResetInternal may have been called since the read
  // started.
  bool read_in_progress_;

  // True if InitInternal has been called, the rewind hasn't completed, and
  // there hasn't been a ResetInternal call yet. Note that this may be true
  // even when the rewind hasn't yet started, if there's a read in progress.
  bool waiting_on_rewind_;
  // True if there's a rewind operation in progress. Rewinding will only start
  // when |waiting_on_rewind_| is true, and |read_in_progress_| is false. This
  // will only be set to false once it completes, even though ResetInternal may
  // have been called since the rewind started.
  bool rewind_in_progress_;

  // Set to false when a read starts, true when a rewind completes.
  bool at_front_of_stream_;

  Delegate* const delegate_;

  // Vends pointers on the network thread, though created on a client thread.
  base::WeakPtrFactory<CronetUploadDataStream> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(CronetUploadDataStream);
};

}  // namespace cronet

#endif  // COMPONENTS_CRONET_CRONET_UPLOAD_DATA_STREAM_H_
