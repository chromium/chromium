// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_TACHYON_CLIENT_IMPL_H_
#define CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_TACHYON_CLIENT_IMPL_H_

#include <memory>
#include <string>

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/boca/babelorca/tachyon_client.h"

namespace network {

class SharedURLLoaderFactory;
class SimpleURLLoader;

}  // namespace network

namespace ash::babelorca {

struct RequestDataWrapper;

class TachyonClientImpl : public TachyonClient {
 public:
  explicit TachyonClientImpl(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

  TachyonClientImpl(const TachyonClientImpl&) = delete;
  TachyonClientImpl& operator=(const TachyonClientImpl&) = delete;

  ~TachyonClientImpl() override;

  void StartRequest(std::unique_ptr<RequestDataWrapper> request_data,
                    std::string oauth_token,
                    AuthFailureCallback auth_failure_cb) override;

 private:
  void OnResponse(std::unique_ptr<network::SimpleURLLoader> url_loader,
                  std::unique_ptr<RequestDataWrapper> request_data,
                  AuthFailureCallback auth_failure_cb,
                  std::unique_ptr<std::string> response_body);

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  base::WeakPtrFactory<TachyonClientImpl> weak_ptr_factory_{this};
};

}  // namespace ash::babelorca

#endif  // CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_TACHYON_CLIENT_IMPL_H_
