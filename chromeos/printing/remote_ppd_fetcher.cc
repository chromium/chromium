// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/printing/remote_ppd_fetcher.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/task/sequenced_task_runner.h"
#include "net/base/net_errors.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "url/gurl.h"

namespace {

void InvokeCallbackWithContents(chromeos::RemotePpdFetcher::FetchCallback cb,
                                std::string contents) {
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(cb),
                     chromeos::RemotePpdFetcher::FetchResultCode::kSuccess,
                     std::move(contents)));
}

void InvokeCallbackWithError(chromeos::RemotePpdFetcher::FetchCallback cb,
                             chromeos::RemotePpdFetcher::FetchResultCode code) {
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(cb), code, std::string()));
}

void OnRemoteUrlLoaded(chromeos::RemotePpdFetcher::FetchCallback cb,
                       std::unique_ptr<network::SimpleURLLoader> loader,
                       std::optional<std::string> contents) {
  if (loader->NetError() != net::Error::OK) {
    LOG(WARNING) << "Failed to fetch PPD from remote URL. Network error code: "
                 << loader->NetError();
    InvokeCallbackWithError(
        std::move(cb),
        chromeos::RemotePpdFetcher::FetchResultCode::kNetworkError);
    return;
  }
  InvokeCallbackWithContents(std::move(cb), std::move(contents).value());
}
}  // namespace

namespace chromeos {

class RemotePpdFetcherImpl : public RemotePpdFetcher {
 public:
  explicit RemotePpdFetcherImpl(
      base::RepeatingCallback<network::mojom::URLLoaderFactory*()>
          loader_factory_dispenser)
      : loader_factory_dispenser_(loader_factory_dispenser) {}

  void Fetch(const GURL& url, FetchCallback cb) const override {
    DCHECK(url.SchemeIsHTTPOrHTTPS());

    auto request = std::make_unique<network::ResourceRequest>();
    request->url = url;
    net::NetworkTrafficAnnotationTag traffic_annotation =
        net::DefineNetworkTrafficAnnotation("admin_specified_ppd_fetch", R"(
          semantics {
            sender: "Printer Configuration"
            description:
              "This component downloads PPD files required to setup/install "
              "printers provided via enterprise policies when the URL of the "
              "PPD file is specified by administrator in the policy."
            trigger: "On printer setup in ChromeOS."
            user_data: {
              type: NONE
            }
            data: "None"
            destination: OTHER
            internal: {
              contacts: {
                email: "ust@google.com"
              }
            }
            last_reviewed: "2023-12-27"
          }
          policy {
            cookies_allowed: NO
            setting:
              "Admins must make sure that none of the printers in "
              "'Devices > Chrome > Printers' is configured to use a custom "
              "PPD file."
            chrome_policy {
              PrintersBulkConfiguration: {
                PrintersBulkConfiguration: ""
              }
            }
            chrome_device_policy {
              # DevicePrinters
              device_printers: {
                external_policy: ""
              }
            }
          })");

    auto url_loader = network::SimpleURLLoader::Create(std::move(request),
                                                       traffic_annotation);
    network::SimpleURLLoader* url_loader_ptr = url_loader.get();
    url_loader_ptr->DownloadToString(
        loader_factory_dispenser_.Run(),
        base::BindOnce(&OnRemoteUrlLoaded, std::move(cb),
                       std::move(url_loader)),
        network::SimpleURLLoader::kMaxBoundedStringDownloadSize);
  }

  base::RepeatingCallback<network::mojom::URLLoaderFactory*()>
      loader_factory_dispenser_;
};

std::unique_ptr<RemotePpdFetcher> RemotePpdFetcher::Create(
    base::RepeatingCallback<network::mojom::URLLoaderFactory*()>
        loader_factory_dispenser) {
  return std::make_unique<RemotePpdFetcherImpl>(
      std::move(loader_factory_dispenser));
}

}  // namespace chromeos
