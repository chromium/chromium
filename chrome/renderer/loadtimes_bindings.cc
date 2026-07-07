// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/loadtimes_bindings.h"

#include <string_view>

#include "base/time/time.h"
#include "content/public/renderer/chrome_object_extensions_utils.h"
#include "content/public/renderer/render_frame.h"
#include "gin/object_template_builder.h"
#include "net/http/http_connection_info.h"
#include "third_party/blink/public/platform/scheduler/web_agent_group_scheduler.h"
#include "third_party/blink/public/platform/web_url_response.h"
#include "third_party/blink/public/web/web_document_loader.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_navigation_type.h"
#include "third_party/blink/public/web/web_performance_metrics_for_reporting.h"
#include "v8/include/cppgc/allocation.h"
#include "v8/include/v8-context.h"
#include "v8/include/v8-function.h"
#include "v8/include/v8-isolate.h"
#include "v8/include/v8-microtask-queue.h"
#include "v8/include/v8-object.h"
#include "v8/include/v8-primitive.h"
#include "v8/include/v8-template.h"

namespace {

// Values for CSI "tran" property
constexpr int kTransitionLink = 0;
constexpr int kTransitionForwardBack = 6;
constexpr int kTransitionOther = 15;
constexpr int kTransitionReload = 16;

constexpr std::string_view GetNavigationType(
    blink::WebNavigationType nav_type) {
  switch (nav_type) {
    case blink::kWebNavigationTypeLinkClicked:
      return "LinkClicked";
    case blink::kWebNavigationTypeFormSubmitted:
      return "FormSubmitted";
    case blink::kWebNavigationTypeBackForward:
    case blink::kWebNavigationTypeRestore:
      return "BackForward";
    case blink::kWebNavigationTypeReload:
      return "Reload";
    case blink::kWebNavigationTypeFormResubmittedBackForward:
    case blink::kWebNavigationTypeFormResubmittedReload:
      return "Resubmitted";
    case blink::kWebNavigationTypeOther:
      return "Other";
  }
  return "";
}

int GetCSITransitionType(blink::WebNavigationType nav_type) {
  switch (nav_type) {
    case blink::kWebNavigationTypeLinkClicked:
    case blink::kWebNavigationTypeFormSubmitted:
    case blink::kWebNavigationTypeFormResubmittedBackForward:
    case blink::kWebNavigationTypeFormResubmittedReload:
      return kTransitionLink;
    case blink::kWebNavigationTypeBackForward:
    case blink::kWebNavigationTypeRestore:
      return kTransitionForwardBack;
    case blink::kWebNavigationTypeReload:
      return kTransitionReload;
    case blink::kWebNavigationTypeOther:
      return kTransitionOther;
  }
  return kTransitionOther;
}

void EmptySetter(v8::Local<v8::Name> name,
                 v8::Local<v8::Value> value,
                 const v8::PropertyCallbackInfo<v8::Boolean>& info) {
  // Empty setter is required to keep the native data property in "accessor"
  // state even in case the value is updated by user code.
}

void LoadtimesGetter(v8::Local<v8::Name> name,
                     const v8::PropertyCallbackInfo<v8::Value>& info) {
  if (blink::WebLocalFrame* frame =
          blink::WebLocalFrame::FrameForCurrentContext()) {
    frame->UsageCountChromeLoadTimes(blink::WebString::FromUtf8(
        *v8::String::Utf8Value(info.GetIsolate(), name)));
  }
  info.GetReturnValue().Set(info.Data());
}

void CSIGetter(v8::Local<v8::Name> name,
               const v8::PropertyCallbackInfo<v8::Value>& info) {
  if (blink::WebLocalFrame* frame =
          blink::WebLocalFrame::FrameForCurrentContext()) {
    frame->UsageCountChromeCSI(blink::WebString::FromUtf8(
        *v8::String::Utf8Value(info.GetIsolate(), name)));
  }
  info.GetReturnValue().Set(info.Data());
}

}  // namespace

// static
void LoadTimesBindings::LoadTimesCallback(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  LoadTimesBindings* bindings = nullptr;
  if (gin::ConvertFromV8(isolate, info.Data(), &bindings) && bindings) {
    info.GetReturnValue().Set(bindings->GetLoadTimes(isolate));
  }
}

// static
void LoadTimesBindings::CSICallback(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  LoadTimesBindings* bindings = nullptr;
  if (gin::ConvertFromV8(isolate, info.Data(), &bindings) && bindings) {
    info.GetReturnValue().Set(bindings->GetCSI(isolate));
  }
}

// static
void LoadTimesBindings::Install(v8::Local<v8::Context> context) {
  v8::Isolate* isolate = v8::Isolate::GetCurrent();

  DCHECK(isolate);

  v8::HandleScope handle_scope(isolate);
  if (context.IsEmpty()) {
    return;
  }

  v8::MicrotasksScope microtasks_scope(
      isolate, context->GetMicrotaskQueue(),
      v8::MicrotasksScope::kDoNotRunMicrotasks);
  v8::Context::Scope context_scope(context);

  auto* bindings = cppgc::MakeGarbageCollected<LoadTimesBindings>(
      isolate->GetCppHeap()->GetAllocationHandle());
  v8::Local<v8::Object> wrapper;
  if (!bindings->GetWrapper(isolate).ToLocal(&wrapper)) {
    return;
  }

  v8::Local<v8::Object> chrome =
      content::GetOrCreateChromeObject(isolate, context);

  v8::Local<v8::Function> load_times_func;
  if (v8::Function::New(context, LoadTimesCallback, wrapper)
          .ToLocal(&load_times_func)) {
    chrome->Set(context, gin::StringToV8(isolate, "loadTimes"), load_times_func)
        .Check();
  }

  v8::Local<v8::Function> csi_func;
  if (v8::Function::New(context, CSICallback, wrapper).ToLocal(&csi_func)) {
    chrome->Set(context, gin::StringToV8(isolate, "csi"), csi_func).Check();
  }
}

LoadTimesBindings::LoadTimesBindings() = default;

LoadTimesBindings::~LoadTimesBindings() = default;

gin::ObjectTemplateBuilder LoadTimesBindings::GetObjectTemplateBuilder(
    v8::Isolate* isolate) {
  return gin::Wrappable<LoadTimesBindings>::GetObjectTemplateBuilder(isolate);
}

v8::Local<v8::Value> LoadTimesBindings::GetLoadTimes(v8::Isolate* isolate) {
  blink::WebLocalFrame* frame = blink::WebLocalFrame::FrameForCurrentContext();
  if (!frame) {
    return v8::Null(isolate);
  }
  blink::WebDocumentLoader* document_loader = frame->GetDocumentLoader();
  if (!document_loader) {
    return v8::Null(isolate);
  }
  const blink::WebURLResponse& response = document_loader->GetWebResponse();
  blink::WebPerformanceMetricsForReporting web_performance =
      frame->PerformanceMetricsForReporting();

  // Though request time now tends to be used to describe the time that the
  // request for the main resource was issued, when chrome.loadTimes() was
  // added, it was used to describe 'The time the request to load the page was
  // received', which is the time now known as navigation start. For backward
  // compatibility, we continue to provide request_time, setting its value to
  // navigation start.
  double request_time = web_performance.NavigationStart();
  // Developers often use start_load_time as the time the navigation was
  // started, so we return navigationStart for this value as well. See
  // https://gist.github.com/search?utf8=%E2%9C%93&q=startLoadTime.
  // Note that, historically, start_load_time reported the time that a
  // provisional load was first processed in the render process. For
  // browser-initiated navigations, this is some time after navigation start,
  // which means that developers who used this value as a way to track the
  // start of a navigation were misusing this timestamp and getting the wrong
  // value - they should be using navigationStart instead. Provisional loads
  // will not be processed by the render process for browser-initiated
  // navigations, so reporting the time a provisional load was processed in
  // the render process will no longer make sense. Thus, we now report the
  // time for navigationStart, which is a value more consistent with what
  // developers currently use start_load_time for.
  double start_load_time = web_performance.NavigationStart();
  // TODO(bmcquade): Remove this. 'commit' time is a concept internal to
  // chrome that shouldn't be exposed to the web platform.
  double commit_load_time = web_performance.ResponseStart();
  double finish_document_load_time = web_performance.DomContentLoadedEventEnd();
  double finish_load_time = web_performance.LoadEventEnd();
  double first_paint_time = web_performance.FirstPaint();
  // TODO(bmcquade): remove this. It's misleading to track the first paint
  // after the load event, since many pages perform their meaningful paints
  // long before the load event fires. We report a time of zero for the
  // time being.
  double first_paint_after_load_time = 0.0;
  std::string_view navigation_type =
      GetNavigationType(document_loader->GetNavigationType());
  bool was_fetched_via_spdy = response.WasFetchedViaSPDY();
  bool was_alpn_negotiated = response.WasAlpnNegotiated();
  std::string alpn_negotiated_protocol =
      response.AlpnNegotiatedProtocol().Utf8();
  bool was_alternate_protocol_available =
      response.WasAlternateProtocolAvailable();
  std::string_view connection_info =
      net::HttpConnectionInfoToString(response.ConnectionInfo());

  // Important: |frame| and |document_loader| should not be referred to below
  // this line, as JS setters below can invalidate these pointers.
  v8::Local<v8::Context> ctx = isolate->GetCurrentContext();
  v8::Local<v8::Object> load_times = v8::Object::New(isolate);

  auto v8_num = [=](double value) { return v8::Number::New(isolate, value); };
  auto v8_bool = [=](bool value) { return v8::Boolean::New(isolate, value); };
  auto v8_str = [=](std::string_view str) {
    return v8::String::NewFromUtf8(isolate, str.data(),
                                   v8::NewStringType::kNormal, str.length())
        .ToLocalChecked();
  };

  auto define_prop = [=](std::string_view name, v8::Local<v8::Value> value) {
    v8::Local<v8::String> name_str =
        v8::String::NewFromUtf8(isolate, name.data(),
                                v8::NewStringType::kInternalized, name.length())
            .ToLocalChecked();

    return load_times
        ->SetNativeDataProperty(ctx, name_str, LoadtimesGetter, EmptySetter,
                                value)
        .FromMaybe(false);
  };

  if (!define_prop("requestTime", v8_num(request_time)) ||
      !define_prop("startLoadTime", v8_num(start_load_time)) ||
      !define_prop("commitLoadTime", v8_num(commit_load_time)) ||
      !define_prop("finishDocumentLoadTime",
                   v8_num(finish_document_load_time)) ||
      !define_prop("finishLoadTime", v8_num(finish_load_time)) ||
      !define_prop("firstPaintTime", v8_num(first_paint_time)) ||
      !define_prop("firstPaintAfterLoadTime",
                   v8_num(first_paint_after_load_time)) ||
      !define_prop("navigationType", v8_str(navigation_type)) ||
      !define_prop("wasFetchedViaSpdy", v8_bool(was_fetched_via_spdy)) ||
      !define_prop("wasNpnNegotiated", v8_bool(was_alpn_negotiated)) ||
      !define_prop("npnNegotiatedProtocol", v8_str(alpn_negotiated_protocol)) ||
      !define_prop("wasAlternateProtocolAvailable",
                   v8_bool(was_alternate_protocol_available)) ||
      !define_prop("connectionInfo", v8_str(connection_info))) {
    return v8::Null(isolate);
  }

  return load_times;
}

v8::Local<v8::Value> LoadTimesBindings::GetCSI(v8::Isolate* isolate) {
  blink::WebLocalFrame* frame = blink::WebLocalFrame::FrameForCurrentContext();
  if (!frame) {
    return v8::Null(isolate);
  }
  blink::WebDocumentLoader* document_loader = frame->GetDocumentLoader();
  if (!document_loader) {
    return v8::Null(isolate);
  }
  blink::WebPerformanceMetricsForReporting web_performance =
      frame->PerformanceMetricsForReporting();
  base::Time now = base::Time::Now();
  base::Time start =
      base::Time::FromSecondsSinceUnixEpoch(web_performance.NavigationStart());

  base::Time dom_content_loaded_end = base::Time::FromSecondsSinceUnixEpoch(
      web_performance.DomContentLoadedEventEnd());
  base::TimeDelta page = now - start;
  int navigation_type =
      GetCSITransitionType(document_loader->GetNavigationType());

  // Important: |frame| and |document_loader| should not be referred to below
  // this line, as JS setters below can invalidate these pointers.
  v8::Local<v8::Context> ctx = isolate->GetCurrentContext();
  v8::Local<v8::Object> csi = v8::Object::New(isolate);

  auto v8_num = [=](double value) { return v8::Number::New(isolate, value); };

  auto define_prop = [=](std::string_view name, v8::Local<v8::Value> value) {
    v8::Local<v8::String> name_str =
        v8::String::NewFromUtf8(isolate, name.data(),
                                v8::NewStringType::kInternalized, name.length())
            .ToLocalChecked();

    return csi
        ->SetNativeDataProperty(ctx, name_str, CSIGetter, EmptySetter, value)
        .FromMaybe(false);
  };

  if (!define_prop("startE", v8_num(start.InMillisecondsSinceUnixEpoch())) ||
      // NOTE: historically, the CSI onload field has reported the time the
      // document finishes parsing, which is DOMContentLoaded. Thus, we continue
      // to report that here, despite the fact that the field is named onloadT.
      !define_prop(
          "onloadT",
          v8_num(dom_content_loaded_end.InMillisecondsSinceUnixEpoch())) ||
      !define_prop("pageT", v8_num(page.InMillisecondsF())) ||
      !define_prop("tran", v8_num(navigation_type))) {
    return v8::Null(isolate);
  }
  return csi;
}

const gin::WrapperInfo* LoadTimesBindings::wrapper_info() const {
  return &kWrapperInfo;
}
