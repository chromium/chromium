// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_TRANSCRIPT_RECEIVER_H_
#define CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_TRANSCRIPT_RECEIVER_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/timer/timer.h"
#include "chromeos/ash/components/boca/babelorca/tachyon_streaming_client.h"
#include "chromeos/ash/services/boca/babelorca/mojom/tachyon_parsing_service.mojom-forward.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

namespace media {
struct SpeechRecognitionResult;
}  // namespace media

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace ash::babelorca {

class TachyonAuthedClient;
class TachyonRequestDataProvider;
class TachyonResponse;
class TranscriptBuilder;

class TranscriptReceiver {
 public:
  using StreamingClientGetter =
      base::RepeatingCallback<std::unique_ptr<TachyonAuthedClient>(
          scoped_refptr<network::SharedURLLoaderFactory>,
          TachyonStreamingClient::OnMessageCallback)>;
  using OnTranscript =
      base::RepeatingCallback<void(media::SpeechRecognitionResult,
                                   std::string language)>;
  TranscriptReceiver(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const net::NetworkTrafficAnnotationTag& network_traffic_annotation,
      TachyonRequestDataProvider* request_data_provider,
      StreamingClientGetter streaming_client_getter,
      int max_retries = 3);

  TranscriptReceiver(const TranscriptReceiver&) = delete;
  TranscriptReceiver& operator=(const TranscriptReceiver&) = delete;

  ~TranscriptReceiver();

  void StartReceiving(OnTranscript on_transcript_cb,
                      base::OnceClosure on_failure_cb);

 private:
  void StartReceivingInternal();

  void OnMessage(mojom::BabelOrcaMessagePtr message);

  void OnResponse(TachyonResponse response);

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  const net::NetworkTrafficAnnotationTag network_traffic_annotation_;
  const raw_ptr<TachyonRequestDataProvider> request_data_provider_;
  const StreamingClientGetter streaming_client_getter_;

  OnTranscript on_transcript_cb_;
  base::OnceClosure on_failure_cb_;

  std::unique_ptr<TachyonAuthedClient> streaming_client_;
  std::unique_ptr<TranscriptBuilder> transcript_builder_;

  int retry_count_ = 0;
  base::OneShotTimer retry_timer_;
  const int max_retries_;
};

}  // namespace ash::babelorca

#endif  // CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_TRANSCRIPT_RECEIVER_H_
