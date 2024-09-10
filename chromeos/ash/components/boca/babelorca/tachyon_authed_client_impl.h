// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_TACHYON_AUTHED_CLIENT_IMPL_H_
#define CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_TACHYON_AUTHED_CLIENT_IMPL_H_

#include <memory>
#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "chromeos/ash/components/boca/babelorca/tachyon_authed_client.h"

namespace ash::babelorca {

class TachyonClient;
class TokenManager;
struct RequestDataWrapper;

class TachyonAuthedClientImpl : public TachyonAuthedClient {
 public:
  TachyonAuthedClientImpl(std::unique_ptr<TachyonClient> client,
                          TokenManager* oauth_token_manager);

  TachyonAuthedClientImpl(const TachyonAuthedClientImpl&) = delete;
  TachyonAuthedClientImpl& operator=(const TachyonAuthedClientImpl&) = delete;

  ~TachyonAuthedClientImpl() override;

  // TachyonAuthedClient:
  void StartAuthedRequest(
      std::unique_ptr<RequestDataWrapper> request_data,
      std::unique_ptr<google::protobuf::MessageLite> request_proto) override;
  void StartAuthedRequestString(
      std::unique_ptr<RequestDataWrapper> request_data,
      std::string request_string) override;

 private:
  void OnRequestProtoSerialized(
      std::unique_ptr<RequestDataWrapper> request_data,
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
