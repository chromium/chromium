// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/bindings/bindings_manager_cast.h"

#include <utility>
#include <vector>

#include "base/logging.h"
#include "base/macros.h"
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

void BindingsManagerCast::AddBinding(base::StringPiece binding_name,
                                     base::StringPiece binding_script) {
  cast_web_contents_->script_injector()->AddScriptForAllOrigins(
      binding_name.as_string(), binding_script);
}

void BindingsManagerCast::OnPageStateChanged(
    CastWebContents* cast_web_contents) {
  auto page_state = cast_web_contents->page_state();

  switch (page_state) {
    case CastWebContents::PageState::LOADING:
      cast_web_contents_->InjectScriptsIntoMainFrame();
      break;
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
    case CastWebContents::PageState::CLOSED:
      break;
  }
}

}  // namespace bindings
}  // namespace chromecast
