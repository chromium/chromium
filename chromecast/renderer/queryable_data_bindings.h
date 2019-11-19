// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_RENDERER_QUERYABLE_DATA_BINDINGS_H_
#define CHROMECAST_RENDERER_QUERYABLE_DATA_BINDINGS_H_

#include <string>

#include "base/macros.h"
#include "chromecast/renderer/native_bindings_helper.h"
#include "chromecast/renderer/queryable_data_store.h"
#include "content/public/renderer/render_frame_observer.h"
#include "services/service_manager/public/cpp/binder_registry.h"

namespace chromecast {

class QueryableDataBindings : public CastBinding {
 public:
  explicit QueryableDataBindings(content::RenderFrame* frame);

  // content::RenderFrameObserver implementation:
  void OnInterfaceRequestForFrame(
      const std::string& interface_name,
      mojo::ScopedMessagePipeHandle* interface_pipe) override;

 private:
  friend class CastBinding;

  ~QueryableDataBindings() override;

  // CastBinding implementation:
  void Install(v8::Local<v8::Object> cast_platform,
               v8::Isolate* isolate) override;

  // Binding methods
  v8::Local<v8::Value> QueryPlatformValue(const std::string& query_key);

  service_manager::BinderRegistry registry_;
  const std::unique_ptr<QueryableDataStore> queryable_data_store_;

  DISALLOW_COPY_AND_ASSIGN(QueryableDataBindings);
};

}  // namespace chromecast

#endif  // CHROMECAST_RENDERER_QUERYABLE_DATA_BINDINGS_H_
