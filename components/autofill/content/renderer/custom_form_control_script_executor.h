// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CONTENT_RENDERER_CUSTOM_FORM_CONTROL_SCRIPT_EXECUTOR_H_
#define COMPONENTS_AUTOFILL_CONTENT_RENDERER_CUSTOM_FORM_CONTROL_SCRIPT_EXECUTOR_H_

#include <cstdint>
#include <string_view>

#include "base/memory/weak_ptr.h"
#include "content/public/renderer/render_frame_observer.h"
#include "v8/include/v8-forward.h"
#include "v8/include/v8-persistent-handle.h"

namespace autofill {

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
// may destroy it then or later, even while a script of the executor runs (e.g.
// if the script detached the frame).
class CustomFormControlScriptExecutor : public content::RenderFrameObserver {
 public:
  // `world_id` is the id of the isolated world to run scripts in, which is
  // chosen by the creator (Chrome passes
  // `ISOLATED_WORLD_ID_AUTOFILL_CUSTOM_FORM_CONTROLS`). It must be a valid
  // embedder world id: positive, since 0 is the page's main world, and below
  // Blink's limit for embedder world ids.
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

 private:
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
