// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast_receiver/browser/bindings_message_port_connector.h"

#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "components/cast/message_port/cast/message_port_cast.h"
#include "components/cast/named_message_port_connector/grit/named_message_port_connector_resources.h"
#include "content/public/browser/message_port_provider.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/common/messaging/web_message_port.h"
#include "ui/base/resource/resource_bundle.h"

namespace cast_receiver {
namespace {

constexpr uint64_t kNamedMessagePortConnectorBindingsId = 1000;

// Adds a return value to a void function, as the caller requires the method
// signature return a bool and base::BindRepeating cannot use weak_ptrs with
// non-void functions.
bool AddReturnValue(
    base::WeakPtr<BindingsMessagePortConnector> ptr,
    base::RepeatingCallback<
        void(std::string_view, std::unique_ptr<cast_api_bindings::MessagePort>)>
        callback,
    std::string_view port_name,
    std::unique_ptr<cast_api_bindings::MessagePort> port) {
  callback.Run(std::move(port_name), std::move(port));
  return !!ptr;
}

}  // namespace

BindingsMessagePortConnector::Client::~Client() = default;

BindingsMessagePortConnector::BindingsMessagePortConnector(
    content::WebContents* web_contents,
    Client& client)
    : content::WebContentsObserver(web_contents),
      PageStateObserver(web_contents),
      client_(client),
      weak_factory_(this) {
  // Register the port connection JS script for early injection.
  std::string bindings_script_string =
      ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
          IDR_PORT_CONNECTOR_JS);
  if (!bindings_script_string.empty()) {
    AddBeforeLoadJavaScript(kNamedMessagePortConnectorBindingsId,
                            bindings_script_string);
  }
}

BindingsMessagePortConnector::~BindingsMessagePortConnector() = default;

void BindingsMessagePortConnector::ConnectToBindingsService() {
  auto on_port_connected_cb =
      base::BindRepeating(&BindingsMessagePortConnector::OnPortConnected,
                          weak_factory_.GetWeakPtr());
  RegisterPortHandler(base::BindRepeating(&AddReturnValue,
                                          weak_factory_.GetWeakPtr(),
                                          std::move(on_port_connected_cb)));

  // Fetch bindings and inject scripts into |script_injector_|.
  std::vector<Client::ApiBinding> bindings = client_->GetAllBindings();
  if (bindings.empty()) {
    LOG(ERROR) << "Received empty bindings.";
    client_->OnError();
  } else {
    constexpr uint64_t kBindingsIdStart = 0xFF0000;

    // Enumerate and inject all scripts in |bindings|.
    uint64_t bindings_id = kBindingsIdStart;
    for (auto& entry : bindings) {
      AddBeforeLoadJavaScript(bindings_id++, entry.script);
    }
  }
}

void BindingsMessagePortConnector::AddBeforeLoadJavaScript(
    uint64_t id,
    std::string_view script) {
  script_injector_.AddScriptForAllOrigins(id, std::string(script));
}

void BindingsMessagePortConnector::OnPortConnected(
    std::string_view port_name,
    std::unique_ptr<cast_api_bindings::MessagePort> port) {
  client_->Connect(
      std::string(port_name),
      cast_api_bindings::MessagePortCast::FromMessagePort(port.get())
          ->TakePort()
          .PassPort());
}

void BindingsMessagePortConnector::OnPageLoadComplete() {
  // Send the port connection message to the page once it is loaded.
  std::string connect_message;
  std::unique_ptr<cast_api_bindings::MessagePort> port;
  GetConnectMessage(&connect_message, &port);

  std::vector<blink::WebMessagePort> ports;
  ports.push_back(
      cast_api_bindings::MessagePortCast::FromMessagePort(port.get())
          ->TakePort());

  DCHECK(!connect_message.empty());
  std::u16string data_utf16 = base::UTF8ToUTF16(connect_message);

  const std::optional<std::u16string> target_origin_utf16;
  content::MessagePortProvider::PostMessageToFrame(
      web_contents()->GetPrimaryPage(), std::u16string(), target_origin_utf16,
      data_utf16, std::move(ports));
}

void BindingsMessagePortConnector::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInPrimaryMainFrame() ||
      navigation_handle->IsSameDocument()) {
    return;
  }

  // Main frame has an ongoing navigation. This might overwrite a
  // previously active navigation. We only care about tracking
  // the most recent main frame navigation.
  active_navigation_ = navigation_handle;
}

void BindingsMessagePortConnector::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  // Ignore sub-frame and non-current main frame navigation.
  if (navigation_handle != active_navigation_) {
    return;
  }
  active_navigation_ = nullptr;
}

void BindingsMessagePortConnector::ReadyToCommitNavigation(
    content::NavigationHandle* navigation_handle) {
  // Skip injecting bindings scripts if |navigation_handle| is not
  // 'current' main frame navigation, e.g. another DidStartNavigation is
  // emitted. Also skip injecting for same document navigation and error page.
  if (navigation_handle == active_navigation_ &&
      !navigation_handle->IsErrorPage()) {
    // Injects registered bindings script into the main frame.
    script_injector_.InjectScriptsForURL(
        navigation_handle->GetURL(), navigation_handle->GetRenderFrameHost());
  }
}

}  // namespace cast_receiver
