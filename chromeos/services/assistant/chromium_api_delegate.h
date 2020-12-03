// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_ASSISTANT_CHROMIUM_API_DELEGATE_H_
#define CHROMEOS_SERVICES_ASSISTANT_CHROMIUM_API_DELEGATE_H_

#include "chromeos/services/assistant/chromium_http_connection.h"

#include <memory>

#include "base/macros.h"
#include "libassistant/shared/internal_api/fuchsia_api_helper.h"

namespace network {
class PendingSharedURLLoaderFactory;
}  // namespace network

namespace chromeos {
namespace assistant {

class ChromiumHttpConnectionFactory;

class ChromiumApiDelegate : public assistant_client::FuchsiaApiDelegate {
 public:
  explicit ChromiumApiDelegate(
      std::unique_ptr<network::PendingSharedURLLoaderFactory>
          pending_url_loader_factory);
  ~ChromiumApiDelegate() override;
  // assistant_client::FuchsiaApiDelegate overrides:
  assistant_client::HttpConnectionFactory* GetHttpConnectionFactory() override;

 private:
  ChromiumHttpConnectionFactory http_connection_factory_;
  DISALLOW_COPY_AND_ASSIGN(ChromiumApiDelegate);
};

}  // namespace assistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_ASSISTANT_CHROMIUM_API_DELEGATE_H_
