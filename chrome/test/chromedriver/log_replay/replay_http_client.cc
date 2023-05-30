// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/test/chromedriver/log_replay/replay_http_client.h"

#include <utility>

#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "url/gurl.h"

namespace {

// Fetch the path from the given url (i.e. "http://foo.bar/baz" -> "/baz")
std::string UrlPath(const std::string& url) {
  GURL gurl(url);
  return gurl.path();
}

}  // namespace

ReplayHttpClient::ReplayHttpClient(const DevToolsEndpoint& endpoint,
                                   network::mojom::URLLoaderFactory* factory,
                                   const base::FilePath& log_path)
    : DevToolsHttpClient(endpoint, factory), log_reader_(log_path) {}
ReplayHttpClient::~ReplayHttpClient() = default;

bool ReplayHttpClient::FetchUrlAndLog(const std::string& url,
                                      std::string* response) {
  VLOG(1) << "DevTools HTTP Request: " << url;
  std::string path_from_url = UrlPath(url);
  std::unique_ptr<LogEntry> next_command = log_reader_.GetNext(LogEntry::kHTTP);
  // The HTTP requests should happen in the same order as they occur in the
  // log file. We use this as a sanity check and return false if something
  // appears to be out of order or if the log file appears truncated.
  if (next_command == nullptr) {
    return false;
  }
  if (UrlPath(next_command->command_name) == path_from_url &&
      next_command->event_type == LogEntry::kRequest) {
    std::unique_ptr<LogEntry> next_response =
        log_reader_.GetNext(LogEntry::kHTTP);
    if (next_response == nullptr) {
      return false;
    }
    *response = next_response->payload;
    VLOG(1) << "DevTools HTTP Response: " << next_response->payload;
    return true;
  }
  return false;
}
