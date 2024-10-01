// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_TACHYON_STREAMING_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_TACHYON_STREAMING_CLIENT_H_

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "chromeos/ash/components/boca/babelorca/tachyon_client.h"
#include "chromeos/ash/services/boca/babelorca/mojom/tachyon_parsing_service.mojom-shared.h"
#include "chromeos/ash/services/boca/babelorca/mojom/tachyon_parsing_service.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/cpp/simple_url_loader_stream_consumer.h"

namespace network {

class SharedURLLoaderFactory;
class SimpleURLLoader;

}  // namespace network

namespace ash::babelorca {

struct RequestDataWrapper;

class TachyonStreamingClient : public TachyonClient,
                               public network::SimpleURLLoaderStreamConsumer {
 public:
  using ParsingServiceBinder =
      base::RepeatingCallback<mojo::Remote<mojom::TachyonParsingService>()>;
  using OnMessageCallback =
      base::RepeatingCallback<void(mojom::BabelOrcaMessagePtr)>;

  TachyonStreamingClient(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      ParsingServiceBinder binder_callback,
      OnMessageCallback on_message_callback);

  TachyonStreamingClient(const TachyonStreamingClient&) = delete;
  TachyonStreamingClient& operator=(const TachyonStreamingClient&) = delete;

  ~TachyonStreamingClient() override;

  // TachyonClient:
  void StartRequest(std::unique_ptr<RequestDataWrapper> request_data,
                    std::string oauth_token,
                    AuthFailureCallback auth_failure_cb) override;

  // network::SimpleURLLoaderStreamConsumer:
  void OnDataReceived(std::string_view string_piece,
                      base::OnceClosure resume) override;
  void OnComplete(bool success) override;
  void OnRetry(base::OnceClosure start_retry) override;

 private:
  void OnParsed(base::OnceClosure resume,
                mojom::ParsingState parsing_state,
                std::vector<mojom::BabelOrcaMessagePtr> messages,
                mojom::StreamStatusPtr stream_status);
  void OnParsingServiceDisconnected();

  const scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  const ParsingServiceBinder binder_callback_;
  const OnMessageCallback on_message_callback_;

  SEQUENCE_CHECKER(sequence_checker_);

  std::unique_ptr<RequestDataWrapper> request_data_
      GUARDED_BY_CONTEXT(sequence_checker_);

  AuthFailureCallback auth_failure_cb_ GUARDED_BY_CONTEXT(sequence_checker_);

  mojo::Remote<mojom::TachyonParsingService> parsing_service_
      GUARDED_BY_CONTEXT(sequence_checker_);

  std::unique_ptr<network::SimpleURLLoader> url_loader_
      GUARDED_BY_CONTEXT(sequence_checker_);
};

}  // namespace ash::babelorca

#endif  // CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_TACHYON_STREAMING_CLIENT_H_
