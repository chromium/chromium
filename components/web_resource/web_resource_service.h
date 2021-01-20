// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEB_RESOURCE_WEB_RESOURCE_SERVICE_H_
#define COMPONENTS_WEB_RESOURCE_WEB_RESOURCE_SERVICE_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "components/web_resource/resource_request_allowed_notifier.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "url/gurl.h"

class PrefService;

namespace base {
class DictionaryValue;
}

namespace network {
class SimpleURLLoader;
class SharedURLLoaderFactory;
}

namespace web_resource {

// A WebResourceService fetches JSON data from a web server and periodically
// refreshes it.
class WebResourceService : public ResourceRequestAllowedNotifier::Observer {
 public:
  // Creates a new WebResourceService.
  // If |application_locale| is not empty, it will be appended as a locale
  // parameter to the resource URL.
  WebResourceService(
      PrefService* prefs,
      const GURL& web_resource_server,
      const std::string& application_locale,  // May be empty
      const char* last_update_time_pref_name,
      int start_fetch_delay_ms,
      int cache_update_delay_ms,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const char* disable_network_switch,
      const net::NetworkTrafficAnnotationTag& traffic_annotation,
      ResourceRequestAllowedNotifier::NetworkConnectionTrackerGetter
          network_connection_tracker_getter);

  ~WebResourceService() override;

  // Sleep until cache needs to be updated, but always for at least
  // |start_fetch_delay_ms| so we don't interfere with startup.
  // Then begin updating resources.
  void StartAfterDelay();

  // Sets the ResourceRequestAllowedNotifier to make it configurable.
  void SetResourceRequestAllowedNotifier(
      std::unique_ptr<ResourceRequestAllowedNotifier> notifier);

 protected:
  PrefService* prefs_;
  bool GetFetchScheduled() const;

 private:
  friend class WebResourceServiceTest;

  // For the subclasses to process the result of a fetch.
  virtual void Unpack(const base::DictionaryValue& parsed_json) = 0;

  // Callback from SimpleURLLoader.
  void OnSimpleLoaderComplete(std::unique_ptr<std::string> response_body);

  // Schedules a fetch after |delay_ms| milliseconds.
  void ScheduleFetch(int64_t delay_ms);

  // Starts fetching data from the server.
  void StartFetch();

  // Set |in_fetch_| to false, clean up temp directories (in the future).
  void EndFetch();

  // Callback from the JSON parser.
  void OnJsonParsed(data_decoder::DataDecoder::ValueOrError result);

  // Implements ResourceRequestAllowedNotifier::Observer.
  void OnResourceRequestsAllowed() override;

  // Helper class used to tell this service if it's allowed to make network
  // resource requests.
  std::unique_ptr<ResourceRequestAllowedNotifier>
      resource_request_allowed_notifier_;

  // True if we have scheduled a fetch after start_fetch_delay_ms_
  // or another one in |cache_update_delay_ms_| time. Set to false
  // before fetching starts so that next fetch is scheduled. This
  // is to make sure not more than one fetch is scheduled for given
  // point in time.
  bool fetch_scheduled_;

  // The tool that loads the url data from the server.
  std::unique_ptr<network::SimpleURLLoader> simple_url_loader_;

  // True if we are currently fetching or unpacking data. If we are asked to
  // start a fetch when we are still fetching resource data, schedule another
  // one in |cache_update_delay_ms_| time, and silently exit.
  bool in_fetch_;

  // URL that hosts the web resource.
  GURL web_resource_server_;

  // Application locale, appended to the URL if not empty.
  std::string application_locale_;

  // Pref name to store the last update's time.
  const char* last_update_time_pref_name_;

  // Delay on first fetch so we don't interfere with startup.
  int start_fetch_delay_ms_;

  // Delay between calls to update the web resource cache. This delay may be
  // different for different builds of Chrome.
  int cache_update_delay_ms_;

  // The URL loader factory for the resource load.
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  // Network traffic annotation for initialization of URLFetcher.
  const net::NetworkTrafficAnnotationTag traffic_annotation_;

  // So that we can delay our start so as not to affect start-up time; also,
  // so that we can schedule future cache updates.
  base::WeakPtrFactory<WebResourceService> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(WebResourceService);
};

}  // namespace web_resource

#endif  // COMPONENTS_WEB_RESOURCE_WEB_RESOURCE_SERVICE_H_
