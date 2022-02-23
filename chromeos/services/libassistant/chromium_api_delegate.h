// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_LIBASSISTANT_CHROMIUM_API_DELEGATE_H_
#define CHROMEOS_SERVICES_LIBASSISTANT_CHROMIUM_API_DELEGATE_H_

#include <memory>

#include "build/buildflag.h"
#include "chromeos/assistant/internal/buildflags.h"
#include "chromeos/assistant/internal/libassistant/shared_headers.h"
#include "chromeos/services/libassistant/chromium_http_connection.h"

namespace network {
class PendingSharedURLLoaderFactory;
}  // namespace network

namespace chromeos {
namespace libassistant {

class ChromiumHttpConnectionFactory;

// TODO(b/195985225): `ChromeOSApiDelegate` is not supported in the prebuilt
// library.
#if BUILDFLAG(IS_PREBUILT_LIBASSISTANT)
class ChromiumApiDelegate {
 public:
  explicit ChromiumApiDelegate(
      std::unique_ptr<network::PendingSharedURLLoaderFactory>
          pending_url_loader_factory);

  ChromiumApiDelegate(const ChromiumApiDelegate&) = delete;
  ChromiumApiDelegate& operator=(const ChromiumApiDelegate&) = delete;

  ~ChromiumApiDelegate();

 private:
  ChromiumHttpConnectionFactory http_connection_factory_;
};
#else
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

  void OverrideDoNotDisturb(bool do_not_disturb_enabled) override {}

 private:
  ChromiumHttpConnectionFactory http_connection_factory_;
};
#endif  // BUILDFLAG(IS_PREBUILT_LIBASSISTANT)

}  // namespace libassistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_LIBASSISTANT_CHROMIUM_API_DELEGATE_H_
