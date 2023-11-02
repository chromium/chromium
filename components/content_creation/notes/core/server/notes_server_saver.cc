// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_creation/notes/core/server/notes_server_saver.h"

#include "base/strings/strcat.h"
#include "components/content_creation/notes/core/note_features.h"
#include "components/content_creation/notes/core/server/note.pb.h"
#include "components/content_creation/notes/core/server/note_data.h"
#include "components/content_creation/notes/core/server/save_note_response.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"

namespace content_creation {

namespace {
size_t kMaxSaveResponseSize = 2048;

net::NetworkTrafficAnnotationTag GetSaveNoteRequestTrafficAnnotation() {
  return net::DefineNetworkTrafficAnnotation("publish_note_request", R"(
        semantics {
          sender: "Note Creation Component"
          description:
            "Chrome provides the ability to create a note about web content "
            "and offers the user the possibility of saving it to Googler "
            "servers and making it available to the web. "
          trigger: "User presses on the option to publish a note after they "
            "are done creating one."
          data: "The data necessary to render the created note. For example, a "
            "comment, a quote, the link to the original page where the quote "
            "was taken, etc."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting:
            "There are not settings to disable the feature at the moment. "
            "However, this is a feature that is only activated by a user "
            "action, it is therefore only run on demand."
          policy_exception_justification: "No policy affecting this feature "
            "at the moment."
        })");
}

}  // namespace

NotesServerSaver::NotesServerSaver(
    scoped_refptr<network::SharedURLLoaderFactory> loader_factory,
    signin::IdentityManager* identity_manager,
    NoteData note_data,
    base::OnceCallback<void(SaveNoteResponse)> callback)
    : NotesServerBase{loader_factory, identity_manager},
      note_data_(note_data),
      save_callback_(std::move(callback)) {}

NotesServerSaver::~NotesServerSaver() {}

void NotesServerSaver::Start() {
  if (!IsPublishEnabled()) {
    return;
  }

  // Start fetching the access token. This will trigger the method to save the
  // note when done.
  NotesServerBase::StartAccessTokenFetch();
}

void NotesServerSaver::SendSaveNoteRequest() {
  DCHECK(IsPublishEnabled());

  // Prepare the Note payload.
  web_notes::PutWebnoteRequest save_request;
  web_notes::Webnote* note_proto = save_request.mutable_webnote();
  note_proto->set_quote(note_data_.quote);
  note_proto->set_note(note_data_.comment);
  note_proto->set_web_page_url(note_data_.webpage_url.spec());
  note_proto->set_highlight_directive(note_data_.highlight_directive);
  std::string request_body = save_request.SerializeAsString();

  auto resource_request = NotesServerBase::CreateNoteResourceRequest(
      NotesServerBase::GetNotesServerURL(),
      net::HttpRequestHeaders::kPostMethod);

  resource_request->headers.SetHeader(net::HttpRequestHeaders::kAuthorization,
                                      base::StrCat({"Bearer ", access_token_}));

  url_loader_ = network::SimpleURLLoader::Create(
      std::move(resource_request), GetSaveNoteRequestTrafficAnnotation());
  url_loader_->SetAllowHttpErrorResults(true);
  url_loader_->AttachStringForUpload(request_body, "application/x-protobuf");

  url_loader_->DownloadToString(
      url_loader_factory_.get(),
      base::BindOnce(&NotesServerSaver::OnSaveNoteComplete,
                     weak_factory_.GetWeakPtr()),
      kMaxSaveResponseSize);
}

void NotesServerSaver::OnSaveNoteComplete(
    std::unique_ptr<std::string> response_body) {
  SaveNoteResponse response;
  if (response_body == nullptr) {
    DVLOG(1) << "Has no response";
  } else if (!HasValidNonEmptyResponse(*response_body.get())) {
    DVLOG(1) << "Has empty invalid response";
  } else {
    // Parse the response.
    web_notes::PutWebnoteResponse save_note_response;
    if (!save_note_response.ParseFromString(*response_body.get())) {
      DVLOG(1) << "Failed to parse note";
    } else if (!save_note_response.has_webnote_content_id()) {
      DVLOG(1) << "Response did not contain content ID information";
    } else {
      web_notes::WebnoteContentId note_content_id =
          save_note_response.webnote_content_id();
      response.account_id = note_content_id.account_id();
      response.note_id = note_content_id.webnote_id();
    }
  }

  url_loader_.reset();

  std::move(save_callback_).Run(response);
}

void NotesServerSaver::AccessTokenFetchFinished(
    base::TimeTicks token_start_ticks,
    GoogleServiceAuthError error,
    signin::AccessTokenInfo access_token_info) {
  access_token_ = access_token_info.token;
  SendSaveNoteRequest();
}

}  // namespace content_creation
