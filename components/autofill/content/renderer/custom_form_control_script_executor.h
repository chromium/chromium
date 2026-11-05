// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CONTENT_RENDERER_CUSTOM_FORM_CONTROL_SCRIPT_EXECUTOR_H_
#define COMPONENTS_AUTOFILL_CONTENT_RENDERER_CUSTOM_FORM_CONTROL_SCRIPT_EXECUTOR_H_

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "content/public/renderer/render_frame_observer.h"
#include "third_party/blink/public/web/web_node.h"
#include "v8/include/v8-forward.h"
#include "v8/include/v8-persistent-handle.h"

namespace autofill {

struct CustomFormControl;

// Runs the custom form control script of the current document in an isolated
// world, so that the script can access the frame's DOM without sharing globals
// with the page or with other features.
//
// A script must declare `function createCustomFormControlManager()`, which
// returns the script's manager: an object whose methods detect, read and fill
// the page's custom form controls (e.g. select-like widgets that aren't
// <select> elements). The executor keeps the manager until the isolated world's
// script context is released, e.g. when a new document is committed or the
// frame is detached.
//
// The executor is owned by its creator, not by the render frame. Once the frame
// is destroyed, the executor keeps no manager and its methods fail; the creator
// may destroy it then or later, even while `LoadScript()` runs a script (e.g.
// if the script detached the frame), but not during a call (see
// `CallFunction()`).
//
// The isolated world has a restrictive Content Security Policy: scripts can
// access the frame's DOM, but can't use `eval()`, load scripts or make network
// requests.
class CustomFormControlScriptExecutor : public content::RenderFrameObserver {
 public:
  // `world_id` is the id of the isolated world to run scripts in, which is
  // chosen by the creator (Chrome passes
  // `ISOLATED_WORLD_ID_AUTOFILL_CUSTOM_FORM_CONTROLS`). It must be a valid
  // embedder world id: positive, since 0 is the page's main world, and below
  // Blink's limit for embedder world ids. The constructor sets up the world's
  // Content Security Policy, which applies in all frames of the renderer
  // process.
  CustomFormControlScriptExecutor(content::RenderFrame& render_frame,
                                  int32_t world_id);
  CustomFormControlScriptExecutor(const CustomFormControlScriptExecutor&) =
      delete;
  CustomFormControlScriptExecutor& operator=(
      const CustomFormControlScriptExecutor&) = delete;
  ~CustomFormControlScriptExecutor() override;

  // Runs `script` and keeps the manager returned by its
  // `createCustomFormControlManager()`, replacing any previous one. Returns
  // false, and keeps no manager, if the script throws an exception, doesn't
  // declare `createCustomFormControlManager()`, or the function's result isn't
  // an object or is a function, array or promise (e.g. because the function is
  // `async`). Also returns false if running the script detaches the frame; the
  // executor may then already be destroyed when this returns.
  bool LoadScript(std::string_view script);

  // Returns whether a manager is loaded for the current document.
  bool has_manager() const { return !manager_.IsEmpty(); }

  // Calls the manager's `detectCustomFormControls()` and returns the valid
  // custom form controls it detected.
  std::vector<CustomFormControl> DetectCustomFormControls();

 private:
  friend class CustomFormControlScriptExecutorTestApi;

  // An argument of a call: a DOM node, which must not be null, or a string,
  // passed as `undefined` if it is `std::nullopt`.
  using Argument = std::variant<blink::WebNode, std::optional<std::u16string>>;

  // Runs with the isolate, the isolated world's context (which is entered) and
  // the value the method returned, so that the caller can convert the value to
  // C++ in that context. The context and the value are empty if the call
  // failed; the isolate is then null if the frame was already gone. The
  // callback runs outside of the call's `v8::TryCatch`, so it must catch
  // exceptions that converting the value throws.
  using ResultCallback = base::OnceCallback<
      void(v8::Isolate*, v8::Local<v8::Context>, v8::MaybeLocal<v8::Value>)>;

  // Calls the manager's method `function_name` with `arguments` in the isolated
  // world and runs `callback` with the result synchronously. Microtasks that
  // the call queues run later, at the next microtask checkpoint.
  // The call fails if there is no manager or no such method, if an argument
  // is a null node or can't be converted, if the method throws or returns a
  // promise, or if the world's script context is released during the call (e.g.
  // because the script detached the frame). Unlike during `LoadScript()`, the
  // owner must not destroy the executor while the call runs; `AutofillAgent`,
  // for example, is only deleted asynchronously after the frame is destroyed.
  //
  // Public methods for each method of the manager (e.g. to detect the custom
  // form controls), which convert the results to C++, are added together with
  // their callers.
  void CallFunction(std::string_view function_name,
                    std::vector<Argument> arguments,
                    ResultCallback callback);

  // Converts `argument` to a value of the current (isolated world) context. DOM
  // nodes are wrapped for that context. Returns an empty handle if the argument
  // can't be converted.
  static v8::MaybeLocal<v8::Value> ToV8Value(v8::Isolate* isolate,
                                             Argument& argument);

  // content::RenderFrameObserver:
  //
  // Called before a script context of the frame is destroyed: for the isolated
  // world's context, when a new document is committed in the frame or when the
  // frame is detached. The manager belongs to that context, so it must not
  // keep the context (and the document) alive.
  void WillReleaseScriptContext(v8::Local<v8::Context> context,
                                int32_t world_id) override;

  // Called when a navigation to a new document has committed, which replaces
  // the frame's script contexts. Not called for same-document navigations
  // (e.g. `history.pushState()`), which keep the document, its contexts and
  // the manager.
  void DidCommitProvisionalLoad(ui::PageTransition transition) override;

  // Called when the `RenderFrame` is destroyed. The executor is owned by its
  // creator and may outlive the frame until the creator destroys it, so this
  // drops the manager right away instead of in the destructor.
  void OnDestruct() override;

  // The isolated world that scripts run in.
  const int32_t world_id_;

  // The manager of the current document, or empty if no script is loaded.
  v8::Global<v8::Object> manager_;

  base::WeakPtrFactory<CustomFormControlScriptExecutor> weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CONTENT_RENDERER_CUSTOM_FORM_CONTROL_SCRIPT_EXECUTOR_H_
