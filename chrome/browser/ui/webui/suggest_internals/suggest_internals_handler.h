// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SUGGEST_INTERNALS_SUGGEST_INTERNALS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SUGGEST_INTERNALS_SUGGEST_INTERNALS_HANDLER_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
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
                                public RemoteSuggestionsService::Observer {
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
                        HardcodeResponseCallback callback) override;

  // RemoteSuggestionsService::Observer:
  void OnSuggestRequestStarting(
      const base::UnguessableToken& request_id,
      const network::ResourceRequest* request) override;
  void OnSuggestRequestCompleted(
      const base::UnguessableToken& request_id,
      const bool response_received,
      const std::unique_ptr<std::string>& response_body) override;

 private:
  raw_ptr<Profile> profile_;
  raw_ptr<content::WebContents> web_contents_;
  base::ScopedObservation<RemoteSuggestionsService,
                          RemoteSuggestionsService::Observer>
      remote_suggestions_service_observation_{this};

  // Used to override the response for all requests, if non-empty.
  std::string hardcoded_response_;

  mojo::Remote<suggest_internals::mojom::Page> page_;
  mojo::Receiver<suggest_internals::mojom::PageHandler> page_handler_;

  base::WeakPtrFactory<SuggestInternalsHandler> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_SUGGEST_INTERNALS_SUGGEST_INTERNALS_HANDLER_H_
