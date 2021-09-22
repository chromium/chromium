// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_LIBASSISTANT_CHROMIUM_API_DELEGATE_H_
#define CHROMEOS_SERVICES_LIBASSISTANT_CHROMIUM_API_DELEGATE_H_

#include "chromeos/services/libassistant/chromium_http_connection.h"

#include <memory>

#include "base/macros.h"
#include "build/buildflag.h"
#include "chromeos/assistant/internal/buildflags.h"
#include "libassistant/shared/internal_api/fuchsia_api_helper.h"

namespace network {
class PendingSharedURLLoaderFactory;
}  // namespace network

namespace chromeos {
namespace libassistant {

class ChromiumHttpConnectionFactory;

class ChromiumApiDelegate : public assistant_client::ChromeOSApiDelegate {
 public:
  explicit ChromiumApiDelegate(
      std::unique_ptr<network::PendingSharedURLLoaderFactory>
          pending_url_loader_factory);

  ChromiumApiDelegate(const ChromiumApiDelegate&) = delete;
  ChromiumApiDelegate& operator=(const ChromiumApiDelegate&) = delete;

  ~ChromiumApiDelegate() override;
  // assistant_client::FuchsiaApiDelegate overrides:
  assistant_client::HttpConnectionFactory* GetHttpConnectionFactory() override;

#if BUILDFLAG(BUILD_LIBASSISTANT_152S)
  void OverrideDoNotDisturb(bool do_not_disturb_enabled) override {}
#endif  // BUILD_LIBASSISTANT_152S

 private:
  ChromiumHttpConnectionFactory http_connection_factory_;
};

}  // namespace libassistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_LIBASSISTANT_CHROMIUM_API_DELEGATE_H_
