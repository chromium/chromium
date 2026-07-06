// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_BENCHMARKING_BINDINGS_H_
#define CHROME_RENDERER_BENCHMARKING_BINDINGS_H_

#include <cstdint>

#include "gin/wrappable.h"
#include "v8/include/v8-forward.h"

class BenchmarkingBindings final : public gin::Wrappable<BenchmarkingBindings> {
 public:
  static constexpr gin::WrapperInfo kWrapperInfo = {{gin::kEmbedderNativeGin},
                                                    gin::kBenchmarkingBindings};

  BenchmarkingBindings(const BenchmarkingBindings&) = delete;
  BenchmarkingBindings& operator=(const BenchmarkingBindings&) = delete;

  static void Install(v8::Local<v8::Context> context);

  BenchmarkingBindings();
  ~BenchmarkingBindings() override;

  bool IsSingleProcess();
  int GetRendererPid();
  int32_t GetRendererMainTid();
  double HiResTime();
  v8::Local<v8::Object> GetMarkFunctions(v8::Isolate* isolate);

 private:
  // gin::WrappableBase
  const gin::WrapperInfo* wrapper_info() const override;

  gin::ObjectTemplateBuilder GetObjectTemplateBuilder(
      v8::Isolate* isolate) override;
};

#endif  // CHROME_RENDERER_BENCHMARKING_BINDINGS_H_
