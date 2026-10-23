// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRIVATE_AI_IOS_PRIVATE_AI_OAK_SESSION_DRIVER_IOS_H_
#define COMPONENTS_PRIVATE_AI_IOS_PRIVATE_AI_OAK_SESSION_DRIVER_IOS_H_

#include <map>
#include <memory>

#include "base/memory/weak_ptr.h"
#include "components/private_ai/mojom/oak_session.mojom.h"
#include "components/private_ai/oak_session_service/oak_session_service.h"
#include "components/private_ai/oak_session_service/oak_tls_session_service.h"
#include "components/private_ai/private_ai_oak_session_driver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace private_ai {

// iOS implementation of the PrivateAiOakSessionDriver.
// Evaluates the Oak Session Service in-process since iOS doesn't support
// utility processes.
class PrivateAiOakSessionDriverIOS : public PrivateAiOakSessionDriver {
 public:
  PrivateAiOakSessionDriverIOS();
  ~PrivateAiOakSessionDriverIOS() override;

  PrivateAiOakSessionDriverIOS(const PrivateAiOakSessionDriverIOS&) = delete;
  PrivateAiOakSessionDriverIOS& operator=(const PrivateAiOakSessionDriverIOS&) =
      delete;

  // private_ai::PrivateAiOakSessionDriver overrides:
  mojo::Remote<mojom::OakSession> BindOakSessionService() override;
  mojo::Remote<mojom::OakTlsSession> BindOakTlsSessionService() override;

  size_t GetActiveSessionsCountForTesting() const {
    return services_.size() + tls_services_.size();
  }

 private:
  void OnServiceDisconnected(OakSessionService* service);
  void OnTlsServiceDisconnected(OakTlsSessionService* service);

  std::map<OakSessionService*, std::unique_ptr<OakSessionService>> services_;
  std::map<OakTlsSessionService*, std::unique_ptr<OakTlsSessionService>>
      tls_services_;

  base::WeakPtrFactory<PrivateAiOakSessionDriverIOS> weak_ptr_factory_{this};
};

}  // namespace private_ai

#endif  // COMPONENTS_PRIVATE_AI_IOS_PRIVATE_AI_OAK_SESSION_DRIVER_IOS_H_
