// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/picker/picker_client_impl.h"

#include <memory>
#include <string>
#include <utility>

#include "ash/picker/picker_controller.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/ash_web_view_impl.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "components/user_manager/user_manager.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace {

scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory() {
  auto* const active_user = user_manager::UserManager::Get()->GetActiveUser();
  auto* const profile = Profile::FromBrowserContext(
      ash::BrowserContextHelper::Get()->GetBrowserContextByUser(active_user));
  return profile->GetURLLoaderFactory();
}

void OnGifDownloaded(PickerClientImpl::DownloadGifToStringCallback callback,
                     std::unique_ptr<network::SimpleURLLoader> simple_loader,
                     std::unique_ptr<std::string> response_body) {
  if (simple_loader->NetError() == net::OK && response_body) {
    std::move(callback).Run(*response_body);
    return;
  }
  // TODO: b/316936723 - Add better handling of errors.
  std::move(callback).Run(std::string());
}

}  // namespace

PickerClientImpl::PickerClientImpl(ash::PickerController* controller)
    : controller_(controller) {
  controller_->SetClient(this);
}

PickerClientImpl::~PickerClientImpl() {
  controller_->SetClient(nullptr);
}

std::unique_ptr<ash::AshWebView> PickerClientImpl::CreateWebView(
    const ash::AshWebView::InitParams& params) {
  return std::make_unique<AshWebViewImpl>(params);
}

void PickerClientImpl::DownloadGifToString(
    const GURL& url,
    DownloadGifToStringCallback callback) {
  // For now, only allow gifs from tenor.
  // TODO: b/316936723 - Once we know what gifs the picker might show, consider
  // making the method parameters more specific to allowed gif sources.
  CHECK(url.DomainIs("media.tenor.com") && url.SchemeIs(url::kHttpsScheme));

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = url;
  resource_request->method = "GET";
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;

  constexpr net::NetworkTrafficAnnotationTag kTrafficAnnotation =
      net::DefineNetworkTrafficAnnotation("chromeos_picker_gif_fetcher", R"(
      semantics {
        sender: "ChromeOS Picker"
        description:
          "Fetches a GIF from tenor for the specified url. This is used to"
          "show a preview of the GIF in the ChromeOS picker, which users can"
          "select to insert the GIF into supported textfields."
        trigger:
          "Triggered when the user opens the ChromeOS picker."
        data:
          "A GIF ID to specify the GIF to fetch."
        destination: GOOGLE_OWNED_SERVICE
        internal {
          contacts {
              email: "e14s-eng@google.com"
          }
        }
        user_data {
          type: NONE
        }
        last_reviewed: "2024-01-03"
      }
      policy {
        cookies_allowed: NO
        setting:
          "No setting. Users must take explicit action to trigger the feature."
        policy_exception_justification:
          "Not implemented, not considered useful. This request is part of a "
          "flow which is user-initiated."
      }
  )");
  auto loader = network::SimpleURLLoader::Create(std::move(resource_request),
                                                 kTrafficAnnotation);
  auto* loader_ptr = loader.get();
  loader_ptr->DownloadToString(
      GetURLLoaderFactory().get(),
      base::BindOnce(&OnGifDownloaded, std::move(callback), std::move(loader)),
      network::SimpleURLLoader::kMaxBoundedStringDownloadSize);
}
