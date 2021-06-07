// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/bindings/bindings_manager_cast.h"

#include <utility>
#include <vector>

#include "base/logging.h"
#include "base/macros.h"
#include "base/notreached.h"
#include "chromecast/bindings/named_message_port_connector_cast.h"
#include "components/on_load_script_injector/browser/on_load_script_injector_host.h"

namespace chromecast {
namespace bindings {

BindingsManagerCast::BindingsManagerCast(
    chromecast::CastWebContents* cast_web_contents)
    : cast_web_contents_(cast_web_contents) {
  DCHECK(cast_web_contents_);

  CastWebContents::Observer::Observe(cast_web_contents_);

  port_connector_ =
      std::make_unique<NamedMessagePortConnectorCast>(cast_web_contents_, this);

  port_connector_->RegisterPortHandler(base::BindRepeating(
      &BindingsManagerCast::OnPortConnected, base::Unretained(this)));
}

BindingsManagerCast::~BindingsManagerCast() = default;

mojo::PendingRemote<mojom::ApiBindings> BindingsManagerCast::CreateRemote() {
  DCHECK(!receiver_.is_bound());

  mojo::PendingRemote<mojom::ApiBindings> pending_remote =
      receiver_.BindNewPipeAndPassRemote();
  receiver_.set_disconnect_handler(base::BindOnce(
      &BindingsManagerCast::OnClientDisconnected, base::Unretained(this)));

  return pending_remote;
}

void BindingsManagerCast::AddBinding(base::StringPiece binding_name,
                                     base::StringPiece binding_script) {
  bindings_[std::string(binding_name)] = std::string(binding_script);
}

void BindingsManagerCast::OnClientDisconnected() {
  receiver_.reset();
}

void BindingsManagerCast::OnPageStateChanged(
    CastWebContents* cast_web_contents) {
  // TODO(b/183378843): Remove usage of CWC in this class and
  // move the CWC::Observer logics to other components, e.g.
  // NMPC or |ApiBindingsClient|.
  auto page_state = cast_web_contents->page_state();

  switch (page_state) {
    case CastWebContents::PageState::DESTROYED:
    case CastWebContents::PageState::ERROR:
      CastWebContents::Observer::Observe(nullptr);
      cast_web_contents_ = nullptr;
      port_connector_.reset();
      break;
    case CastWebContents::PageState::LOADED:
      port_connector_->OnPageLoaded();
      break;
    case CastWebContents::PageState::IDLE:
    case CastWebContents::PageState::LOADING:
    case CastWebContents::PageState::CLOSED:
      break;
  }
}

void BindingsManagerCast::GetAll(GetAllCallback callback) {
  std::vector<chromecast::mojom::ApiBindingPtr> bindings_vector;
  for (const auto& bindings_name_and_script : bindings_) {
    bindings_vector.emplace_back(
        chromecast::mojom::ApiBinding::New(bindings_name_and_script.second));
  }
  std::move(callback).Run(std::move(bindings_vector));
}

void BindingsManagerCast::Connect(const std::string& port_name,
                                  blink::MessagePortDescriptor port) {
  // TODO(b/183378843): Implements this method and migrate NMPC to use it.
  NOTIMPLEMENTED_LOG_ONCE();
}

}  // namespace bindings
}  // namespace chromecast
