// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BIDI_MAPPER_HOST_BIDI_MAPPER_HOST_H_
#define COMPONENTS_BIDI_MAPPER_HOST_BIDI_MAPPER_HOST_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <string_view>

namespace content {
class BrowserContext;
}  // namespace content

namespace bidi_mapper {

// Runs an instance of the BiDi mapper, the WebDriver BiDi implementation
// bundled with the browser, and exchanges WebDriver BiDi messages with it.
//
// Each host runs its own mapper in a hidden WebContents and serves at most one
// BiDi session. A session is not limited to the BrowserContext of the host: it
// automates the whole browser.
//
// The WebContents is a privileged one, see
// content::WebContents::PrivilegedParams.
//
// The host plays the role of the remote end toward the mapper:
//  - `session.status`, `session.new` and `session.end` follow the lifetime of
//    the host. `session.end` closes the host.
//  - Commands other than `session.status` and `session.new` are refused until
//    a session is created.
//  - The `goog:cdp` module, which gives raw access to the DevTools protocol,
//    is refused.
//  - `script.evaluate` and `script.callFunction`, which run script of the
//    client in the pages that a session automates, are refused.
//
// Must be used on the UI thread.
class BidiMapperHost {
 public:
  enum class FailureReason {
    // The embedder does not allow attaching to the mapper over the DevTools
    // protocol, e.g. because DevTools are disabled by policy.
    kNotAllowed,
    // The mapper could not be loaded or started.
    kFailed,
  };

  class Client {
   public:
    virtual ~Client() = default;

    // Called once when the mapper has started.
    virtual void OnStarted() = 0;

    // Called once if the mapper failed to start. The host does nothing and can
    // only be destroyed.
    virtual void OnFailed(FailureReason reason) = 0;

    // Receives the BiDi messages sent by the remote end, as JSON text.
    virtual void OnMessage(const std::string& message) = 0;

    // Called at most once, when a host that was started stops working: the
    // BiDi session ended, or the mapper crashed or was closed. A closed host
    // does nothing; it can only be destroyed.
    virtual void OnClosed() = 0;
  };

  // Returns true if hosts can be created (features::kChromeDriverExtensionApi
  // is enabled).
  static bool IsEnabled();

  // Creates and starts a host that runs the mapper in `browser_context`.
  // Returns nullptr unless IsEnabled().
  //
  // `privileged_feature_id` is the WebContents::PrivilegedParams::feature_id
  // of the WebContents that runs the mapper. The embedder must assign an id
  // that does not collide with its other privileged features.
  //
  // Both `browser_context` and `client` must outlive the returned host.
  //
  // `client` methods are invoked asynchronously, and never after the host is
  // destroyed. The host may be destroyed from within any of them.
  static std::unique_ptr<BidiMapperHost> Create(
      content::BrowserContext* browser_context,
      int32_t privileged_feature_id,
      Client* client);

  virtual ~BidiMapperHost() = default;

  // Sends a BiDi message, as JSON text, to the remote end. Must only be called
  // after OnStarted(). A host closes before OnClosed() tells its client, so the
  // client may send a message to a closed host. Such a message is dropped.
  virtual void SendBidiMessage(std::string_view message) = 0;
};

}  // namespace bidi_mapper

#endif  // COMPONENTS_BIDI_MAPPER_HOST_BIDI_MAPPER_HOST_H_
