// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_SERVICE_IMPL_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_SERVICE_IMPL_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "components/autofill_assistant/browser/access_token_fetcher.h"
#include "components/autofill_assistant/browser/client_context.h"
#include "components/autofill_assistant/browser/device_context.h"
#include "components/autofill_assistant/browser/service.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "components/signin/public/identity_manager/access_token_fetcher.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "url/gurl.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace autofill_assistant {
class Client;

// Native autofill assistant service which communicates with the server to get
// scripts and client actions.
// TODO(b/158998456): Add unit tests.
class ServiceImpl : public Service {
 public:
  // Convenience method for creating a service. |context| and |client| must
  // remain valid for the lifetime of the service instance. Will enable
  // authentication unless disabled via the autofill-assistant-auth command line
  // flag.
  static std::unique_ptr<ServiceImpl> Create(content::BrowserContext* context,
                                             Client* client);

  bool IsLiteService() const override;

  // |context| and |access_token_fetcher| must remain valid for the lifetime of
  // the service instance.
  ServiceImpl(content::BrowserContext* context,
              version_info::Channel channel,
              std::unique_ptr<ClientContext> client_context,
              AccessTokenFetcher* access_token_fetcher,
              bool auth_enabled);
  ServiceImpl(const std::string& api_key,
              const GURL& server_url,
              content::BrowserContext* context,
              std::unique_ptr<ClientContext> client_context,
              AccessTokenFetcher* access_token_fetcher,
              bool auth_enabled);
  ~ServiceImpl() override;

  // Get scripts for a given |url|, which should be a valid URL.
  void GetScriptsForUrl(const GURL& url,
                        const TriggerContext& trigger_context,
                        ResponseCallback callback) override;

  // Get actions.
  void GetActions(const std::string& script_path,
                  const GURL& url,
                  const TriggerContext& trigger_context,
                  const std::string& global_payload,
                  const std::string& script_payload,
                  ResponseCallback callback) override;

  // Get next sequence of actions according to server payloads in previous
  // response.
  void GetNextActions(
      const TriggerContext& trigger_context,
      const std::string& previous_global_payload,
      const std::string& previous_script_payload,
      const std::vector<ProcessedActionProto>& processed_actions,
      ResponseCallback callback) override;

 private:
  friend class ServiceImplTest;

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

  // The client context to send to the backend.
  std::unique_ptr<ClientContext> client_context_;

  // Pointer must remain valid for the lifetime of the Service instance.
  AccessTokenFetcher* access_token_fetcher_;

  // True while waiting for a response from AccessTokenFetcher.
  bool fetching_token_;

  // Whether requests should be authenticated.
  bool auth_enabled_;

  // An OAuth 2 token. Empty if not fetched yet or if the token has been
  // invalidated.
  std::string access_token_;

  base::WeakPtrFactory<ServiceImpl> weak_ptr_factory_;

  FRIEND_TEST_ALL_PREFIXES(ServiceImplTestSignedInStatus, SetsSignedInStatus);

  DISALLOW_COPY_AND_ASSIGN(ServiceImpl);
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_SERVICE_IMPL_H_
