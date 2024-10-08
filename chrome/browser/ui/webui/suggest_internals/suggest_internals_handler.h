// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SUGGEST_INTERNALS_SUGGEST_INTERNALS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SUGGEST_INTERNALS_SUGGEST_INTERNALS_HANDLER_H_

#include <optional>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "chrome/browser/ui/webui/suggest_internals/suggest_internals.mojom.h"
#include "components/omnibox/browser/remote_suggestions_service.h"
#include "components/search_engines/template_url.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

class Profile;

namespace content {
class WebContents;
class WebUIDataSource;
}  // namespace content

// Handles communication between the browser and chrome://suggest-internals.
class SuggestInternalsHandler : public suggest_internals::mojom::PageHandler,
                                public RemoteSuggestionsService::Observer,
                                public RemoteSuggestionsService::Delegate {
 public:
  static void SetupWebUIDataSource(content::WebUIDataSource* source,
                                   Profile* profile);
  SuggestInternalsHandler(
      mojo::PendingReceiver<suggest_internals::mojom::PageHandler>
          pending_page_handler,
      Profile* profile,
      content::WebContents* web_contents);
  SuggestInternalsHandler(const SuggestInternalsHandler&) = delete;
  SuggestInternalsHandler& operator=(const SuggestInternalsHandler&) = delete;
  ~SuggestInternalsHandler() override;

  // suggest_internals::mojom::PageHandler:
  void SetPage(mojo::PendingRemote<suggest_internals::mojom::Page> pending_page)
      override;
  void HardcodeResponse(const std::string& response,
                        base::TimeDelta delay,
                        HardcodeResponseCallback callback) override;

  // RemoteSuggestionsService::Observer:
  void OnSuggestRequestCreated(
      const base::UnguessableToken& request_id,
      const network::ResourceRequest* request) override;
  void OnSuggestRequestStarted(const base::UnguessableToken& request_id,
                               network::SimpleURLLoader* loader,
                               const std::string& request_body) override;
  void OnSuggestRequestCompleted(
      const base::UnguessableToken& request_id,
      const int response_code,
      const std::unique_ptr<std::string>& response_body) override;

  // RemoteSuggestionsService::Delegate:
  void OnSuggestRequestCompleted(const network::SimpleURLLoader* source,
                                 const int response_code,
                                 std::unique_ptr<std::string> response_body,
                                 RemoteSuggestionsService::CompletionCallback
                                     completion_callback) override;

 private:
  raw_ptr<Profile> profile_;
  raw_ptr<content::WebContents> web_contents_;
  base::ScopedObservation<RemoteSuggestionsService,
                          RemoteSuggestionsService::Observer>
      remote_suggestions_service_observation_{this};

  // Used to override the response after a delay for all requests.
  std::optional<std::pair<std::string, base::TimeDelta>>
      hardcoded_response_and_delay_;

  mojo::Remote<suggest_internals::mojom::Page> page_;
  mojo::Receiver<suggest_internals::mojom::PageHandler> page_handler_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_SUGGEST_INTERNALS_SUGGEST_INTERNALS_HANDLER_H_
