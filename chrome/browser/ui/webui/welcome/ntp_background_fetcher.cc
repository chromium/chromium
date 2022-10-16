// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/welcome/ntp_background_fetcher.h"

#include <utility>

#include "base/memory/ref_counted_memory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/search/background/ntp_backgrounds.h"
#include "net/base/load_flags.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "url/gurl.h"

namespace welcome {

NtpBackgroundFetcher::NtpBackgroundFetcher(
    size_t index,
    content::WebUIDataSource::GotDataCallback callback)
    : index_(index), callback_(std::move(callback)) {
  DCHECK(callback_);
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("nux_ntp_background_preview", R"(
        semantics {
          sender: "Navi Onboarding NTP background module"
          description:
            "As part of the Navi Onboarding flow, the NTP background module "
            "allows users to preview what a custom background for the "
            "New Tab Page would look like. The list of available backgrounds "
            "is manually whitelisted."
          trigger:
            "The user selects an image to preview."
          data:
            "User-selected image URL."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting:
            "This feature cannot be disabled by settings, but it is only "
            "triggered by a user action."
          policy_exception_justification: "Not implemented."
        })");

  auto backgrounds = GetNtpBackgrounds();

  if (index_ >= backgrounds.size()) {
    OnFetchCompleted(nullptr);
    return;
  }

  GURL url = backgrounds[index_];
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = url;
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  simple_loader_ = network::SimpleURLLoader::Create(std::move(resource_request),
                                                    traffic_annotation);

  network::mojom::URLLoaderFactory* loader_factory =
      g_browser_process->system_network_context_manager()
          ->GetURLLoaderFactory();
  simple_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      loader_factory, base::BindOnce(&NtpBackgroundFetcher::OnFetchCompleted,
                                     base::Unretained(this)));
}

NtpBackgroundFetcher::~NtpBackgroundFetcher() = default;

void NtpBackgroundFetcher::OnFetchCompleted(
    std::unique_ptr<std::string> response_body) {
  if (response_body) {
    std::move(callback_).Run(base::MakeRefCounted<base::RefCountedString>(
        std::move(*response_body)));
  } else {
    std::move(callback_).Run(base::MakeRefCounted<base::RefCountedBytes>());
  }
}

}  // namespace welcome
