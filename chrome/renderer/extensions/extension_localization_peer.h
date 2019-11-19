// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_EXTENSIONS_EXTENSION_LOCALIZATION_PEER_H_
#define CHROME_RENDERER_EXTENSIONS_EXTENSION_LOCALIZATION_PEER_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/macros.h"
#include "content/public/renderer/request_peer.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "mojo/public/cpp/system/simple_watcher.h"
#include "services/network/public/cpp/url_loader_completion_status.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace IPC {
class Sender;
}

// The ExtensionLocalizationPeer is a proxy to a
// content::RequestPeer instance.  It is used to pre-process
// CSS files requested by extensions to replace localization templates with the
// appropriate localized strings.
//
// Call the factory method CreateExtensionLocalizationPeer() to obtain an
// instance of ExtensionLocalizationPeer based on the original Peer.
//
// The main flow of method calls is like this:
// 1.   OnReceivedResponse() when the response header is ready.
// 2-a. OnStartLoadingResponseBody() when the body streaming starts. It starts
//      to read the body from the data pipe. After finishing to read the whole
//      body, this class replaces the body using the message catalogs, sends the
//      response header, sends a data pipe to the original peer, and starts to
//      send the body over the data pipe.
// 2-b. OnCompletedRequest() when the final status is available. The status code
//      is stored as a member.
// 3.   CompleteRequest() when both of 2-a and 2-b finish. Sends the stored
//      status code to the original peer.
//
// Note that OnCompletedRequest() can be called at any time, even before
// OnReceivedResponse().
class ExtensionLocalizationPeer : public content::RequestPeer {
 public:
  ~ExtensionLocalizationPeer() override;

  static std::unique_ptr<content::RequestPeer> CreateExtensionLocalizationPeer(
      std::unique_ptr<content::RequestPeer> peer,
      IPC::Sender* message_sender,
      const std::string& mime_type,
      const GURL& request_url);

  // content::RequestPeer methods.
  void OnUploadProgress(uint64_t position, uint64_t size) override;
  bool OnReceivedRedirect(const net::RedirectInfo& redirect_info,
                          network::mojom::URLResponseHeadPtr head) override;
  void OnReceivedResponse(network::mojom::URLResponseHeadPtr head) override;
  void OnStartLoadingResponseBody(
      mojo::ScopedDataPipeConsumerHandle body) override;
  void OnTransferSizeUpdated(int transfer_size_diff) override;
  void OnCompletedRequest(
      const network::URLLoaderCompletionStatus& status) override;
  scoped_refptr<base::TaskRunner> GetTaskRunner() override;

 private:
  friend class ExtensionLocalizationPeerTest;

  // Use CreateExtensionLocalizationPeer to create an instance.
  ExtensionLocalizationPeer(std::unique_ptr<content::RequestPeer> peer,
                            IPC::Sender* message_sender,
                            const GURL& request_url);

  void OnReadableBody(MojoResult, const mojo::HandleSignalsState&);
  void StartSendingBody();
  void OnWritableBody(MojoResult, const mojo::HandleSignalsState&);

  // Loads message catalogs, and replaces all __MSG_some_name__ templates within
  // loaded file.
  void ReplaceMessages();

  void CompleteRequest();

  // Original peer that handles the request once we are done processing data_.
  std::unique_ptr<content::RequestPeer> original_peer_;

  // We just pass though the response info. This holds the copy of the original.
  network::mojom::URLResponseHeadPtr response_head_;

  struct DataPipeState {
    DataPipeState();
    ~DataPipeState();

    // Data pipe for reading the body which is passed on
    // OnStartLoadingResponseBody() and its watcher. When reading the body
    // reaches to the end, the handle will be reset.
    mojo::ScopedDataPipeConsumerHandle source_handle_;
    mojo::SimpleWatcher source_watcher_;

    // Data pipe for pushing the body to the |original_peer_| and its
    // watcher.
    mojo::ScopedDataPipeProducerHandle destination_handle_;
    mojo::SimpleWatcher destination_watcher_;

    // Size sent to the destination.
    size_t sent_in_bytes_ = 0;

    // Shows the state of streaming the body to the |original_peer_|.
    enum class BodyState {
      // Before getting |source_handle_|.
      kInitial,
      // Reading the body from |source_handle_|.
      kReadingBody,
      // Sending the body via |destination_handle_|.
      kSendingBody,
      // Sent all the body to |destination_handle_|.
      kDone
    };
    BodyState body_state_ = BodyState::kInitial;
  };

  DataPipeState data_pipe_state_;

  // Set when OnCompletedRequest() is called, and sent to the original peer on
  // CompleteRequest().
  base::Optional<network::URLLoaderCompletionStatus> completion_status_;

  // Sends ExtensionHostMsg_GetMessageBundle message to the browser to fetch
  // message catalog.
  IPC::Sender* message_sender_;

  // Buffer for incoming data. We wait until OnCompletedRequest before using it.
  std::string data_;

  // Original request URL.
  GURL request_url_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ExtensionLocalizationPeer);
};

#endif  // CHROME_RENDERER_EXTENSIONS_EXTENSION_LOCALIZATION_PEER_H_
