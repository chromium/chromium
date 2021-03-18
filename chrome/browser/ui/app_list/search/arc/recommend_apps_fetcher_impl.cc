// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/arc/recommend_apps_fetcher_impl.h"

#include <cstdint>
#include <iomanip>

#include "base/base64url.h"
#include "base/callback_forward.h"
#include "base/json/json_reader.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "chrome/browser/ui/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ui/app_list/search/arc/recommend_apps_fetcher_delegate.h"
#include "net/base/load_flags.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"

namespace app_list {

namespace {

constexpr const char kGetAppListUrl[] =
    "https://android.clients.google.com/fdfe/chrome/"
    "getfastreinstallappslist?cfecu=false";

// These are error codes from the json response of the API.
constexpr int kResponseErrorNotEnoughApps = 5;
constexpr int kResponseErrorNotFirstTimeChromebookUser = 6;

// The response starts with a prefix ")]}'". This needs to be removed before
// further parsing.
constexpr base::StringPiece kJsonXssPreventionPrefix = ")]}'";

constexpr base::TimeDelta kDownloadTimeOut = base::TimeDelta::FromMinutes(1);

constexpr const int64_t kMaxDownloadBytes = 1024 * 1024;  // 1Mb

// Convert a decimal number to a hexadecimal number. Return a string as result.
std::string HexEncode(int64_t id) {
  std::ostringstream string_stream;
  string_stream << std::setbase(16) << id;
  return string_stream.str();
}

void OnGetAndroidId(bool* get_android_id_successfully,
                    int64_t* android_id,
                    base::OnceCallback<void()> callback,
                    bool ok,
                    int64_t id) {
  // TODO(thanhdng): Add a UMA histogram here.
  *get_android_id_successfully = ok;
  *android_id = id;
  std::move(callback).Run();
}

}  // namespace

RecommendAppsFetcherImpl::RecommendAppsFetcherImpl(
    RecommendAppsFetcherDelegate* delegate,
    network::mojom::URLLoaderFactory* url_loader_factory)
    : delegate_(delegate), url_loader_factory_(url_loader_factory) {
}

RecommendAppsFetcherImpl::~RecommendAppsFetcherImpl() = default;

void RecommendAppsFetcherImpl::StartDownload() {
  if (!get_android_id_successfully_) {
    // Retry getting android ID up to 5 times after giving up downloading.
    if (num_get_android_id_retry_ < 5) {
      num_get_android_id_retry_++;
      arc::GetAndroidId(base::BindOnce(
          &OnGetAndroidId, &get_android_id_successfully_, &android_id_,
          base::BindOnce(&RecommendAppsFetcherImpl::StartDownload,
                         weak_ptr_factory_.GetWeakPtr())));
    }
    return;
  }

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("play_recommended_apps_reinstall", R"(
        semantics {
          sender: "ChromeOS Launcher"
          description:
            "ChromeOS launcher provides app reinstall suggestions for user."
            "This request downloads the suggestions' metadata using user's "
            "Android ID."
          trigger:
            "When user open the launcher."
          data:
            "URL of the Google Play API."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: YES
          cookie_store: "user"
          setting:
            "NA"
          chrome_policy {
            SuggestedContentEnabled {
             SuggestedContentEnabled: true
            }
          }
        })");

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = GURL(kGetAppListUrl);
  resource_request->method = "GET";
  resource_request->load_flags =
      net::LOAD_BYPASS_CACHE | net::LOAD_DISABLE_CACHE;

  resource_request->headers.SetHeader(net::HttpRequestHeaders::kContentType,
                                      "application/json");
  resource_request->headers.SetHeader("X-DFE-Device-Id",
                                      HexEncode(android_id_));

  start_time_ = base::TimeTicks::Now();
  app_list_loader_ = network::SimpleURLLoader::Create(
      std::move(resource_request), traffic_annotation);
  // Retry up to three times if network changes are detected during the
  // download.
  app_list_loader_->SetRetryOptions(
      3, network::SimpleURLLoader::RETRY_ON_NETWORK_CHANGE);
  app_list_loader_->DownloadToString(
      url_loader_factory_,
      base::BindOnce(&RecommendAppsFetcherImpl::OnDownloaded,
                     base::Unretained(this)),
      kMaxDownloadBytes);

  // Abort the download attempt if it takes longer than one minute.
  download_timer_.Start(FROM_HERE, kDownloadTimeOut, this,
                        &RecommendAppsFetcherImpl::OnDownloadTimeout);
}

void RecommendAppsFetcherImpl::OnDownloadTimeout() {
  // Destroy the fetcher, which will abort the download attempt.
  app_list_loader_.reset();

  // TODO(thanhdng): Add a UMA histogram here recording the time difference.
  delegate_->OnLoadError();
}

void RecommendAppsFetcherImpl::OnDownloaded(
    std::unique_ptr<std::string> response_body) {
  download_timer_.Stop();

  // TODO(thanhdng): Add a UMA histogram here recording the time difference.

  std::unique_ptr<network::SimpleURLLoader> loader(std::move(app_list_loader_));
  int response_code = 0;
  if (!loader->ResponseInfo() || !loader->ResponseInfo()->headers) {
    delegate_->OnLoadError();
    return;
  }
  response_code = loader->ResponseInfo()->headers->response_code();
  // TODO(thanhndng): Add a UMA histogram here recording the response code.

  // If the recommended app list could not be downloaded, show an error message
  // to the user.
  if (!response_body || response_body->empty()) {
    delegate_->OnLoadError();
    return;
  }

  // If the recommended app list were downloaded successfully, show them to
  // the user.
  //
  base::StringPiece response_body_json(*response_body);
  if (base::StartsWith(response_body_json, kJsonXssPreventionPrefix))
    response_body_json.remove_prefix(kJsonXssPreventionPrefix.length());
  base::Optional<base::Value> output = ParseResponse(response_body_json);
  if (!output.has_value()) {
    // TODO(thanhdng): Add a UMA histogram here.
    delegate_->OnParseResponseError();
    return;
  }

  delegate_->OnLoadSuccess(std::move(output.value()));
}

base::Optional<base::Value> RecommendAppsFetcherImpl::ParseResponse(
    base::StringPiece response) {
  base::JSONReader::ValueWithError parsed_json =
      base::JSONReader::ReadAndReturnValueWithError(response);

  if (!parsed_json.value ||
      (!parsed_json.value->is_list() && !parsed_json.value->is_dict())) {
    LOG(ERROR) << "Error parsing response JSON: " << parsed_json.error_message;
    // TODO(thanhdng): Add a UMA histogram here.
    return base::nullopt;
  }

  // If the response is a dictionary, it is an error message in the
  // following format:
  //   {"Error code":"error code","Error message":"Error message"}
  if (parsed_json.value->is_dict()) {
    const base::Value* response_error_code_value =
        parsed_json.value->FindKeyOfType("Error code",
                                         base::Value::Type::STRING);

    if (!response_error_code_value) {
      LOG(ERROR) << "Unable to find error code: response="
                 << response.substr(0, 128);
      // TODO(thanhdng): Add a UMA histogram here.
      return base::nullopt;
    }

    base::StringPiece response_error_code_str =
        response_error_code_value->GetString();
    int response_error_code = 0;
    if (!base::StringToInt(response_error_code_str, &response_error_code)) {
      LOG(WARNING) << "Unable to parse error code: " << response_error_code_str;
      // TODO(thanhdng): Add a UMA histogram here.
      return base::nullopt;
    }

    if (response_error_code == kResponseErrorNotFirstTimeChromebookUser) {
      NOTREACHED();
    } else if (response_error_code == kResponseErrorNotEnoughApps) {
      // TODO(thanhdng): Add a UMA histogram here.
    } else {
      LOG(WARNING) << "Unknown error code: " << response_error_code_str;
      // TODO(thanhdng): Add a UMA histogram here.
    }

    return base::nullopt;
  }

  // Otherwise, the response should return a list of apps.
  base::Value::ConstListView app_list = parsed_json.value->GetList();
  if (app_list.empty()) {
    DVLOG(1) << "No app in the response.";
    // TODO(thanhdng): Add a UMA histogram here.
    return base::nullopt;
  }

  base::Value output(base::Value::Type::LIST);
  for (auto& item : app_list) {
    base::Value output_map(base::Value::Type::DICTIONARY);

    if (!item.is_dict()) {
      DVLOG(1) << "Cannot parse item.";
      continue;
    }

    // Retrieve the app title.
    const base::Value* title =
        item.FindPathOfType({"title_", "name_"}, base::Value::Type::STRING);
    if (title)
      output_map.SetKey("name", base::Value(title->GetString()));

    // Retrieve the package name.
    const base::Value* package_name =
        item.FindPathOfType({"id_", "id_"}, base::Value::Type::STRING);
    if (package_name)
      output_map.SetKey("package_name", base::Value(package_name->GetString()));

    // Retrieve the icon URL for the app.
    //
    // The name "privateDoNotAccessOrElseSafeUrlWrappedValue_" here is because
    // it is a direct serialization from the proto message. The value has been
    // sanitized so it is regarded as a safe URL. In general, if the response is
    // a protobuf, we should not directly access this field but use the wrapper
    // method getSafeUrlString() to read it. In our case, we don't have the
    // option other than access it directly.
    const base::Value* icon_url = item.FindPathOfType(
        {"icon_", "url_", "privateDoNotAccessOrElseSafeUrlWrappedValue_"},
        base::Value::Type::STRING);
    if (icon_url)
      output_map.SetKey("icon", base::Value(icon_url->GetString()));

    if (output_map.DictEmpty()) {
      DVLOG(1) << "Invalid app item.";
      continue;
    }

    output.Append(std::move(output_map));
  }

  // TODO(thanhdng): Add a UMA histogram here to record the parse have completed
  // successfully.
  return output;
}

}  // namespace app_list
