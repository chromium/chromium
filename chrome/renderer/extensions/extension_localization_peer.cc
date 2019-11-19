// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/extension_localization_peer.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_util.h"
#include "chrome/common/url_constants.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_messages.h"
#include "extensions/common/message_bundle.h"
#include "ipc/ipc_sender.h"
#include "net/base/net_errors.h"
#include "net/http/http_response_headers.h"

ExtensionLocalizationPeer::DataPipeState::DataPipeState()
    : source_watcher_(FROM_HERE, mojo::SimpleWatcher::ArmingPolicy::MANUAL),
      destination_watcher_(FROM_HERE,
                           mojo::SimpleWatcher::ArmingPolicy::MANUAL) {}

ExtensionLocalizationPeer::DataPipeState::~DataPipeState() = default;

ExtensionLocalizationPeer::ExtensionLocalizationPeer(
    std::unique_ptr<content::RequestPeer> peer,
    IPC::Sender* message_sender,
    const GURL& request_url)
    : original_peer_(std::move(peer)),
      message_sender_(message_sender),
      request_url_(request_url) {}

ExtensionLocalizationPeer::~ExtensionLocalizationPeer() {
}

// static
std::unique_ptr<content::RequestPeer>
ExtensionLocalizationPeer::CreateExtensionLocalizationPeer(
    std::unique_ptr<content::RequestPeer> peer,
    IPC::Sender* message_sender,
    const std::string& mime_type,
    const GURL& request_url) {
  // Return the given |peer| as is if content is not text/css or it doesn't
  // belong to extension scheme.
  return (request_url.SchemeIs(extensions::kExtensionScheme) &&
          base::StartsWith(mime_type, "text/css",
                           base::CompareCase::INSENSITIVE_ASCII))
             ? base::WrapUnique(new ExtensionLocalizationPeer(
                   std::move(peer), message_sender, request_url))
             : std::move(peer);
}

void ExtensionLocalizationPeer::OnUploadProgress(uint64_t position,
                                                 uint64_t size) {
  NOTREACHED();
}

bool ExtensionLocalizationPeer::OnReceivedRedirect(
    const net::RedirectInfo& redirect_info,
    network::mojom::URLResponseHeadPtr head) {
  NOTREACHED();
  return false;
}

void ExtensionLocalizationPeer::OnReceivedResponse(
    network::mojom::URLResponseHeadPtr head) {
  response_head_ = std::move(head);
}

void ExtensionLocalizationPeer::OnStartLoadingResponseBody(
    mojo::ScopedDataPipeConsumerHandle body) {
  data_pipe_state_.body_state_ = DataPipeState::BodyState::kReadingBody;
  data_pipe_state_.source_handle_ = std::move(body);
  data_pipe_state_.source_watcher_.Watch(
      data_pipe_state_.source_handle_.get(),
      MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED,
      MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED,
      base::BindRepeating(&ExtensionLocalizationPeer::OnReadableBody,
                          base::Unretained(this)));
  data_pipe_state_.source_watcher_.ArmOrNotify();
}

void ExtensionLocalizationPeer::OnTransferSizeUpdated(int transfer_size_diff) {
  original_peer_->OnTransferSizeUpdated(transfer_size_diff);
}

void ExtensionLocalizationPeer::OnCompletedRequest(
    const network::URLLoaderCompletionStatus& status) {
  if (completion_status_.has_value()) {
    // This means that we've already returned error status to the original peer
    // due to an error on the data pipe.
    return;
  }
  completion_status_ = status;

  if (status.error_code != net::OK) {
    data_pipe_state_.source_watcher_.Cancel();
    data_pipe_state_.source_handle_.reset();
    data_pipe_state_.destination_watcher_.Cancel();
    data_pipe_state_.destination_handle_.reset();
    data_pipe_state_.body_state_ = DataPipeState::BodyState::kDone;
  }

  if (data_pipe_state_.body_state_ != DataPipeState::BodyState::kDone) {
    // Still reading, or sending the body. Wait until all data has been read,
    // and sent to the |original_peer_|.
    return;
  }

  // We've sent all the body to the peer. Complete the request.
  CompleteRequest();
}

scoped_refptr<base::TaskRunner> ExtensionLocalizationPeer::GetTaskRunner() {
  return original_peer_->GetTaskRunner();
}

void ExtensionLocalizationPeer::OnReadableBody(
    MojoResult,
    const mojo::HandleSignalsState&) {
  DCHECK(data_pipe_state_.source_handle_.is_valid());
  DCHECK_EQ(DataPipeState::BodyState::kReadingBody,
            data_pipe_state_.body_state_);

  const void* buffer;
  uint32_t read_bytes = 0;
  MojoResult result = data_pipe_state_.source_handle_->BeginReadData(
      &buffer, &read_bytes, MOJO_READ_DATA_FLAG_NONE);

  if (result == MOJO_RESULT_SHOULD_WAIT) {
    data_pipe_state_.source_watcher_.ArmOrNotify();
    return;
  }

  if (result == MOJO_RESULT_FAILED_PRECONDITION) {
    data_pipe_state_.source_watcher_.Cancel();
    data_pipe_state_.source_handle_.reset();
    StartSendingBody();
    return;
  }

  if (result != MOJO_RESULT_OK) {
    // Something went wrong.
    data_pipe_state_.source_watcher_.Cancel();
    data_pipe_state_.source_handle_.reset();
    completion_status_ = network::URLLoaderCompletionStatus(net::ERR_FAILED);
    data_pipe_state_.body_state_ = DataPipeState::BodyState::kDone;
    CompleteRequest();
    return;
  }

  data_.append(static_cast<const char*>(buffer), read_bytes);

  result = data_pipe_state_.source_handle_->EndReadData(read_bytes);
  DCHECK_EQ(MOJO_RESULT_OK, result);
  data_pipe_state_.source_watcher_.ArmOrNotify();
}

void ExtensionLocalizationPeer::StartSendingBody() {
  DCHECK(!data_pipe_state_.source_handle_.is_valid());
  DCHECK_EQ(DataPipeState::BodyState::kReadingBody,
            data_pipe_state_.body_state_);

  data_pipe_state_.body_state_ = DataPipeState::BodyState::kSendingBody;

  ReplaceMessages();

  mojo::ScopedDataPipeConsumerHandle consumer_to_send;
  MojoResult result = mojo::CreateDataPipe(
      nullptr, &data_pipe_state_.destination_handle_, &consumer_to_send);
  if (result != MOJO_RESULT_OK) {
    completion_status_ =
        network::URLLoaderCompletionStatus(net::ERR_INSUFFICIENT_RESOURCES);
    data_pipe_state_.body_state_ = DataPipeState::BodyState::kDone;
    CompleteRequest();
    return;
  }

  original_peer_->OnReceivedResponse(std::move(response_head_));
  original_peer_->OnStartLoadingResponseBody(std::move(consumer_to_send));

  data_pipe_state_.destination_watcher_.Watch(
      data_pipe_state_.destination_handle_.get(),
      MOJO_HANDLE_SIGNAL_WRITABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED,
      MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED,
      base::BindRepeating(&ExtensionLocalizationPeer::OnWritableBody,
                          base::Unretained(this)));
  data_pipe_state_.destination_watcher_.ArmOrNotify();
}

void ExtensionLocalizationPeer::OnWritableBody(
    MojoResult,
    const mojo::HandleSignalsState&) {
  DCHECK(data_pipe_state_.destination_handle_.is_valid());
  DCHECK_EQ(DataPipeState::BodyState::kSendingBody,
            data_pipe_state_.body_state_);

  uint32_t available = data_.size() - data_pipe_state_.sent_in_bytes_;
  MojoResult result = data_pipe_state_.destination_handle_->WriteData(
      data_.data() + data_pipe_state_.sent_in_bytes_, &available,
      MOJO_WRITE_DATA_FLAG_NONE);
  if (result == MOJO_RESULT_SHOULD_WAIT) {
    data_pipe_state_.destination_watcher_.ArmOrNotify();
    return;
  }

  if (result != MOJO_RESULT_OK) {
    // The pipe is closed on the receiver side.
    data_ = std::string();
    data_pipe_state_.destination_watcher_.Cancel();
    data_pipe_state_.destination_handle_.reset();
    data_pipe_state_.body_state_ = DataPipeState::BodyState::kDone;
    completion_status_ = network::URLLoaderCompletionStatus(net::ERR_FAILED);
    CompleteRequest();
    return;
  }

  data_pipe_state_.sent_in_bytes_ += available;

  if (data_pipe_state_.sent_in_bytes_ == data_.size()) {
    // We sent all of the data.
    data_ = std::string();
    data_pipe_state_.destination_watcher_.Cancel();
    data_pipe_state_.destination_handle_.reset();
    data_pipe_state_.body_state_ = DataPipeState::BodyState::kDone;
    if (completion_status_.has_value())
      CompleteRequest();
    return;
  }

  // Wait until the pipe is ready to send the next chunk of data.
  data_pipe_state_.destination_watcher_.ArmOrNotify();
}

void ExtensionLocalizationPeer::ReplaceMessages() {
  if (!message_sender_ || data_.empty())
    return;

  if (!request_url_.is_valid())
    return;

  std::string extension_id = request_url_.host();
  extensions::L10nMessagesMap* l10n_messages =
      extensions::GetL10nMessagesMap(extension_id);
  if (!l10n_messages) {
    extensions::L10nMessagesMap messages;
    message_sender_->Send(new ExtensionHostMsg_GetMessageBundle(
        extension_id, &messages));

    // Save messages we got, so we don't have to ask again.
    // Messages map is never empty, it contains at least @@extension_id value.
    extensions::ExtensionToL10nMessagesMap& l10n_messages_map =
        *extensions::GetExtensionToL10nMessagesMap();
    l10n_messages_map[extension_id] = messages;

    l10n_messages = extensions::GetL10nMessagesMap(extension_id);
  }

  std::string error;
  if (extensions::MessageBundle::ReplaceMessagesWithExternalDictionary(
          *l10n_messages, &data_, &error)) {
    data_.resize(data_.size());
  }
}

void ExtensionLocalizationPeer::CompleteRequest() {
  DCHECK(completion_status_.has_value());
  // Body should have been sent to the origial peer at this point when it's
  // from a data pipe.
  DCHECK_EQ(DataPipeState::BodyState::kDone, data_pipe_state_.body_state_);

  if (completion_status_->error_code != net::OK) {
    // We failed to load the resource.
    network::URLLoaderCompletionStatus aborted_status =
        completion_status_.value();
    aborted_status.error_code = net::ERR_ABORTED;
    original_peer_->OnCompletedRequest(aborted_status);
    return;
  }

  original_peer_->OnCompletedRequest(completion_status_.value());
}
