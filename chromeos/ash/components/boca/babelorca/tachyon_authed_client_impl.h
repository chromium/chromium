// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_TACHYON_AUTHED_CLIENT_IMPL_H_
#define CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_TACHYON_AUTHED_CLIENT_IMPL_H_

#include <memory>
#include <optional>
#include <string_view>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "chromeos/ash/components/boca/babelorca/request_data_wrapper.h"
#include "chromeos/ash/components/boca/babelorca/tachyon_authed_client.h"

namespace ash::babelorca {

class TachyonClient;
class TokenManager;

class TachyonAuthedClientImpl : public TachyonAuthedClient {
 public:
  TachyonAuthedClientImpl(std::unique_ptr<TachyonClient> client,
                          TokenManager* oauth_token_manager);

  TachyonAuthedClientImpl(const TachyonAuthedClientImpl&) = delete;
  TachyonAuthedClientImpl& operator=(const TachyonAuthedClientImpl&) = delete;

  ~TachyonAuthedClientImpl() override;

  // TachyonAuthedClient:
  void StartAuthedRequest(
      const net::NetworkTrafficAnnotationTag& annotation_tag,
      std::unique_ptr<google::protobuf::MessageLite> request_proto,
      std::string_view url,
      int max_retries,
      std::unique_ptr<ResponseCallbackWrapper> response_cb) override;
  void StartAuthedRequestString(
      const net::NetworkTrafficAnnotationTag& annotation_tag,
      std::string request_string,
      std::string_view url,
      int max_retries,
      std::unique_ptr<ResponseCallbackWrapper> response_cb) override;

 private:
  void OnRequestProtoSerialized(
      const net::NetworkTrafficAnnotationTag& annotation_tag,
      std::string_view url,
      int max_retries,
      std::unique_ptr<ResponseCallbackWrapper> response_cb,
      std::optional<std::string> request_string);

  void StartAuthedRequestInternal(
      std::unique_ptr<RequestDataWrapper> request_data,
      bool has_oauth_token);

  void OnRequestAuthFailure(std::unique_ptr<RequestDataWrapper> request_data);

  SEQUENCE_CHECKER(sequence_checker_);

  const std::unique_ptr<TachyonClient> client_;
  raw_ptr<TokenManager> oauth_token_manager_;

  base::WeakPtrFactory<TachyonAuthedClientImpl> weak_ptr_factory_{this};
};

}  // namespace ash::babelorca

#endif  // CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_TACHYON_AUTHED_CLIENT_IMPL_H_
