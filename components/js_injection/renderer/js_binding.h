// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_JS_INJECTION_RENDERER_JS_BINDING_H_
#define COMPONENTS_JS_INJECTION_RENDERER_JS_BINDING_H_

#include <string>

#include "base/auto_reset.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/js_injection/common/interfaces.mojom.h"
#include "gin/arguments.h"
#include "gin/wrappable.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "third_party/blink/public/common/messaging/string_message_codec.h"
#include "v8/include/v8.h"

namespace v8 {
template <typename T>
class Global;
class Function;
}  // namespace v8

namespace content {
class RenderFrame;
}  // namespace content

namespace js_injection {
class JsCommunication;

// A gin::Wrappable class used for providing JavaScript API. JsCommunication
// creates an instance of JsBinding for each unique name exposed to the page.
// JsBinding is owned by v8.
class JsBinding final : public gin::Wrappable<JsBinding>,
                        public mojom::BrowserToJsMessaging {
 public:
  static gin::WrapperInfo kWrapperInfo;

  JsBinding(const JsBinding&) = delete;
  JsBinding& operator=(const JsBinding&) = delete;

  static base::WeakPtr<JsBinding> Install(
      content::RenderFrame* render_frame,
      const std::u16string& js_object_name,
      base::WeakPtr<JsCommunication> js_communication,
      v8::Isolate* isolate,
      v8::Local<v8::Context> context);

  // mojom::BrowserToJsMessaging implementation.
  void OnPostMessage(blink::WebMessagePayload message) override;

  void ReleaseV8GlobalObjects();

  void Bind(
      mojo::PendingAssociatedReceiver<mojom::BrowserToJsMessaging> receiver);

 protected:
  ~JsBinding() override;

 private:
  explicit JsBinding(content::RenderFrame* render_frame,
                     const std::u16string& js_object_name,
                     base::WeakPtr<JsCommunication> js_java_configurator);

  // gin::Wrappable implementation
  gin::ObjectTemplateBuilder GetObjectTemplateBuilder(
      v8::Isolate* isolate) override;

  // For jsObject.postMessage(message[, ports]) JavaScript API.
  void PostMessage(gin::Arguments* args);
  // For jsObject.addEventListener("message", listener) JavaScript API.
  void AddEventListener(gin::Arguments* args);
  // For jsObject.removeEventListener("message", listener) JavaScript API.
  void RemoveEventListener(gin::Arguments* args);
  // For get jsObject.onmessage.
  v8::Local<v8::Function> GetOnMessage(v8::Isolate* isolate);
  // For set jsObject.onmessage.
  void SetOnMessage(v8::Isolate* isolate, v8::Local<v8::Value> value);

  raw_ptr<content::RenderFrame> render_frame_;
  std::u16string js_object_name_;
  v8::Global<v8::Function> on_message_;
  std::vector<v8::Global<v8::Function>> listeners_;

  base::WeakPtr<JsCommunication> js_communication_;

  mojo::AssociatedReceiver<mojom::BrowserToJsMessaging> receiver_{this};

  base::WeakPtrFactory<JsBinding> weak_ptr_factory_{this};
};

}  // namespace js_injection

#endif  // COMPONENTS_JS_INJECTION_RENDERER_JS_BINDING_H_
