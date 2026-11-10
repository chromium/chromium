// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CONTENT_RENDERER_CUSTOM_FORM_CONTROL_SCRIPT_EXECUTOR_H_
#define COMPONENTS_AUTOFILL_CONTENT_RENDERER_CUSTOM_FORM_CONTROL_SCRIPT_EXECUTOR_H_

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/types/strong_alias.h"
#include "content/public/renderer/render_frame_observer.h"
#include "third_party/blink/public/web/web_node.h"
#include "v8/include/v8-forward.h"
#include "v8/include/v8-persistent-handle.h"

namespace autofill {

struct CustomFormControl;
struct CustomFormControlOptions;

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
// if the script detached the frame), but not while a called method runs (see
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
  // executor may then already be destroyed when this returns. A call that
  // awaits resolution of a promise of the previous manager fails first.
  bool LoadScript(std::string_view script);

  // Returns whether a manager is loaded for the current document.
  bool has_manager() const { return !manager_.IsEmpty(); }

  // Calls the manager's `detectCustomFormControls()` and returns the valid
  // custom form controls it detected. The method must not return a promise.
  std::vector<CustomFormControl> DetectCustomFormControls();

  // Calls the manager's `closePopover()` with `node`, e.g. to close a popover
  // that was opened to extract the options, and runs `callback` with whether
  // the call succeeded. `callback` may run asynchronously.
  void ClosePopover(const blink::WebNode& node,
                    base::OnceCallback<void(bool success)> callback);

  // Calls the manager's `extractOptions()` with `node` and runs `callback` with
  // the valid options, or with `std::nullopt` if the call fails. `callback` may
  // run asynchronously.
  void ExtractOptions(
      const blink::WebNode& node,
      base::OnceCallback<void(std::optional<CustomFormControlOptions>)>
          callback);

 private:
  friend class CustomFormControlScriptExecutorTestApi;

  // Whether a called method may return a promise.
  using MayReturnPromise = base::StrongAlias<class MayReturnPromiseTag, bool>;

  // An argument of a call: a DOM node, which must not be null, or a string,
  // passed as `undefined` if it is `std::nullopt`.
  using Argument = std::variant<blink::WebNode, std::optional<std::u16string>>;

  // Runs with the isolate, the isolated world's context (which is entered) and
  // the value the method returned, so that the caller can convert the value to
  // C++ in that context. The context and the value are empty if the call
  // failed. The isolate is then null if the frame is gone or the executor is
  // being destroyed. The callback runs outside of the call's `v8::TryCatch`, so
  // it must catch exceptions that converting the value throws.
  using ResultCallback = base::OnceCallback<
      void(v8::Isolate*, v8::Local<v8::Context>, v8::MaybeLocal<v8::Value>)>;

  // A call that waits for its promise.
  struct PendingCall;

  // Calls the manager's method `function_name` with `arguments` in the isolated
  // world. Then runs `callback` with the result. This is a helper function for
  // public methods that hide the V8 types from the public interface.
  //
  // The called `function_name` may return a promise only if
  // `may_return_promise` is true. Then the result is the value that the promise
  // is fulfilled with. If `may_return_promise` is false, a returned promise
  // fails the call.
  // `CallFunction()` doesn't run microtasks. So a returned promise is usually
  // still pending when the method returns.
  //
  // The call fails, and `callback` gets an empty result, in these cases:
  // - There is no manager or no method `function_name`.
  // - An argument is a null node or can't be converted.
  // - The method throws.
  // - The promise that the method returned is rejected.
  // - Another `CallFunction()` call still waits for its promise. Calls to
  //   `CallFunction()` aren't queued.
  // - The world's script context is released. This happens, for example, when
  //   the script detaches the frame or a new document is committed.
  // - `LoadScript()` replaces the manager while the call waits for its promise.
  // - The executor is destroyed while the call waits for its promise.
  //
  // About `callback`:
  // - It runs exactly once.
  // - It runs before `CallFunction()` returns, unless the method returned a
  //   pending promise. In that case, it runs when the promise settles or the
  //   call fails.
  // - It may call `CallFunction()` again.
  // - It must not call `LoadScript()`. It may run while the world's script
  //   context is being released, and it can't tell. Running script then isn't
  //   allowed.
  // - It may run while the executor is being destroyed. Then it must not access
  //   the executor.
  //
  // The owner must not destroy the executor while the method runs. The owner
  // may destroy the executor while the call waits for its promise.
  void CallFunction(MayReturnPromise may_return_promise,
                    std::string_view function_name,
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
  // keep the context (and the document) alive. Also fails `pending_call_`,
  // whose promise can no longer settle.
  void WillReleaseScriptContext(v8::Local<v8::Context> context,
                                int32_t world_id) override;

  // Called when a navigation to a new document has committed, which replaces
  // the frame's script contexts. Not called for same-document navigations
  // (e.g. `history.pushState()`), which keep the document, its contexts and
  // the manager. Also fails `pending_call_`.
  void DidCommitProvisionalLoad(ui::PageTransition transition) override;

  // Called when the `RenderFrame` is destroyed. The executor is owned by its
  // creator and may outlive the frame until the creator destroys it, so this
  // drops the manager, and fails `pending_call_`, right away instead of in the
  // destructor.
  void OnDestruct() override;

  // Runs `callback` right away if `promise` is already settled. Otherwise,
  // keeps `callback` in `pending_call_` and attaches reactions to `promise`
  // that run it once `promise` settles.
  void SetCallbackForPromise(v8::Isolate* isolate,
                             v8::Local<v8::Context> context,
                             v8::Local<v8::Promise> promise,
                             ResultCallback callback);

  // Reactions to the promise. They find the waiting executor by the call id
  // and run its pending callback with the fulfilled value, or with an empty
  // value if the promise was rejected.
  static void OnPromiseFulfilled(
      const v8::FunctionCallbackInfo<v8::Value>& info);
  static void OnPromiseRejected(
      const v8::FunctionCallbackInfo<v8::Value>& info);

  // Runs the callback of `pending_call_` with `value`, which is empty if the
  // call failed, and resets `pending_call_`.
  void RunPendingCallback(v8::Isolate* isolate,
                          v8::Local<v8::Context> context,
                          v8::MaybeLocal<v8::Value> value);

  // The isolated world that scripts run in.
  const int32_t world_id_;

  // The manager of the current document, or empty if no script is loaded.
  v8::Global<v8::Object> manager_;

  // The call that waits for a promise, if any.
  std::unique_ptr<PendingCall> pending_call_;

  base::WeakPtrFactory<CustomFormControlScriptExecutor> weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CONTENT_RENDERER_CUSTOM_FORM_CONTROL_SCRIPT_EXECUTOR_H_
