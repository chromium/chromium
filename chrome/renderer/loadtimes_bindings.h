// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_LOADTIMES_BINDINGS_H_
#define CHROME_RENDERER_LOADTIMES_BINDINGS_H_

#include "gin/wrappable.h"
#include "v8/include/v8-forward.h"

// LoadTimesBindings installs the chrome.loadTimes() and chrome.csi()
// functions on the chrome object.
//
// chrome.loadTimes() returns an object containing the following members:
// requestTime: The time the request to load the page was received.
// startLoadTime: The time the renderer started the load process.
// commitLoadTime: The time the page commit occurred.
// finishDocumentLoadTime: The time the document itself was loaded
//                         (this is before the onload() method is fired).
// finishLoadTime: The time all loading is done, after the onload()
//                 method and all resources.
// firstPaintTime: The time of the first paint.
// firstPaintAfterLoadTime: Historically tracked first paint after load.
// navigationType: The type of navigation.
// wasFetchedViaSpdy: Whether the page was fetched via SPDY.
// wasNpnNegotiated: Whether NPN was negotiated.
// npnNegotiatedProtocol: The negotiated protocol.
// wasAlternateProtocolAvailable: Whether an alternate protocol was available.
// connectionInfo: The connection info.
//
// chrome.csi() returns an object containing client-side instrumentation
// information:
// startE: The start time of the navigation.
// onloadT: The time the document finished parsing (DOMContentLoaded).
// pageT: The time elapsed since start.
// tran: The transition type.
class LoadTimesBindings : public gin::Wrappable<LoadTimesBindings> {
 public:
  static constexpr gin::WrapperInfo kWrapperInfo = {{gin::kEmbedderNativeGin},
                                                    gin::kLoadTimesBindings};

  LoadTimesBindings(const LoadTimesBindings&) = delete;
  LoadTimesBindings& operator=(const LoadTimesBindings&) = delete;

  static void Install(v8::Local<v8::Context> context);

  LoadTimesBindings();
  ~LoadTimesBindings() override;

 private:
  static void LoadTimesCallback(
      const v8::FunctionCallbackInfo<v8::Value>& info);
  static void CSICallback(const v8::FunctionCallbackInfo<v8::Value>& info);

  // gin::WrappableBase
  const gin::WrapperInfo* wrapper_info() const override;

  gin::ObjectTemplateBuilder GetObjectTemplateBuilder(
      v8::Isolate* isolate) override;

  v8::Local<v8::Value> GetLoadTimes(v8::Isolate* isolate);
  v8::Local<v8::Value> GetCSI(v8::Isolate* isolate);
};

#endif  // CHROME_RENDERER_LOADTIMES_BINDINGS_H_
