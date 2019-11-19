// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metal_util/test_shader.h"

#import <Metal/Metal.h>

#include "base/bind.h"
#include "base/debug/dump_without_crashing.h"
#include "base/mac/scoped_nsobject.h"
#include "base/memory/ref_counted.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/lock.h"
#include "base/task/post_task.h"
#include "components/metal_util/device.h"

namespace metal {

namespace {

const char* kTestShaderSource =
    ""
    "#include <metal_stdlib>\n"
    "#include <simd/simd.h>\n"
    "typedef struct {\n"
    "    float4 clipSpacePosition [[position]];\n"
    "    float4 color;\n"
    "} RasterizerData;\n"
    "\n"
    "vertex RasterizerData vertexShader(\n"
    "    uint vertexID [[vertex_id]],\n"
    "    constant vector_float2 *positions[[buffer(0)]],\n"
    "    constant vector_float4 *colors[[buffer(1)]]) {\n"
    "  RasterizerData out;\n"
    "  out.clipSpacePosition = vector_float4(0.0, 0.0, 0.0, 1.0);\n"
    "  out.clipSpacePosition.xy = positions[vertexID].xy;\n"
    "  out.color = colors[vertexID];\n"
    "  return out;\n"
    "}\n"
    "\n"
    "fragment float4 fragmentShader(RasterizerData in [[stage_in]]) {\n"
    "    return %f * in.color;\n"
    "}\n"
    "";

// State shared between the compiler callback and the caller.
class API_AVAILABLE(macos(10.11)) TestShaderState
    : public base::RefCountedThreadSafe<TestShaderState> {
 public:
  TestShaderState(TestShaderCallback callback)
      : callback_(std::move(callback)) {}
  void RunCallback(TestShaderResult result) {
    TestShaderCallback callback;
    {
      base::AutoLock lock(lock_);
      callback = std::move(callback_);
    }
    if (callback)
      std::move(callback).Run(result);
  }

 protected:
  base::Lock lock_;
  TestShaderCallback callback_;
  friend class base::RefCountedThreadSafe<TestShaderState>;
  virtual ~TestShaderState() {}
};

}  // namespace

void TestShader(float seed,
                TestShaderCallback callback,
                const base::TimeDelta& timeout) {
  if (@available(macOS 10.11, *)) {
    base::scoped_nsprotocol<id<MTLDevice>> device(CreateDefaultDevice());
    if (device) {
      auto state = base::MakeRefCounted<TestShaderState>(std::move(callback));
      const std::string shader_source =
          base::StringPrintf(kTestShaderSource, seed);
      base::scoped_nsobject<NSString> source([[NSString alloc]
          initWithCString:shader_source.c_str()
                 encoding:NSASCIIStringEncoding]);
      base::scoped_nsobject<MTLCompileOptions> options(
          [[MTLCompileOptions alloc] init]);
      MTLNewLibraryCompletionHandler completion_handler =
          ^(id<MTLLibrary> library, NSError* error) {
            if (library)
              state->RunCallback(TestShaderResult::kSucceeded);
            else
              state->RunCallback(TestShaderResult::kFailed);
          };
      [device newLibraryWithSource:source
                           options:options
                 completionHandler:completion_handler];
      base::PostDelayedTask(FROM_HERE, {base::ThreadPool()},
                            base::BindOnce(&TestShaderState::RunCallback, state,
                                           TestShaderResult::kTimedOut),
                            timeout);
      return;
    }
  }
  std::move(callback).Run(TestShaderResult::kNotAttempted);
}

}  // namespace metal
