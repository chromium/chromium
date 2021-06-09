// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_creation/notes/core/server/notes_server_base.h"

#include "components/signin/public/identity_manager/primary_account_access_token_fetcher.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/google_api_keys.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"

namespace content_creation {

NotesServerBase::NotesServerBase(
    scoped_refptr<network::SharedURLLoaderFactory> loader_factory,
    signin::IdentityManager* identity_manager)
    : identity_manager_(identity_manager) {
  url_loader_factory_ = std::move(loader_factory);
}

NotesServerBase::~NotesServerBase() {}

signin::ScopeSet NotesServerBase::GetAuthScopes() {
  return {GaiaConstants::kOAuth1LoginScope};
}

GURL NotesServerBase::GetNotesServerURL() {
  GURL base_url("staging-gsaprototype-pa.sandbox.googleapis.com");
  GURL::Replacements replacements;
  replacements.SetPathStr("/v1/webnotes");
  return base_url.ReplaceComponents(replacements);
}

std::unique_ptr<network::ResourceRequest>
NotesServerBase::CreateNoteResourceRequest(GURL request_url,
                                           const std::string request_method) {
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = request_url;
  resource_request->method = request_method;
  std::string api_key = google_apis::GetAPIKey();
  DCHECK(!api_key.empty());
  resource_request->headers.SetHeader("x-goog-api-key", api_key);
  resource_request->headers.SetHeader(net::HttpRequestHeaders::kContentType,
                                      "application/x-protobuf");
  return resource_request;
}

bool NotesServerBase::HasValidNonEmptyResponse(
    const std::string& response_body) {
  NOTIMPLEMENTED();

  return true;
}

void NotesServerBase::StartAccessTokenFetch() {
  // It's safe to pass base::Unretained(this) since deleting the token fetcher
  // will prevent the callback from being completed.
  token_fetcher_ = std::make_unique<signin::PrimaryAccountAccessTokenFetcher>(
      "note", identity_manager_, GetAuthScopes(),
      base::BindOnce(&NotesServerBase::AccessTokenFetchFinished,
                     base::Unretained(this), base::TimeTicks::Now()),
      signin::PrimaryAccountAccessTokenFetcher::Mode::kWaitUntilAvailable);
}

}  // namespace content_creation
