// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/web_resource/web_resource_service.h"

#include <memory>

#include "base/bind.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/google/core/common/google_util.h"
#include "components/prefs/pref_service.h"
#include "net/base/load_flags.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "url/gurl.h"

// No anonymous namespace, because const variables automatically get internal
// linkage.
const char kUnexpectedJSONFormatError[] =
    "Data from web resource server does not have expected format.";

namespace web_resource {

WebResourceService::WebResourceService(
    PrefService* prefs,
    const GURL& web_resource_server,
    const std::string& application_locale,
    const char* last_update_time_pref_name,
    int start_fetch_delay_ms,
    int cache_update_delay_ms,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const char* disable_network_switch,
    const net::NetworkTrafficAnnotationTag& traffic_annotation,
    ResourceRequestAllowedNotifier::NetworkConnectionTrackerGetter
        network_connection_tracker_getter)
    : prefs_(prefs),
      resource_request_allowed_notifier_(new ResourceRequestAllowedNotifier(
          prefs,
          disable_network_switch,
          std::move(network_connection_tracker_getter))),
      fetch_scheduled_(false),
      in_fetch_(false),
      web_resource_server_(web_resource_server),
      application_locale_(application_locale),
      last_update_time_pref_name_(last_update_time_pref_name),
      start_fetch_delay_ms_(start_fetch_delay_ms),
      cache_update_delay_ms_(cache_update_delay_ms),
      url_loader_factory_(url_loader_factory),
      traffic_annotation_(traffic_annotation) {
  resource_request_allowed_notifier_->Init(this, false /* leaky */);
  DCHECK(prefs);
}

void WebResourceService::StartAfterDelay() {
  // If resource requests are not allowed, we'll get a callback when they are.
  if (resource_request_allowed_notifier_->ResourceRequestsAllowed())
    OnResourceRequestsAllowed();
}

WebResourceService::~WebResourceService() = default;

void WebResourceService::OnSimpleLoaderComplete(
    std::unique_ptr<std::string> response_body) {
  simple_url_loader_.reset();
  if (response_body) {
    // Calls EndFetch() on completion.
    // Full JSON parsing might spawn a utility process (for security).
    // To limit the the number of simultaneously active processes
    // (on Android in particular) we short-cut the full parsing in the case of
    // trivially "empty" JSONs.
    if (response_body->empty() || *response_body == "{}") {
      OnJsonParsed(data_decoder::DataDecoder::ValueOrError::Value(
          base::Value(base::Value::Type::DICTIONARY)));
    } else {
      data_decoder::DataDecoder::ParseJsonIsolated(
          *response_body, base::BindOnce(&WebResourceService::OnJsonParsed,
                                         weak_ptr_factory_.GetWeakPtr()));
    }
  } else {
    // Don't parse data if attempt to download was unsuccessful.
    // Stop loading new web resource data, and silently exit.
    // We do not end up invoking OnJsonParsed(), so we need to call EndFetch()
    // ourselves.
    EndFetch();
  }
}

// Delay initial load of resource data into cache so as not to interfere
// with startup time.
void WebResourceService::ScheduleFetch(int64_t delay_ms) {
  if (fetch_scheduled_)
    return;
  fetch_scheduled_ = true;
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&WebResourceService::StartFetch,
                     weak_ptr_factory_.GetWeakPtr()),
      base::TimeDelta::FromMilliseconds(delay_ms));
}

void WebResourceService::SetResourceRequestAllowedNotifier(
    std::unique_ptr<ResourceRequestAllowedNotifier> notifier) {
  resource_request_allowed_notifier_ = std::move(notifier);
  resource_request_allowed_notifier_->Init(this, false /* leaky */);
}

bool WebResourceService::GetFetchScheduled() const {
  return fetch_scheduled_;
}

// Initializes the fetching of data from the resource server.  Data
// load calls OnSimpleLoaderComplete.
void WebResourceService::StartFetch() {
  // Set to false so that next fetch can be scheduled after this fetch or
  // if we receive notification that resource is allowed.
  fetch_scheduled_ = false;
  // Check whether fetching is allowed.
  if (!resource_request_allowed_notifier_->ResourceRequestsAllowed())
    return;

  // First, put our next cache load on the MessageLoop.
  ScheduleFetch(cache_update_delay_ms_);

  // Set cache update time in preferences.
  prefs_->SetString(last_update_time_pref_name_,
                    base::NumberToString(base::Time::Now().ToDoubleT()));

  // If we are still fetching data, exit.
  if (in_fetch_)
    return;
  in_fetch_ = true;

  GURL web_resource_server =
      application_locale_.empty()
          ? web_resource_server_
          : google_util::AppendGoogleLocaleParam(web_resource_server_,
                                                 application_locale_);

  DVLOG(1) << "WebResourceService StartFetch " << web_resource_server;
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = web_resource_server;
  // Do not let url fetcher affect existing state in system context
  // (by setting cookies, for example).
  resource_request->load_flags = net::LOAD_DISABLE_CACHE;
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  simple_url_loader_ = network::SimpleURLLoader::Create(
      std::move(resource_request), traffic_annotation_);
  simple_url_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory_.get(),
      base::BindOnce(&WebResourceService::OnSimpleLoaderComplete,
                     base::Unretained(this)));
}

void WebResourceService::EndFetch() {
  in_fetch_ = false;
}

void WebResourceService::OnJsonParsed(
    data_decoder::DataDecoder::ValueOrError result) {
  if (!result.value) {
    LOG(ERROR) << *result.error;
    EndFetch();
    return;
  }

  const base::DictionaryValue* dict = nullptr;
  if (!result.value->GetAsDictionary(&dict)) {
    LOG(ERROR) << kUnexpectedJSONFormatError;
    EndFetch();
    return;
  }
  Unpack(*dict);

  EndFetch();
}

void WebResourceService::OnResourceRequestsAllowed() {
  int64_t delay = start_fetch_delay_ms_;
  // Check whether we have ever put a value in the web resource cache;
  // if so, pull it out and see if it's time to update again.
  if (prefs_->HasPrefPath(last_update_time_pref_name_)) {
    std::string last_update_pref =
        prefs_->GetString(last_update_time_pref_name_);
    if (!last_update_pref.empty()) {
      double last_update_value;
      base::StringToDouble(last_update_pref, &last_update_value);
      int64_t ms_until_update =
          cache_update_delay_ms_ -
          static_cast<int64_t>(
              (base::Time::Now() - base::Time::FromDoubleT(last_update_value))
                  .InMilliseconds());
      // Wait at least |start_fetch_delay_ms_|.
      if (ms_until_update > start_fetch_delay_ms_)
        delay = ms_until_update;
    }
  }
  // Start fetch and wait for UpdateResourceCache.
  ScheduleFetch(delay);
}

}  // namespace web_resource
