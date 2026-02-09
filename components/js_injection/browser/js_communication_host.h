// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_JS_INJECTION_BROWSER_JS_COMMUNICATION_HOST_H_
#define COMPONENTS_JS_INJECTION_BROWSER_JS_COMMUNICATION_HOST_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "components/js_injection/common/enum.mojom-forward.h"
#include "components/origin_matcher/origin_matcher.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/web_contents_observer.h"

namespace content {
class RenderFrameHost;
}  // namespace content

namespace js_injection {

struct JsObject;
class WebMessageHostFactory;

struct JavaScriptExecutable {
  JavaScriptExecutable(std::u16string script,
                       mojom::DocumentInjectionTime event_type,
                       origin_matcher::OriginMatcher allowed_origin_rules,
                       int32_t world_identifier,
                       int32_t script_id);
  JavaScriptExecutable(JavaScriptExecutable&) = delete;
  JavaScriptExecutable& operator=(JavaScriptExecutable&) = delete;
  JavaScriptExecutable(JavaScriptExecutable&&) = default;
  JavaScriptExecutable& operator=(JavaScriptExecutable&&) = default;

  std::u16string script_;
  origin_matcher::OriginMatcher allowed_origin_rules_;
  int32_t script_id_;
  mojom::DocumentInjectionTime event_type_;
  int32_t world_identifier_;
};

// This class is 1:1 with WebContents, when AddWebMessageListener() is called,
// it stores the information in this class and send them to renderer side
// JsCommunication if there is any. When RenderFrameCreated() gets called, it
// needs to configure that new RenderFrame with the information stores in this
// class.
class JsCommunicationHost : public content::WebContentsObserver {
 public:
  explicit JsCommunicationHost(content::WebContents* web_contents);

  JsCommunicationHost(const JsCommunicationHost&) = delete;
  JsCommunicationHost& operator=(const JsCommunicationHost&) = delete;

  ~JsCommunicationHost() override;

  // Captures the result of adding script. There are two possibilities when
  // adding script: there was an error, in which case |error_message| is set,
  // otherwise the add was successful and |script_id| is set.
  struct AddScriptResult {
    AddScriptResult();
    AddScriptResult(const AddScriptResult&);
    AddScriptResult& operator=(const AddScriptResult&);
    ~AddScriptResult();

    std::optional<std::string> error_message;
    std::optional<int> script_id;
  };

  // Native side AddPersistentJavaScript, returns an error message if the
  // parameters didn't pass necessary checks.
  AddScriptResult AddPersistentJavaScript(
      std::u16string script,
      mojom::DocumentInjectionTime event_type,
      const std::vector<std::string>& allowed_origin_rules,
      int32_t world_identifier);

  bool RemovePersistentJavaScript(int script_id);

  const std::vector<JavaScriptExecutable>& GetPersistentJavaScripts() const;

  // Adds a new WebMessageHostFactory. For any urls that match
  // |allowed_origin_rules|, |js_object_name| is registered as a JS object that
  // can be used by script on the page to send and receive messages. Returns
  // an empty string on success. On failure, the return string gives the error
  // message.
  std::u16string AddWebMessageHostFactory(
      std::unique_ptr<WebMessageHostFactory> factory,
      const std::u16string& js_object_name,
      const std::vector<std::string>& allowed_origin_rules,
      int32_t world_identifier);

  // Returns the factory previously registered under the specified name.
  void RemoveWebMessageHostFactory(const std::u16string& js_object_name,
                                   int32_t world_identifier);

  struct RegisteredFactory {
    std::u16string js_name;
    origin_matcher::OriginMatcher allowed_origin_rules;
    int32_t world_id;
    raw_ptr<WebMessageHostFactory> factory = nullptr;
  };

  // Returns the registered factories.
  std::vector<RegisteredFactory> GetWebMessageHostFactories();

  // content::WebContentsObserver implementations
  void RenderFrameCreated(content::RenderFrameHost* render_frame_host) override;
  void RenderFrameDeleted(content::RenderFrameHost* render_frame_host) override;
  void RenderFrameHostStateChanged(
      content::RenderFrameHost* render_frame_host,
      content::RenderFrameHost::LifecycleState old_state,
      content::RenderFrameHost::LifecycleState new_state) override;
  void PrimaryPageChanged(content::Page& page) override;

 private:
  class JsToBrowserMessagingList;
  void NotifyFrameForWebMessageListener(
      content::RenderFrameHost* render_frame_host);
  void NotifyFrameForAllPersistentJavaScripts(
      content::RenderFrameHost* render_frame_host);
  void NotifyFrameForPersistentJavaScript(
      const JavaScriptExecutable* script,
      content::RenderFrameHost* render_frame_host);
  void NotifyFrameForRemovePersistentJavaScript(
      int32_t script_id,
      content::RenderFrameHost* render_frame_host);

  int32_t next_script_id_ = 0;
  std::vector<JavaScriptExecutable> sticky_scripts_;
  std::vector<std::unique_ptr<JsObject>> js_objects_;
  std::map<content::GlobalRenderFrameHostId,
           std::unique_ptr<JsToBrowserMessagingList>>
      js_to_browser_messagings_;
  bool has_navigation_listener_ = false;
};

}  // namespace js_injection

#endif  // COMPONENTS_JS_INJECTION_BROWSER_JS_COMMUNICATION_HOST_H_
