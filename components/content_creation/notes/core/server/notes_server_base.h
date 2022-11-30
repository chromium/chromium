// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTENT_CREATION_NOTES_CORE_SERVER_NOTES_SERVER_BASE_H_
#define COMPONENTS_CONTENT_CREATION_NOTES_CORE_SERVER_NOTES_SERVER_BASE_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/scope_set.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "services/network/public/cpp/resource_request.h"

namespace network {
class SimpleURLLoader;
class SharedURLLoaderFactory;
}  // namespace network

namespace signin {
class IdentityManager;
class PrimaryAccountAccessTokenFetcher;
}  // namespace signin

namespace content_creation {

// Base class for interactions with the Notes Server.
class NotesServerBase {
 public:
  explicit NotesServerBase(
      scoped_refptr<network::SharedURLLoaderFactory> loader_factory,
      signin::IdentityManager* identity_manager);
  virtual ~NotesServerBase();

  virtual void Start() = 0;

  // Not copyable or movable.
  NotesServerBase(const NotesServerBase&) = delete;
  NotesServerBase& operator=(const NotesServerBase&) = delete;

 protected:
  // Returns the ScopeSet for which the user needs to authenticate to access the
  // Notes Server.
  signin::ScopeSet GetAuthScopes();

  // Returns the URL of the Notes Server
  GURL GetNotesServerURL();

  // Creates the basic ResourceRequest for Note queries.
  std::unique_ptr<network::ResourceRequest> CreateNoteResourceRequest(
      GURL request_url,
      const std::string request_method);

  // Returns whether the response returned from the server is valid and
  // non-empty.
  bool HasValidNonEmptyResponse(const std::string& response_body);

  // Starts the request to get the user's access token.
  void StartAccessTokenFetch();

  // Called when the user's access token has been fetched.
  virtual void AccessTokenFetchFinished(
      base::TimeTicks token_start_ticks,
      GoogleServiceAuthError error,
      signin::AccessTokenInfo access_token_info) = 0;

  // The access token for the user. Used to authenticate to the server.
  std::string access_token_;

  const raw_ptr<signin::IdentityManager> identity_manager_;
  std::unique_ptr<signin::PrimaryAccountAccessTokenFetcher> token_fetcher_;

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  std::unique_ptr<network::SimpleURLLoader> url_loader_;
};

}  // namespace content_creation

#endif  // COMPONENTS_CONTENT_CREATION_NOTES_CORE_SERVER_NOTES_SERVER_BASE_H_
