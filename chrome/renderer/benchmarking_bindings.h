// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_BENCHMARKING_BINDINGS_H_
#define CHROME_RENDERER_BENCHMARKING_BINDINGS_H_

#include <cstdint>

#include "chrome/common/net_benchmarking.mojom.h"
#include "gin/wrappable.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "v8/include/v8-forward.h"

class BenchmarkingBindings final : public gin::Wrappable<BenchmarkingBindings> {
 public:
  static constexpr gin::WrapperInfo kWrapperInfo = {{gin::kEmbedderNativeGin},
                                                    gin::kBenchmarkingBindings};

  BenchmarkingBindings(const BenchmarkingBindings&) = delete;
  BenchmarkingBindings& operator=(const BenchmarkingBindings&) = delete;

  static void InstallConditionally(v8::Local<v8::Context> context);

  BenchmarkingBindings();
  ~BenchmarkingBindings() override;

  bool IsSingleProcess();
  int GetRendererPid();
  int32_t GetRendererMainTid();
  double HiResTime();
  v8::Local<v8::Object> GetMarkFunctions(v8::Isolate* isolate);

  void ClearCache();
  void ClearHostResolverCache();
  void ClearPredictorCache();
  void CloseConnections();

 private:
  chrome::mojom::NetBenchmarking* GetNetBenchmarking();

  // gin::WrappableBase
  const gin::WrapperInfo* wrapper_info() const override;

  gin::ObjectTemplateBuilder GetObjectTemplateBuilder(
      v8::Isolate* isolate) override;

  mojo::Remote<chrome::mojom::NetBenchmarking> net_benchmarking_;
};

#endif  // CHROME_RENDERER_BENCHMARKING_BINDINGS_H_
