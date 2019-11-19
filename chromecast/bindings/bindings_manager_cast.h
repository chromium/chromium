// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BINDINGS_BINDINGS_MANAGER_CAST_H_
#define CHROMECAST_BINDINGS_BINDINGS_MANAGER_CAST_H_

#include <map>
#include <string>

#include "base/callback.h"
#include "chromecast/bindings/bindings_manager.h"
#include "chromecast/browser/cast_web_contents.h"
#include "mojo/public/cpp/bindings/connector.h"
#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/cpp/system/message_pipe.h"

namespace chromecast {
namespace bindings {

// Implements the CastOS BindingsManager.
class BindingsManagerCast : public BindingsManager,
                            public CastWebContents::Observer,
                            public mojo::MessageReceiver {
 public:
  BindingsManagerCast();
  ~BindingsManagerCast() override;

  // Add JS bindings to the page |cast_web_contents_|.
  // Start Observing the PageState changes.
  void AttachToPage(chromecast::CastWebContents* cast_web_contents);

  // The document and its statically-declared subresources are loaded.
  // BindingsManagerCast will inject all registered bindings at this time.
  // BindingsManagerCast will post a message that conveys an end of MessagePort
  // to the loaded page, so that the NamedMessagePort binding could utilize the
  // port to communicate with the native part.
  void OnPageLoaded();

  // BindingsManager implementation:
  void AddBinding(base::StringPiece binding_name,
                  base::StringPiece binding_script) override;

  // CastWebContents::Observer implementation:
  void OnPageStateChanged(CastWebContents* cast_web_contents) override;

 private:
  // |connector_| has been disconnected.
  void OnControlPortDisconnected();

  // mojo::MessageReceiver implementation:
  bool Accept(mojo::Message* message) override;

  // Stores all bindings, keyed on the string-based IDs.
  std::map<std::string, std::string> bindings_by_id_;
  chromecast::CastWebContents* cast_web_contents_;
  // Binded with the MessagePort used to receive messages from the page JS.
  std::unique_ptr<mojo::Connector> connector_;

  DISALLOW_COPY_AND_ASSIGN(BindingsManagerCast);
};

}  // namespace bindings
}  // namespace chromecast

#endif  // CHROMECAST_BINDINGS_BINDINGS_MANAGER_CAST_H_
