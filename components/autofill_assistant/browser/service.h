// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_SERVICE_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_SERVICE_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "components/autofill_assistant/browser/access_token_fetcher.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "services/identity/public/cpp/primary_account_access_token_fetcher.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "url/gurl.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace autofill_assistant {
// Autofill assistant service to communicate with the server to get scripts and
// client actions.
class Service {
 public:
  // |context| and |token_fetcher| must remain valid for the lifetime of the
  // service instance.
  Service(const std::string& api_key,
          const GURL& server_url,
          content::BrowserContext* context,
          AccessTokenFetcher* token_fetcher);
  virtual ~Service();

  using ResponseCallback =
      base::OnceCallback<void(bool result, const std::string&)>;
  // Get scripts for a given |url|, which should be a valid URL.
  virtual void GetScriptsForUrl(
      const GURL& url,
      const std::map<std::string, std::string>& parameters,
      ResponseCallback callback);

  // Get actions.
  virtual void GetActions(const std::string& script_path,
                          const GURL& url,
                          const std::map<std::string, std::string>& parameters,
                          const std::string& server_payload,
                          ResponseCallback callback);

  // Get next sequence of actions according to server payload in previous
  // response.
  virtual void GetNextActions(
      const std::string& previous_server_payload,
      const std::vector<ProcessedActionProto>& processed_actions,
      ResponseCallback callback);

 private:
  // Struct to store scripts and actions request.
  struct Loader {
    Loader();
    ~Loader();

    GURL url;
    std::string request_body;
    ResponseCallback callback;
    std::unique_ptr<::network::SimpleURLLoader> loader;
    bool retried_with_fresh_access_token;
  };

  void SendRequest(Loader* loader);

  // Creates a loader and adds it to |loaders_|.
  Loader* AddLoader(const GURL& url,
                    const std::string& request_body,
                    ResponseCallback callback);

  // Sends a request with the given loader, using the current auth token, if one
  // is available.
  void StartLoader(Loader* loader);
  void OnURLLoaderComplete(Loader* loader,
                           std::unique_ptr<std::string> response_body);

  // Fetches the access token and, once this is done, starts all pending loaders
  // in |loaders_|.
  void FetchAccessToken();
  void OnFetchAccessToken(bool success, const std::string& access_token);

  content::BrowserContext* context_;
  GURL script_server_url_;
  GURL script_action_server_url_;

  // Destroying this object will cancel ongoing requests.
  std::map<Loader*, std::unique_ptr<Loader>> loaders_;

  // API key to add to the URL of unauthenticated requests.
  std::string api_key_;

  // Pointer must remain valid for the lifetime of the Service instance.
  AccessTokenFetcher* access_token_fetcher_;

  // True while waiting for a response from AccessTokenFetcher.
  bool fetching_token_;

  // Whether requests should be authenticated.
  bool auth_enabled_;

  // An OAuth 2 token. Empty if not fetched yet or if the token has been
  // invalidated.
  std::string access_token_;

  base::WeakPtrFactory<Service> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(Service);
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_SERVICE_H_
