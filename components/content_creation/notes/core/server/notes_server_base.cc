// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/content_creation/notes/core/server/notes_server_base.h"

#import "components/signin/public/identity_manager/primary_account_access_token_fetcher.h"
#import "google_apis/google_api_keys.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"
#import "services/network/public/cpp/simple_url_loader.h"

NotesServerBase::NotesServerBase(
    scoped_refptr<network::SharedURLLoaderFactory> loader_factory,
    signin::IdentityManager* identity_manager)
    : identity_manager_(identity_manager) {
  url_loader_factory_ = std::move(loader_factory);
}

NotesServerBase::~NotesServerBase() {}

signin::ScopeSet NotesServerBase::GetAuthScopes() {
  NOTIMPLEMENTED();

  return {"put_real_one_here"};
}

GURL NotesServerBase::GetNotesServerURL() {
  NOTIMPLEMENTED();

  return GURL();
}

std::unique_ptr<network::ResourceRequest>
NotesServerBase::CreateNoteResourceRequest(GURL request_url,
                                           const std::string request_method) {
  NOTIMPLEMENTED();

  return std::make_unique<network::ResourceRequest>();
}

bool NotesServerBase::HasValidNonEmptyResponse(
    const std::string& response_body) {
  NOTIMPLEMENTED();

  return true;
}

void NotesServerBase::StartAccessTokenFetch() {
  NOTIMPLEMENTED();
}
