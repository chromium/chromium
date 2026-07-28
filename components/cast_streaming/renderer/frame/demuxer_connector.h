// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CAST_STREAMING_RENDERER_FRAME_DEMUXER_CONNECTOR_H_
#define COMPONENTS_CAST_STREAMING_RENDERER_FRAME_DEMUXER_CONNECTOR_H_

#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/synchronization/lock.h"
#include "base/task/sequenced_task_runner.h"
#include "components/cast_streaming/common/public/mojom/demuxer_connector.mojom.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"

namespace cast_streaming {

// Thread-safe buffer used to store stream initialization configurations
// received over Mojo IPC on the main thread and dispatch them to
// FrameInjectingDemuxer on the media thread.
class DemuxerStreamConfigBuffer
    : public base::RefCountedThreadSafe<DemuxerStreamConfigBuffer> {
 public:
  using ConfigCallback =
      base::OnceCallback<void(mojom::AudioStreamInitializationInfoPtr,
                              mojom::VideoStreamInitializationInfoPtr)>;

  DemuxerStreamConfigBuffer();

  void SetConfigs(mojom::AudioStreamInitializationInfoPtr audio_stream_info,
                  mojom::VideoStreamInitializationInfoPtr video_stream_info);

  void ReadConfigs(scoped_refptr<base::SequencedTaskRunner> media_task_runner,
                   ConfigCallback callback);

 private:
  friend class base::RefCountedThreadSafe<DemuxerStreamConfigBuffer>;
  ~DemuxerStreamConfigBuffer();

  base::Lock lock_;
  bool has_configs_ GUARDED_BY(lock_) = false;
  mojom::AudioStreamInitializationInfoPtr audio_stream_info_ GUARDED_BY(lock_);
  mojom::VideoStreamInitializationInfoPtr video_stream_info_ GUARDED_BY(lock_);

  scoped_refptr<base::SequencedTaskRunner> media_task_runner_ GUARDED_BY(lock_);
  ConfigCallback pending_callback_ GUARDED_BY(lock_);
};

// Handles initiating the streaming session between the browser-process sender
// and renderer-process receiver of the Cast Streaming Session. Specifically,
// this class manages the DemuxerConnector's lifetime in the renderer
// process. The lifetime of this object should match that of |render_frame| with
// which it is associated.
class DemuxerConnector final : public mojom::DemuxerConnector {
 public:
  DemuxerConnector();
  ~DemuxerConnector() override;
  DemuxerConnector(const DemuxerConnector&) = delete;
  DemuxerConnector& operator=(const DemuxerConnector&) = delete;

  void BindReceiver(
      mojo::PendingAssociatedReceiver<mojom::DemuxerConnector> connector);

  scoped_refptr<DemuxerStreamConfigBuffer> config_buffer() const {
    return config_buffer_;
  }

  // Returns true if a Mojo connection is active.
  bool IsBound() const;

 private:
  void MaybeCallEnableReceiverCallback();

  void OnReceiverDisconnected();

  // mojom::DemuxerConnector implementation.
  void EnableReceiver(EnableReceiverCallback callback) override;
  void OnStreamsInitialized(
      mojom::AudioStreamInitializationInfoPtr audio_stream_info,
      mojom::VideoStreamInitializationInfoPtr video_stream_info) override;

  mojo::AssociatedReceiver<mojom::DemuxerConnector> demuxer_connector_receiver_{
      this};

  EnableReceiverCallback enable_receiver_callback_;
  scoped_refptr<DemuxerStreamConfigBuffer> config_buffer_ =
      base::MakeRefCounted<DemuxerStreamConfigBuffer>();

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace cast_streaming

#endif  // COMPONENTS_CAST_STREAMING_RENDERER_FRAME_DEMUXER_CONNECTOR_H_
