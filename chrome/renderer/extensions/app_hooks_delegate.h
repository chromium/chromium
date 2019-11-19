// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_EXTENSIONS_APP_HOOKS_DELEGATE_H_
#define CHROME_RENDERER_EXTENSIONS_APP_HOOKS_DELEGATE_H_

#include <string>

#include "base/macros.h"
#include "chrome/renderer/extensions/chrome_v8_extension_handler.h"
#include "extensions/renderer/bindings/api_binding_hooks_delegate.h"
#include "v8/include/v8.h"

class GURL;

namespace content {
class RenderFrame;
}

namespace extensions {
class APIRequestHandler;
class Dispatcher;
class ScriptContext;

// The custom hooks for the chrome.app API.
class AppHooksDelegate : public APIBindingHooksDelegate {
 public:
  using GetterCallback =
      base::Callback<void(const v8::PropertyCallbackInfo<v8::Value>& info)>;

  AppHooksDelegate(Dispatcher* dispatcher, APIRequestHandler* request_handler);
  ~AppHooksDelegate() override;

  // APIBindingHooksDelegate:
  APIBindingHooks::RequestResult HandleRequest(
      const std::string& method_name,
      const APISignature* signature,
      v8::Local<v8::Context> context,
      std::vector<v8::Local<v8::Value>>* arguments,
      const APITypeReferenceMap& refs) override;
  void InitializeTemplate(v8::Isolate* isolate,
                          v8::Local<v8::ObjectTemplate> object_template,
                          const APITypeReferenceMap& type_refs) override;

  // Total misnomer. Returns true if there is a hosted app associated with
  // |script_context| active in this process. This naming is to match the
  // chrome.app function it implements.
  bool GetIsInstalled(ScriptContext* script_context) const;

 private:
  // A helper class to handle IPC message sending/receiving. Isolated from
  // AppHooksDelegate to avoid multiple inheritence.
  class IPCHelper : public ChromeV8ExtensionHandler {
   public:
    explicit IPCHelper(AppHooksDelegate* owner);
    ~IPCHelper() override;

    // Sends the IPC message to the browser to get the install state of the
    // app.
    void SendGetAppInstallStateMessage(content::RenderFrame* render_frame,
                                       const GURL& url,
                                       int request_id);

   private:
    // IPC::Listener:
    bool OnMessageReceived(const IPC::Message& message) override;

    // Handle for ExtensionMsg_GetAppInstallStateResponse; just forwards to
    // AppHooksDelegate.
    void OnAppInstallStateResponse(const std::string& state, int request_id);

    AppHooksDelegate* owner_ = nullptr;

    DISALLOW_COPY_AND_ASSIGN(IPCHelper);
  };

  // Returns the manifest of the extension associated with the frame.
  v8::Local<v8::Value> GetDetails(ScriptContext* script_context) const;

  // Determines the install state for the extension associated with the frame.
  // Note that this could be "disabled" for hosted apps when the user visits the
  // site but doesn't have the app enabled. The response is determined
  // asynchronously before completing the request with the given |request_id|.
  void GetInstallState(ScriptContext* script_context, int request_id);

  // Returns the "running state" (one of running, cannot run, and ready to run)
  // for the extension associated with the frame of the script context.
  const char* GetRunningState(ScriptContext* script_context) const;

  // Handle for ExtensionMsg_GetAppInstallStateResponse.
  void OnAppInstallStateResponse(const std::string& state, int request_id);

  // Dispatcher handle. Not owned.
  Dispatcher* dispatcher_ = nullptr;

  APIRequestHandler* request_handler_ = nullptr;

  IPCHelper ipc_helper_;

  GetterCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(AppHooksDelegate);
};

}  // namespace extensions

#endif  // CHROME_RENDERER_EXTENSIONS_APP_HOOKS_DELEGATE_H_
