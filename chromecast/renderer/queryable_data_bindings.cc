// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/renderer/queryable_data_bindings.h"

#include "base/bind_helpers.h"
#include "base/logging.h"
#include "chromecast/common/queryable_data.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/v8_value_converter.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/web/blink.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "v8/include/v8.h"

namespace chromecast {

namespace {

const char kQueryPlatformValueMethodName[] = "queryPlatformValue";

}  // namespace

QueryableDataBindings::QueryableDataBindings(content::RenderFrame* frame)
    : CastBinding(frame),
      queryable_data_store_(std::make_unique<QueryableDataStore>(
          base::ThreadTaskRunnerHandle::Get())) {
  registry_.AddInterface<shell::mojom::QueryableDataStore>(
      base::BindRepeating(&QueryableDataStore::BindQueryableDataStoreReceiver,
                          base::Unretained(queryable_data_store_.get())));
}

QueryableDataBindings::~QueryableDataBindings() {}

void QueryableDataBindings::Install(v8::Local<v8::Object> cast_platform,
                                    v8::Isolate* isolate) {
  VLOG(1) << "Installing QueryableDataBindings";

  InstallBinding(isolate, cast_platform, kQueryPlatformValueMethodName,
                 &QueryableDataBindings::QueryPlatformValue,
                 base::Unretained(this));
}

void QueryableDataBindings::OnInterfaceRequestForFrame(
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle* interface_pipe) {
  registry_.TryBindInterface(interface_name, interface_pipe);
}

v8::Local<v8::Value> QueryableDataBindings::QueryPlatformValue(
    const std::string& query_key) {
  VLOG(1) << __FUNCTION__ << ": " << query_key;

  v8::Isolate* isolate = blink::MainThreadIsolate();

  const base::Value* query_value = QueryableData::Query(query_key);
  if (!query_value)
    return v8::Local<v8::Value>(v8::Undefined(isolate));

  return content::V8ValueConverter::Create()->ToV8Value(
      query_value, render_frame()->GetWebFrame()->MainWorldScriptContext());
}

}  // namespace chromecast
