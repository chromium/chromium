// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/renderer/custom_form_control_script_executor.h"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "base/check.h"
#include "base/check_op.h"
#include "base/strings/strcat.h"
#include "content/public/renderer/render_frame.h"
#include "third_party/blink/public/platform/scheduler/web_agent_group_scheduler.h"
#include "third_party/blink/public/platform/web_isolated_world_info.h"
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_script_source.h"
#include "url/origin.h"
#include "v8/include/v8-local-handle.h"
#include "v8/include/v8-object.h"
#include "v8/include/v8-value.h"

namespace autofill {

namespace {

// Returns JavaScript that runs `script` and evaluates to the result of the
// entry point, or to undefined if `script` doesn't declare it:
//
//   (createCustomFormControlManager => (() => {
//     <script>
//     ;return typeof createCustomFormControlManager === 'function'
//         ? createCustomFormControlManager() : undefined;
//   })())(undefined)
//
// - The inner function keeps the script's top-level declarations local, so
//   that they don't become globals of the isolated world, which would leak
//   into scripts loaded later for the same document.
// - The outer function's parameter shadows the entry point's name, so that if
//   `script` doesn't declare it, the lookup doesn't fall through to the
//   world's global object, which would find a function that an earlier script
//   for the same document assigned to `window.createCustomFormControlManager`.
// - The two functions are separate scopes, so that a script that declares the
//   entry point with `let` or `const` doesn't fail with a SyntaxError for
//   redeclaring the outer function's parameter.
// - The newline before `;return` ends a trailing `//` comment, and the
//   semicolon ends a last statement without one.
std::string WrapScript(std::string_view script) {
  // Every script declares this function, which returns the script's manager.
  static constexpr char kEntryPoint[] = "createCustomFormControlManager";
  return base::StrCat({"(", kEntryPoint, " => (() => {\n", script,
                       "\n;return typeof ", kEntryPoint, " === 'function' ? ",
                       kEntryPoint, "() : undefined;\n})())(undefined)"});
}

// Returns whether `value` can be a manager. `IsObject()` is also true for
// functions, arrays and promises (e.g. the result of an `async` entry point),
// so these are excluded explicitly.
bool IsManager(v8::Local<v8::Value> value) {
  return !value.IsEmpty() && value->IsObject() && !value->IsFunction() &&
         !value->IsArray() && !value->IsPromise();
}

// Sets the Content Security Policy, origin and name of the isolated world, once
// per renderer process. These are global to the renderer process, so they also
// apply to the world's contexts in other frames. Must always be called with the
// same `world_id`.
void EnsureIsolatedWorldSetUp(int32_t world_id) {
  // Only accessed on the renderer main thread.
  static std::optional<int32_t> last_used_world_id;
  if (last_used_world_id) {
    CHECK_EQ(*last_used_world_id, world_id);
    return;
  }
  last_used_world_id = world_id;

  // No eval, no script loading, no network requests.
  static constexpr char kWorldCsp[] =
      "default-src 'none'; script-src 'none'; connect-src 'none'";
  blink::WebIsolatedWorldInfo info;
  // An isolated world's origin can only add to what its requests may access
  // (e.g. for extensions); requests are still made with the page's origin. An
  // opaque origin adds nothing. Blink requires an origin to set a Content
  // Security Policy.
  info.security_origin = blink::WebSecurityOrigin(url::Origin());
  info.content_security_policy = blink::WebString::FromUtf8(kWorldCsp);
  info.human_readable_name =
      blink::WebString::FromUtf8("Autofill custom form controls");
  blink::SetIsolatedWorldInfo(world_id, info);
}

}  // namespace

CustomFormControlScriptExecutor::CustomFormControlScriptExecutor(
    content::RenderFrame& render_frame,
    int32_t world_id)
    : content::RenderFrameObserver(&render_frame), world_id_(world_id) {
  CHECK_GT(world_id_, blink::kMainDOMWorldId);
  CHECK(!blink::IsEqualOrExceedEmbedderWorldIdLimit(world_id_));
  // Before any script runs in the world.
  EnsureIsolatedWorldSetUp(world_id_);
}

CustomFormControlScriptExecutor::~CustomFormControlScriptExecutor() = default;

bool CustomFormControlScriptExecutor::LoadScript(std::string_view script) {
  manager_.Reset();
  if (!render_frame()) {
    return false;
  }

  blink::WebLocalFrame* frame = render_frame()->GetWebFrame();
  // All worlds of the frame, including the page's main world, share the
  // frame's isolate (the renderer main thread's V8 instance); each world has
  // its own context in it. `world_id_` selects the isolated world's context, so
  // the script doesn't share globals with the page.
  v8::Isolate* isolate = frame->GetAgentGroupScheduler()->Isolate();
  v8::HandleScope handle_scope(isolate);
  // Running the script may detach the frame, e.g. through event handlers of the
  // page that the script triggers, or by calling `frameElement.remove()` in an
  // iframe. That releases the world's script context and destroys the
  // `RenderFrame`, and the creator may then even destroy this executor.
  base::WeakPtr<CustomFormControlScriptExecutor> weak_this =
      weak_ptr_factory_.GetWeakPtr();
  v8::Local<v8::Value> manager =
      frame->ExecuteScriptInIsolatedWorldAndReturnValue(
          world_id_,
          blink::WebScriptSource(
              blink::WebString::FromUtf8(WrapScript(script))),
          blink::BackForwardCacheAware::kAllow);
  if (!weak_this) {
    return false;
  }
  // Don't keep a manager of a released context: if the script detached the
  // frame, `WillReleaseScriptContext()` has already run and wouldn't drop it.
  // (Navigations that the script starts commit asynchronously, after this
  // returns, and drop the manager then.)
  if (!render_frame()) {
    return false;
  }
  // The script threw (empty result), didn't declare the entry point
  // (undefined), or the entry point returned something that can't be a
  // manager.
  if (!IsManager(manager)) {
    return false;
  }
  manager_.Reset(isolate, manager.As<v8::Object>());
  return true;
}

void CustomFormControlScriptExecutor::WillReleaseScriptContext(
    v8::Local<v8::Context> context,
    int32_t world_id) {
  if (world_id == world_id_) {
    manager_.Reset();
  }
}

void CustomFormControlScriptExecutor::DidCommitProvisionalLoad(
    ui::PageTransition transition) {
  manager_.Reset();
}

void CustomFormControlScriptExecutor::OnDestruct() {
  manager_.Reset();
}

}  // namespace autofill
