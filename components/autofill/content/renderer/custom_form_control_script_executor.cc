// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/renderer/custom_form_control_script_executor.h"

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/map_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"
#include "base/no_destructor.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/strcat.h"
#include "components/autofill/content/renderer/custom_form_control_util.h"
#include "components/autofill/content/renderer/form_autofill_util.h"
#include "components/autofill/core/common/form_field_data.h"
#include "content/public/renderer/render_frame.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"
#include "third_party/blink/public/platform/scheduler/web_agent_group_scheduler.h"
#include "third_party/blink/public/platform/web_isolated_world_info.h"
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/public/web/web_form_control_element.h"
#include "third_party/blink/public/web/web_form_element.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_script_source.h"
#include "url/origin.h"
#include "v8/include/v8-container.h"
#include "v8/include/v8-context.h"
#include "v8/include/v8-exception.h"
#include "v8/include/v8-function-callback.h"
#include "v8/include/v8-function.h"
#include "v8/include/v8-local-handle.h"
#include "v8/include/v8-microtask-queue.h"
#include "v8/include/v8-object.h"
#include "v8/include/v8-primitive.h"
#include "v8/include/v8-promise.h"
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

// Maps the id of each call that waits for a promise to the executor that made
// it. There is at most one such call per executor, but there is one executor
// per frame, and several frames (e.g. same-site iframes) can share a renderer
// process, so the map may have several entries.
//
// When the promise settles, V8 runs a reaction (`OnPromiseFulfilled()` or
// `OnPromiseRejected()`) that only knows the call id, and uses this map to find
// the executor. The reaction can't point to the executor directly, since the
// executor may be destroyed before the promise settles. Instead, the map is
// static, outliving all executors, and executors erase their entry when the
// call ends. A reaction for a call that ended finds no entry and does nothing.
//
// Call ids come from `g_next_call_id`, which only increases, so an id is never
// reused. The reactions get the id as a `Number`, which represents integers up
// to 2^53 exactly. Both are only used on the renderer's main thread.
std::map<uint64_t, raw_ref<CustomFormControlScriptExecutor>>&
GetCallsAwaitingPromise() {
  static base::NoDestructor<
      std::map<uint64_t, raw_ref<CustomFormControlScriptExecutor>>>
      calls;
  return *calls;
}
uint64_t g_next_call_id = 1;

// Returns the executor that waits for the call of a promise reaction, whose
// data is the call id (see `SetCallbackForPromise()`), or null if the call has
// ended.
CustomFormControlScriptExecutor* GetWaitingExecutor(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  raw_ref<CustomFormControlScriptExecutor>* executor = base::FindOrNull(
      GetCallsAwaitingPromise(),
      static_cast<uint64_t>(info.DataV2().As<v8::Number>()->Value()));
  return executor ? &executor->get() : nullptr;
}

// Returns the isolate of `render_frame`, or null if the frame is gone.
v8::Isolate* GetIsolate(content::RenderFrame* render_frame) {
  return render_frame
             ? render_frame->GetWebFrame()->GetAgentGroupScheduler()->Isolate()
             : nullptr;
}

// Returns the document of `render_frame`, or null if the frame is gone.
blink::WebDocument GetDocument(content::RenderFrame* render_frame) {
  return render_frame ? render_frame->GetWebFrame()->GetDocument()
                      : blink::WebDocument();
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

v8::MaybeLocal<v8::Value> GetProperty(v8::Isolate* isolate,
                                      v8::Local<v8::Context> context,
                                      v8::Local<v8::Object> obj,
                                      std::string_view key) {
  v8::Local<v8::String> v8_key;
  if (!v8::String::NewFromUtf8(isolate, key.data(), v8::NewStringType::kNormal,
                               base::checked_cast<int>(key.size()))
           .ToLocal(&v8_key)) {
    return {};
  }
  return obj->Get(context, v8_key);
}

std::optional<std::u16string> GetStringProperty(v8::Isolate* isolate,
                                                v8::Local<v8::Context> context,
                                                v8::Local<v8::Object> obj,
                                                std::string_view key) {
  v8::Local<v8::Value> val;
  if (!GetProperty(isolate, context, obj, key).ToLocal(&val) ||
      !val->IsString()) {
    return std::nullopt;
  }
  v8::Local<v8::String> str = val.As<v8::String>();
  std::u16string utf16(str->Length(), u'\0');
  str->Write(isolate, 0, str->Length(),
             reinterpret_cast<uint16_t*>(utf16.data()));
  return utf16;
}

std::optional<bool> GetBoolProperty(v8::Isolate* isolate,
                                    v8::Local<v8::Context> context,
                                    v8::Local<v8::Object> obj,
                                    std::string_view key) {
  v8::Local<v8::Value> val;
  if (!GetProperty(isolate, context, obj, key).ToLocal(&val) ||
      !val->IsBoolean()) {
    return std::nullopt;
  }
  return val->BooleanValue(isolate);
}

v8::MaybeLocal<v8::Object> GetObjectProperty(v8::Isolate* isolate,
                                             v8::Local<v8::Context> context,
                                             v8::Local<v8::Object> obj,
                                             std::string_view key) {
  v8::Local<v8::Value> val;
  if (!GetProperty(isolate, context, obj, key).ToLocal(&val) ||
      !val->IsObject()) {
    return {};
  }
  return val.As<v8::Object>();
}

blink::WebElement GetElementProperty(v8::Isolate* isolate,
                                     v8::Local<v8::Context> context,
                                     v8::Local<v8::Object> obj,
                                     std::string_view key) {
  v8::Local<v8::Value> val;
  if (!GetProperty(isolate, context, obj, key).ToLocal(&val)) {
    return blink::WebElement();
  }
  return blink::WebElement::FromV8Value(isolate, val);
}

// Transforms `obj` into a `SelectOption`. Returns `std::nullopt` if `text`
// isn't a string.
std::optional<SelectOption> ParseSelectOption(v8::Isolate* isolate,
                                              v8::Local<v8::Context> context,
                                              v8::Local<v8::Object> obj) {
  std::optional<std::u16string> text =
      GetStringProperty(isolate, context, obj, "text");
  if (!text) {
    return std::nullopt;
  }
  std::optional<std::u16string> value =
      GetStringProperty(isolate, context, obj, "value");
  return SelectOption{
      // Like for standard form controls, default the value to the text.
      .value = std::move(value).value_or(*text),
      .text = *std::move(text),
  };
}

// Transforms `item` into a `CustomFormControl`, if it is valid.
// `item` follows the schema at http://shortn/_gxfEg4OfoT.
std::optional<CustomFormControl> ParseCustomFormControl(
    v8::Isolate* isolate,
    v8::Local<v8::Context> context,
    v8::Local<v8::Value> item) {
  if (!item->IsObject()) {
    return std::nullopt;
  }
  v8::Local<v8::Object> obj = item.As<v8::Object>();

  // TODO(crbug.com/581337039): Extract and validate "controlType".
  blink::WebElement canonical_dom_node =
      GetElementProperty(isolate, context, obj, "canonicalDomNode");
  std::optional<std::u16string> field_type =
      GetStringProperty(isolate, context, obj, "fieldType");
  std::optional<bool> is_active =
      GetBoolProperty(isolate, context, obj, "isActive");
  if (!canonical_dom_node || !field_type || !is_active) {
    return std::nullopt;
  }

  std::optional<SelectOption> current_option;
  v8::Local<v8::Object> current_value_obj;
  if (GetObjectProperty(isolate, context, obj, "currentValue")
          .ToLocal(&current_value_obj)) {
    current_option = ParseSelectOption(isolate, context, current_value_obj);
    if (!current_option) {
      return std::nullopt;
    }
  }

  return CustomFormControl{
      .canonical_dom_node = std::move(canonical_dom_node),
      .field_type = *std::move(field_type),
      .is_active = *is_active,
      .label = GetStringProperty(isolate, context, obj, "label"),
      .current_option = std::move(current_option),
  };
}

// Transforms `result`, the result of `extractOptions()`, into
// `CustomFormControlOptions`. Returns `std::nullopt` if `options` isn't an
// array or a getter of `result` throws. Skips invalid entries of `options`.
std::optional<CustomFormControlOptions> ParseCustomFormControlOptions(
    v8::Isolate* isolate,
    v8::Local<v8::Context> context,
    v8::Local<v8::Value> result) {
  if (!result->IsObject()) {
    return std::nullopt;
  }
  v8::Local<v8::Object> obj = result.As<v8::Object>();
  v8::TryCatch try_catch(isolate);
  v8::Local<v8::Value> options;
  if (!GetProperty(isolate, context, obj, "options").ToLocal(&options) ||
      !options->IsArray()) {
    return std::nullopt;
  }
  std::optional<bool> popover_needs_to_be_closed =
      GetBoolProperty(isolate, context, obj, "popoverNeedsToBeClosed");
  if (try_catch.HasCaught()) {
    return std::nullopt;
  }
  CustomFormControlOptions extracted{
      .popover_needs_to_be_closed = popover_needs_to_be_closed.value_or(false),
  };
  v8::Local<v8::Array> array = options.As<v8::Array>();
  const uint32_t length = array->Length();
  for (uint32_t i = 0; i < length; ++i) {
    v8::TryCatch item_try_catch(isolate);
    v8::Local<v8::Value> item;
    if (!array->Get(context, i).ToLocal(&item) || !item->IsObject()) {
      continue;
    }
    std::optional<SelectOption> option =
        ParseSelectOption(isolate, context, item.As<v8::Object>());
    if (option && !item_try_catch.HasCaught()) {
      extracted.options.push_back(*std::move(option));
    }
  }
  return extracted;
}

// Transforms `result`, the result of `setValue()`, into a
// `CustomFormControlSetValueResult`. Returns `kExecutionError` if its `status`
// isn't one of the statuses of the manager contract or its getter throws.
CustomFormControlSetValueResult ParseSetValueResult(
    v8::Isolate* isolate,
    v8::Local<v8::Context> context,
    v8::Local<v8::Value> result) {
  using enum CustomFormControlSetValueResult;
  if (!result->IsObject()) {
    return kExecutionError;
  }
  v8::TryCatch try_catch(isolate);
  std::optional<std::u16string> status =
      GetStringProperty(isolate, context, result.As<v8::Object>(), "status");
  if (try_catch.HasCaught() || !status) {
    return kExecutionError;
  }
  if (*status == u"OK") {
    return kOk;
  }
  if (*status == u"OPTION_NOT_FOUND") {
    return kOptionNotFound;
  }
  return kExecutionError;
}

}  // namespace

// A call whose promise hasn't settled yet.
struct CustomFormControlScriptExecutor::PendingCall {
  // The key of the call in `GetCallsAwaitingPromise()`.
  uint64_t id;
  ResultCallback callback;
};

CustomFormControlScriptExecutor::CustomFormControlScriptExecutor(
    content::RenderFrame* render_frame,
    int32_t world_id)
    : content::RenderFrameObserver(render_frame), world_id_(world_id) {
  CHECK_GT(world_id_, blink::kMainDOMWorldId);
  CHECK(!blink::IsEqualOrExceedEmbedderWorldIdLimit(world_id_));
  // Before any script runs in the world.
  EnsureIsolatedWorldSetUp(world_id_);
}

CustomFormControlScriptExecutor::~CustomFormControlScriptExecutor() {
  CHECK(!pending_call_);  // OnDestruct() runs the pending call.
}

bool CustomFormControlScriptExecutor::LoadScript(std::string_view script) {
  manager_.Reset();
  // The promise that `pending_call_` awaits belongs to the dropped manager, so
  // the call fails.
  if (pending_call_) {
    RunPendingCallback(GetIsolate(render_frame()), {}, {});
  }
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
  // `RenderFrame`.
  v8::Local<v8::Value> manager =
      frame->ExecuteScriptInIsolatedWorldAndReturnValue(
          world_id_,
          blink::WebScriptSource(
              blink::WebString::FromUtf8(WrapScript(script))),
          blink::BackForwardCacheAware::kAllow);
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

std::vector<CustomFormControl>
CustomFormControlScriptExecutor::DetectCustomFormControls() {
  std::vector<CustomFormControl> controls;
  // TODO(crbug.com/581337039): Limit the number of custom form controls.
  CallFunction(MayReturnPromise(false), "detectCustomFormControls", {},
               base::BindOnce(
                   [](std::vector<CustomFormControl>& controls,
                      v8::Isolate* isolate, v8::Local<v8::Context> context,
                      v8::MaybeLocal<v8::Value> maybe_result) {
                     v8::Local<v8::Value> result;
                     if (!maybe_result.ToLocal(&result) || !result->IsArray()) {
                       return;
                     }
                     v8::Local<v8::Array> array = result.As<v8::Array>();
                     const uint32_t length = array->Length();
                     for (uint32_t i = 0; i < length; ++i) {
                       v8::TryCatch try_catch(isolate);
                       v8::Local<v8::Value> item;
                       if (!array->Get(context, i).ToLocal(&item)) {
                         continue;
                       }
                       std::optional<CustomFormControl> control =
                           ParseCustomFormControl(isolate, context, item);
                       if (control && !try_catch.HasCaught()) {
                         controls.push_back(*std::move(control));
                       }
                     }
                   },
                   std::ref(controls)));
  // Validate `canonical_dom_node` after all V8 property accesses finish, as
  // reading properties on subsequent objects in the V8 array could execute JS
  // getters that mutate the DOM or detach the frame.
  std::erase_if(controls, [this](const CustomFormControl& control) {
    const blink::WebElement& node = control.canonical_dom_node;
    return !form_util::IsAccessible(node) ||
           node.GetDocument() != GetDocument(render_frame()) ||
           node.DynamicTo<blink::WebFormElement>() ||
           form_util::IsAutofillableElement(
               node.DynamicTo<blink::WebFormControlElement>());
  });
  return controls;
}

// static
v8::MaybeLocal<v8::Value> CustomFormControlScriptExecutor::ToV8Value(
    v8::Isolate* isolate,
    Argument& argument) {
  // `std::visit()` fails to compile if `Argument` gains a type that isn't
  // handled here.
  return std::visit(
      absl::Overload{
          // `blink::WebNode::ToV8Value()` isn't const.
          [isolate](blink::WebNode& node) -> v8::MaybeLocal<v8::Value> {
            if (node.IsNull()) {
              return {};
            }
            return node.ToV8Value(isolate);
          },
          [isolate](const std::optional<std::u16string>& string)
              -> v8::MaybeLocal<v8::Value> {
            if (!string) {
              return v8::Undefined(isolate);
            }
            // Fails if the string is too long for V8.
            return v8::String::NewFromTwoByte(
                isolate, reinterpret_cast<const uint16_t*>(string->data()),
                v8::NewStringType::kNormal,
                base::checked_cast<int>(string->size()));
          }},
      argument);
}

void CustomFormControlScriptExecutor::CallFunction(
    MayReturnPromise may_return_promise,
    std::string_view function_name,
    std::vector<Argument> arguments,
    ResultCallback callback) {
  if (!render_frame()) {
    std::move(callback).Run(nullptr, {}, {});
    return;
  }
  blink::WebLocalFrame* frame = render_frame()->GetWebFrame();
  v8::Isolate* isolate = frame->GetAgentGroupScheduler()->Isolate();
  // Fails while a previous call of this executor still waits for its promise.
  // `GetCallsAwaitingPromise()` has one such call for each executor (frame).
  if (pending_call_ || manager_.IsEmpty()) {
    std::move(callback).Run(isolate, {}, {});
    return;
  }
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context =
      frame->GetScriptContextFromWorldId(isolate, world_id_);
  if (context.IsEmpty()) {
    std::move(callback).Run(isolate, {}, {});
    return;
  }
  v8::Context::Scope context_scope(context);
  v8::Local<v8::Object> manager = manager_.Get(isolate);

  v8::LocalVector<v8::Value> argv(isolate);
  argv.reserve(arguments.size());
  for (Argument& argument : arguments) {
    v8::Local<v8::Value> value;
    if (!ToV8Value(isolate, argument).ToLocal(&value)) {
      std::move(callback).Run(isolate, {}, {});
      return;
    }
    argv.push_back(value);
  }
  v8::Local<v8::String> name;
  if (!v8::String::NewFromUtf8(isolate, function_name.data(),
                               v8::NewStringType::kNormal,
                               base::checked_cast<int>(function_name.size()))
           .ToLocal(&name)) {
    std::move(callback).Run(isolate, {}, {});
    return;
  }

  v8::Local<v8::Value> value;
  {
    // Catches exceptions of the call only, so that `callback` always runs
    // outside of it.
    v8::TryCatch try_catch(isolate);
    // Blink's microtask policy requires a scope around calls into script. The
    // microtasks queued by the call (e.g. promise reactions) run at the next
    // microtask checkpoint, e.g. at the end of the current task, after
    // `callback`.
    v8::MicrotasksScope microtasks(context,
                                   v8::MicrotasksScope::kDoNotRunMicrotasks);
    // `value` stays empty if looking up the method (which may run a getter)
    // throws, if the method is missing or isn't a function, or if calling it
    // throws.
    v8::Local<v8::Value> function;
    if (manager->Get(context, name).ToLocal(&function) &&
        function->IsFunction() &&
        !function.As<v8::Function>()
             ->Call(context, manager, static_cast<int>(argv.size()),
                    argv.data())
             .ToLocal(&value)) {
      // V8 returns an empty handle if and only if an exception was thrown (or
      // script was terminated).
      DCHECK(try_catch.HasCaught());
    }
  }
  // Don't pass on the result of a call that failed, or a value of a context
  // that was released during the call (which drops the manager).
  if (value.IsEmpty() || manager_.IsEmpty()) {
    std::move(callback).Run(isolate, {}, {});
    return;
  }
  if (value->IsPromise() && !may_return_promise) {
    // Nothing waits for the promise, so don't report its rejection as
    // unhandled (e.g. in the console).
    value.As<v8::Promise>()->MarkAsHandled();
    std::move(callback).Run(isolate, {}, {});
    return;
  }
  if (value->IsPromise()) {
    SetCallbackForPromise(isolate, context, value.As<v8::Promise>(),
                          std::move(callback));
    return;
  }
  std::move(callback).Run(isolate, context, value);
}

void CustomFormControlScriptExecutor::SetCallbackForPromise(
    v8::Isolate* isolate,
    v8::Local<v8::Context> context,
    v8::Local<v8::Promise> promise,
    ResultCallback callback) {
  if (promise->State() != v8::Promise::kPending) {
    // The promise may already be settled, e.g. if an `async` method returned
    // without awaiting anything. A rejection is handled here, so don't report
    // it as unhandled (e.g. in the console).
    promise->MarkAsHandled();
    const bool fulfilled = promise->State() == v8::Promise::kFulfilled;
    std::move(callback).Run(
        isolate, fulfilled ? context : v8::Local<v8::Context>(),
        fulfilled ? promise->Result() : v8::MaybeLocal<v8::Value>());
    return;
  }
  const uint64_t call_id = g_next_call_id++;
  // A `Number` represents the id exactly, since it is below 2^53.
  CHECK_LT(call_id, uint64_t{1} << 53);
  v8::Local<v8::Value> data =
      v8::Number::New(isolate, static_cast<double>(call_id));
  v8::Local<v8::Function> on_fulfilled;
  v8::Local<v8::Function> on_rejected;
  if (!v8::Function::New(context, &OnPromiseFulfilled, data)
           .ToLocal(&on_fulfilled) ||
      !v8::Function::New(context, &OnPromiseRejected, data)
           .ToLocal(&on_rejected) ||
      promise->Then(context, on_fulfilled, on_rejected).IsEmpty()) {
    std::move(callback).Run(isolate, {}, {});
    return;
  }
  pending_call_ = std::make_unique<PendingCall>(call_id, std::move(callback));
  GetCallsAwaitingPromise().emplace(call_id, *this);
}

// static
void CustomFormControlScriptExecutor::OnPromiseFulfilled(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  if (CustomFormControlScriptExecutor* executor = GetWaitingExecutor(info)) {
    // The reactions run in the isolated world's context.
    executor->RunPendingCallback(
        info.GetIsolate(), info.GetIsolate()->GetCurrentContext(), info[0]);
  }
}

// static
void CustomFormControlScriptExecutor::OnPromiseRejected(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  if (CustomFormControlScriptExecutor* executor = GetWaitingExecutor(info)) {
    executor->RunPendingCallback(info.GetIsolate(), {}, {});
  }
}

void CustomFormControlScriptExecutor::RunPendingCallback(
    v8::Isolate* isolate,
    v8::Local<v8::Context> context,
    v8::MaybeLocal<v8::Value> value) {
  GetCallsAwaitingPromise().erase(pending_call_->id);
  // Reset before running the callback, which may make another call.
  ResultCallback callback = std::move(pending_call_->callback);
  pending_call_.reset();
  std::move(callback).Run(isolate, context, value);
}

void CustomFormControlScriptExecutor::ClosePopover(
    const blink::WebNode& node,
    base::OnceCallback<void(bool success)> callback) {
  CallFunction(MayReturnPromise(true), "closePopover", {node},
               base::BindOnce(
                   [](base::OnceCallback<void(bool success)> callback,
                      v8::Isolate* isolate, v8::Local<v8::Context> context,
                      v8::MaybeLocal<v8::Value> result) {
                     // `closePopover()` returns nothing, so only whether the
                     // call succeeded matters.
                     std::move(callback).Run(!result.IsEmpty());
                   },
                   std::move(callback)));
}

void CustomFormControlScriptExecutor::ExtractOptions(
    const blink::WebNode& node,
    base::OnceCallback<void(std::optional<CustomFormControlOptions>)>
        callback) {
  CallFunction(
      MayReturnPromise(true), "extractOptions", {node},
      base::BindOnce(
          [](base::OnceCallback<void(std::optional<CustomFormControlOptions>)>
                 callback,
             v8::Isolate* isolate, v8::Local<v8::Context> context,
             v8::MaybeLocal<v8::Value> maybe_result) {
            std::optional<CustomFormControlOptions> options;
            v8::Local<v8::Value> result;
            if (maybe_result.ToLocal(&result)) {
              options = ParseCustomFormControlOptions(isolate, context, result);
            }
            std::move(callback).Run(std::move(options));
          },
          std::move(callback)));
}

void CustomFormControlScriptExecutor::SetValue(
    const blink::WebNode& node,
    SelectOption option,
    base::OnceCallback<void(CustomFormControlSetValueResult)> callback) {
  CallFunction(
      MayReturnPromise(true), "setValue",
      {node, std::move(option.value), std::move(option.text)},
      base::BindOnce(
          [](base::OnceCallback<void(CustomFormControlSetValueResult)> callback,
             v8::Isolate* isolate, v8::Local<v8::Context> context,
             v8::MaybeLocal<v8::Value> maybe_result) {
            v8::Local<v8::Value> result;
            std::move(callback).Run(
                maybe_result.ToLocal(&result)
                    ? ParseSetValueResult(isolate, context, result)
                    : CustomFormControlSetValueResult::kExecutionError);
          },
          std::move(callback)));
}

void CustomFormControlScriptExecutor::WillReleaseScriptContext(
    v8::Local<v8::Context> context,
    int32_t world_id) {
  if (world_id == world_id_) {
    manager_.Reset();
    // The promise of a pending call can no longer settle.
    if (pending_call_) {
      RunPendingCallback(GetIsolate(render_frame()), {}, {});
    }
  }
}

void CustomFormControlScriptExecutor::DidCommitProvisionalLoad(
    ui::PageTransition transition) {
  manager_.Reset();
  if (pending_call_) {
    RunPendingCallback(GetIsolate(render_frame()), {}, {});
  }
}

void CustomFormControlScriptExecutor::OnDestruct() {
  manager_.Reset();
  if (pending_call_) {
    RunPendingCallback(nullptr, {}, {});
  }
}

}  // namespace autofill
