// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/notebooks/internal/notebooks_network_service_impl.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/json/json_writer.h"
#include "base/strings/escape.h"
#include "base/strings/strcat.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/endpoint_fetcher/endpoint_fetcher.h"
#include "components/notebooks/public/features.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "net/base/url_util.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/device_info.h"
#endif

using endpoint_fetcher::EndpointFetcher;
using endpoint_fetcher::EndpointResponse;

namespace notebooks {

namespace {

const char kNotebookDisplayNameFieldName[] = "display_name";
const char kExternalIdentifierFieldName[] = "external_identifier";
const char kExternalIdentifierIdFieldName[] = "id";
const char kProvenanceOriginProductIdQueryParamName[] =
    "provenance.origin_product_type";
const char kClientInfoApplicationPlatformQueryParamName[] =
    "provenance.client_info.application_platform";
const char kClientInfoDeviceQueryParamName[] = "provenance.client_info.device";

// TODO(crbug.com/531809229): Update policy list for traffic annotations.
const net::NetworkTrafficAnnotationTag kCreateNotebookTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("notebooks_service_create_notebook",
                                        R"(
      semantics {
        sender: "Notebooks Service Create Notebook"
        description:
          "Chrome feature that creates a notebook, which is a container for "
          "tab and user-uploaded sources providing functionality for users "
          "to make queries and generate artifacts based on those sources."
        trigger: "User upgrades tab group to notebook."
        data:
          "The OAuth token for the signed in account and the user-defined "
          "display name for the notebook."
        destination: GOOGLE_OWNED_SERVICE
        user_data {
          type: ACCESS_TOKEN
          type: USER_CONTENT
        }
        last_reviewed: "2026-07-15"
        internal {
          contacts {
            email: "chrome-ai-productivity-eng@google.com"
          }
          contacts {
            email: "woodchip@chromium.org"
          }
        }
      }
      policy {
        cookies_allowed: NO
        setting:
          "This feature can be disabled in Chrome settings by toggling off "
          "'History and tabs' under 'You and Google' > 'In your Google "
          "Account'."
        chrome_policy {
          SyncDisabled {
            SyncDisabled: true
          }
        }
        chrome_policy {
          SyncTypesListDisabled {
            SyncTypesListDisabled {
              entries: "tabs"
            }
          }
        }
        chrome_policy {
          GenAiDefaultSettings {
            GenAiDefaultSettings: 2
          }
        }
      })");

const net::NetworkTrafficAnnotationTag kCreateNotebookSourceTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation(
        "notebooks_service_create_notebook_source",
        R"(
      semantics {
        sender: "Notebooks Service Create Notebook Source"
        description:
          "Chrome feature that adds a tab source to a notebook. A notebook is "
          "a container for tab and user-uploaded sources that provides "
          "functionality for users to make queries and generate artifacts "
          "based on said sources."
        trigger: "User opens a new tab inside an existing notebook."
        data:
          "The OAuth token for the signed in account, the tab identifier "
          "string, and the string identifier for the notebook to which the "
          "source is being added."
        destination: GOOGLE_OWNED_SERVICE
        user_data {
          type: ACCESS_TOKEN
          type: SENSITIVE_URL
        }
        last_reviewed: "2026-07-20"
        internal {
          contacts {
            email: "chrome-ai-productivity-eng@google.com"
          }
          contacts {
            email: "woodchip@chromium.org"
          }
        }
      }
      policy {
        cookies_allowed: NO
        setting:
          "This feature can be disabled in Chrome settings by toggling off "
          "'History and tabs' under 'You and Google' > 'In your Google "
          "Account'."
        chrome_policy {
          SyncDisabled {
            SyncDisabled: true
          }
        }
        chrome_policy {
          SyncTypesListDisabled {
            SyncTypesListDisabled {
              entries: "tabs"
            }
          }
        }
        chrome_policy {
          GenAiDefaultSettings {
            GenAiDefaultSettings: 2
          }
        }
      })");

enum class Device {
  kOther,
  kDesktop,
  kDesktopAndroid,
  kMobileAndroid,
  kMobileIos
};

enum class ApplicationPlatform { kWeb, kNative };

Device GetDevice() {
#if BUILDFLAG(IS_ANDROID)
  if (base::android::device_info::is_desktop()) {
    return Device::kDesktopAndroid;
  }
  return Device::kMobileAndroid;
#elif BUILDFLAG(IS_IOS)
  return Device::kMobileIos;
#elif BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX)
  return Device::kDesktop;
#else
  return Device::kOther;
#endif
}

std::string_view DeviceToString(Device device) {
  switch (device) {
    case Device::kDesktop:
      return "DESKTOP";
    case Device::kDesktopAndroid:
      return "DESKTOP_ANDROID";
    case Device::kMobileAndroid:
      return "MOBILE_ANDROID";
    case Device::kMobileIos:
      return "MOBILE_IOS";
    case Device::kOther:
      return "OTHER";
  }
}

ApplicationPlatform GetApplicationPlatform() {
#if BUILDFLAG(IS_ANDROID)
  if (base::android::device_info::is_desktop()) {
    return ApplicationPlatform::kWeb;
  }
  return ApplicationPlatform::kNative;
#elif BUILDFLAG(IS_IOS)
  return ApplicationPlatform::kNative;
#else
  return ApplicationPlatform::kWeb;
#endif
}

std::string_view ApplicationPlatformToString(ApplicationPlatform platform) {
  switch (platform) {
    case ApplicationPlatform::kNative:
      return "NATIVE";
    case ApplicationPlatform::kWeb:
      return "WEB";
  }
}
}  // namespace

std::unique_ptr<EndpointFetcher>
NotebooksNetworkServiceImpl::CreateEndpointFetcher(
    const GURL& url,
    const std::string& post_data,
    const net::NetworkTrafficAnnotationTag& annotation_tag) {
  return std::make_unique<EndpointFetcher>(
      url_loader_factory_, identity_manager_,
      EndpointFetcher::RequestParams::Builder(
          endpoint_fetcher::HttpMethod::kPost, annotation_tag)
          .SetCredentialsMode(endpoint_fetcher::CredentialsMode::kOmit)
          .SetAuthType(endpoint_fetcher::OAUTH)
          .SetConsentLevel(signin::ConsentLevel::kSignin)
          .SetUrl(url)
          .SetOAuthConsumerId(signin::OAuthConsumerId::kNotebooksService)
          .SetPostData(post_data)
          .Build());
}

GURL NotebooksNetworkServiceImpl::ConstructServiceURL(std::string_view path) {
  std::string service_url;
  std::string origin_product_id;
  if (base::FeatureList::IsEnabled(features::kNotebooks)) {
    service_url = features::kNotebooksApiBaseURL.Get();
    origin_product_id = features::kProvenanceOriginProductId.Get();
  }

  if (service_url.empty() || origin_product_id.empty()) {
    return GURL();
  }

  if (!path.empty()) {
    service_url = base::StrCat({service_url, path});
  }

  GURL url(service_url);
  if (!url.is_valid()) {
    return GURL();
  }
  url = net::AppendOrReplaceQueryParameter(
      url, kProvenanceOriginProductIdQueryParamName, origin_product_id);
  url = net::AppendOrReplaceQueryParameter(
      url, kClientInfoApplicationPlatformQueryParamName,
      ApplicationPlatformToString(GetApplicationPlatform()));
  url = net::AppendOrReplaceQueryParameter(url, kClientInfoDeviceQueryParamName,
                                           DeviceToString(GetDevice()));
  return url;
}

NotebooksNetworkServiceImpl::NotebooksNetworkServiceImpl(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    signin::IdentityManager* identity_manager)
    : url_loader_factory_(std::move(url_loader_factory)),
      identity_manager_(identity_manager) {
  CHECK(identity_manager);
}

NotebooksNetworkServiceImpl::~NotebooksNetworkServiceImpl() = default;

// NotebooksNetworkService Impl.
void NotebooksNetworkServiceImpl::CreateNotebook(
    std::string_view notebook_display_name,
    NetworkLoaderCallback callback) {
  GURL service_url = ConstructServiceURL("");
  if (!service_url.is_valid()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), nullptr));
    return;
  }

  // TODO(crbug.com/531809229): Set a request timeout.
  base::DictValue request;
  request.Set(kNotebookDisplayNameFieldName, notebook_display_name);
  std::optional<std::string> request_string = base::WriteJson(request);
  FetchInternal(service_url, request_string.value_or(""),
                kCreateNotebookTrafficAnnotation, std::move(callback));
}

void NotebooksNetworkServiceImpl::CreateNotebookSource(
    std::string_view notebook_id,
    std::string_view source_id,
    NetworkLoaderCallback callback) {
  const std::string& source_url_suffix =
      features::kNotebookSourceURLSuffix.Get();
  GURL service_url;
  bool required_fields_present =
      !(notebook_id.empty() || source_id.empty() || source_url_suffix.empty());
  if (required_fields_present) {
    service_url = ConstructServiceURL(base::StrCat(
        {"/", base::EscapeAllExceptUnreserved(notebook_id), "/",
         source_url_suffix, "/", base::EscapeAllExceptUnreserved(source_id)}));
  }
  if (!required_fields_present || !service_url.is_valid()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), nullptr));
    return;
  }

  base::DictValue external_id;
  external_id.Set(kExternalIdentifierIdFieldName, source_id);

  // TODO(crbug.com/531809229): Add actual source content such as MIME type or
  // page URL.
  base::DictValue request;
  request.Set(kExternalIdentifierFieldName, std::move(external_id));

  std::optional<std::string> request_string = base::WriteJson(request);
  FetchInternal(service_url, request_string.value_or(""),
                kCreateNotebookSourceTrafficAnnotation, std::move(callback));
}

void NotebooksNetworkServiceImpl::FetchInternal(
    const GURL& url,
    const std::string& post_data,
    const net::NetworkTrafficAnnotationTag& annotation_tag,
    NetworkLoaderCallback callback) {
  std::unique_ptr<endpoint_fetcher::EndpointFetcher> endpoint_fetcher =
      CreateEndpointFetcher(url, post_data, annotation_tag);
  auto* const fetcher_ptr = endpoint_fetcher.get();
  fetcher_ptr->Fetch(base::BindPostTaskToCurrentDefault(
      base::BindOnce(&NotebooksNetworkServiceImpl::OnDownloadComplete,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     std::move(endpoint_fetcher))));
}

void NotebooksNetworkServiceImpl::OnDownloadComplete(
    NetworkLoaderCallback callback,
    std::unique_ptr<endpoint_fetcher::EndpointFetcher> fetcher,
    std::unique_ptr<endpoint_fetcher::EndpointResponse> response) {
  if (!response) {
    std::move(callback).Run(nullptr);
    return;
  }
  NetworkLoaderStatus status = NetworkLoaderStatus::kPersistentFailure;
  if (response->http_status_code / 100 == 2) {
    status = NetworkLoaderStatus::kSuccess;
  } else if (response->http_status_code >= 500 ||
             response->error_type ==
                 endpoint_fetcher::FetchErrorType::kNetError) {
    status = NetworkLoaderStatus::kTransientFailure;
  }

  std::move(callback).Run(std::make_unique<LoadResult>(
      std::move(response->response), status, response->http_status_code));
}

}  // namespace notebooks
