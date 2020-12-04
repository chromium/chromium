// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/gpu/metal_api_proxy.h"

#include <objc/objc.h>

#include <map>
#include <string>

#include "base/debug/crash_logging.h"
#include "base/mac/foundation_util.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/synchronization/condition_variable.h"
#include "base/trace_event/trace_event.h"
#include "components/crash/core/common/crash_key.h"
#include "ui/gl/progress_reporter.h"

namespace {

// State shared between the caller of [MTLDevice newLibraryWithSource:] and its
// MTLNewLibraryCompletionHandler (and similarly for -[MTLDevice
// newRenderPipelineStateWithDescriptor:]. The completion handler may be called
// on another thread, so all members are protected by a lock. Accessed via
// scoped_refptr to ensure that it exists until its last accessor is gone.
class AsyncMetalState : public base::RefCountedThreadSafe<AsyncMetalState> {
 public:
  AsyncMetalState() : condition_variable(&lock) {}

  // All members may only be accessed while |lock| is held.
  base::Lock lock;
  base::ConditionVariable condition_variable;

  // Set to true when the completion handler is called.
  bool has_result = false;

  // The results of the async operation. These are set only by the first
  // completion handler to run.
  id<MTLLibrary> library = nil;
  id<MTLRenderPipelineState> render_pipeline_state = nil;
  NSError* error = nil;

 private:
  friend class base::RefCountedThreadSafe<AsyncMetalState>;
  ~AsyncMetalState() { DCHECK(has_result); }
};

id<MTLLibrary> NewLibraryWithRetry(id<MTLDevice> device,
                                   NSString* source,
                                   MTLCompileOptions* options,
                                   __autoreleasing NSError** error,
                                   gl::ProgressReporter* progress_reporter) {
  SCOPED_UMA_HISTOGRAM_TIMER("Gpu.MetalProxy.NewLibraryTime");
  const base::TimeTicks start_time = base::TimeTicks::Now();
  auto state = base::MakeRefCounted<AsyncMetalState>();

  // The completion handler will signal the condition variable we will wait
  // on. Note that completionHandler will hold a reference to |state|.
  MTLNewLibraryCompletionHandler completionHandler =
      ^(id<MTLLibrary> library, NSError* error) {
        base::AutoLock lock(state->lock);
        state->has_result = true;
        state->library = [library retain];
        state->error = [error retain];
        state->condition_variable.Signal();
      };

  // Request asynchronous compilation. Note that |completionHandler| may be
  // called from within this function call, or it may be called from a
  // different thread.
  if (progress_reporter)
    progress_reporter->ReportProgress();
  [device newLibraryWithSource:source
                       options:options
             completionHandler:completionHandler];

  // Suppress the watchdog timer for kTimeout by reporting progress every
  // half-second. After that, allow it to kill the the GPU process.
  constexpr base::TimeDelta kTimeout = base::TimeDelta::FromSeconds(60);
  constexpr base::TimeDelta kWaitPeriod =
      base::TimeDelta::FromMilliseconds(500);
  while (true) {
    if (base::TimeTicks::Now() - start_time < kTimeout && progress_reporter)
      progress_reporter->ReportProgress();
    base::AutoLock lock(state->lock);
    if (state->has_result) {
      *error = [state->error autorelease];
      return state->library;
    }
    state->condition_variable.TimedWait(kWaitPeriod);
  }
}

id<MTLRenderPipelineState> NewRenderPipelineStateWithRetry(
    id<MTLDevice> device,
    MTLRenderPipelineDescriptor* descriptor,
    __autoreleasing NSError** error,
    gl::ProgressReporter* progress_reporter) {
  // This function is almost-identical to the above NewLibraryWithRetry. See
  // comments in that function.
  SCOPED_UMA_HISTOGRAM_TIMER("Gpu.MetalProxy.NewRenderPipelineStateTime");
  const base::TimeTicks start_time = base::TimeTicks::Now();
  auto state = base::MakeRefCounted<AsyncMetalState>();
  MTLNewRenderPipelineStateCompletionHandler completionHandler =
      ^(id<MTLRenderPipelineState> render_pipeline_state, NSError* error) {
        base::AutoLock lock(state->lock);
        state->has_result = true;
        state->render_pipeline_state = [render_pipeline_state retain];
        state->error = [error retain];
        state->condition_variable.Signal();
      };
  if (progress_reporter)
    progress_reporter->ReportProgress();
  [device newRenderPipelineStateWithDescriptor:descriptor
                             completionHandler:completionHandler];
  constexpr base::TimeDelta kTimeout = base::TimeDelta::FromSeconds(60);
  constexpr base::TimeDelta kWaitPeriod =
      base::TimeDelta::FromMilliseconds(500);
  while (true) {
    if (base::TimeTicks::Now() - start_time < kTimeout && progress_reporter)
      progress_reporter->ReportProgress();
    base::AutoLock lock(state->lock);
    if (state->has_result) {
      *error = [state->error autorelease];
      return state->render_pipeline_state;
    }
    state->condition_variable.TimedWait(kWaitPeriod);
  }
}

// Maximum length of a shader to be uploaded with a crash report.
constexpr uint32_t kShaderCrashDumpLength = 8128;

}  // namespace

// A cache of the result of calls to NewLibraryWithRetry. This will store all
// resulting MTLLibraries indefinitely, and will grow without bound. This is to
// minimize the number of calls hitting the MTLCompilerService, which is prone
// to hangs. Should this significantly help the situation, a more robust (and
// not indefinitely-growing) cache will be added either here or in Skia.
// https://crbug.com/974219
class MTLLibraryCache {
 public:
  MTLLibraryCache() = default;
  ~MTLLibraryCache() = default;

  id<MTLLibrary> NewLibraryWithSource(id<MTLDevice> device,
                                      NSString* source,
                                      MTLCompileOptions* options,
                                      __autoreleasing NSError** error,
                                      gl::ProgressReporter* progress_reporter) {
    LibraryKey key(source, options);
    auto found = libraries_.find(key);
    if (found != libraries_.end()) {
      const LibraryData& data = found->second;
      *error = [[data.error retain] autorelease];
      return [data.library retain];
    }
    SCOPED_UMA_HISTOGRAM_TIMER("Gpu.MetalProxy.NewLibraryTime");
    id<MTLLibrary> library =
        NewLibraryWithRetry(device, source, options, error, progress_reporter);
    LibraryData data(library, *error);
    libraries_.insert(std::make_pair(key, std::move(data)));
    return library;
  }
  // The number of cache misses is the number of times that we have had to call
  // the true newLibraryWithSource function.
  uint64_t CacheMissCount() const { return libraries_.size(); }

 private:
  struct LibraryKey {
    LibraryKey(NSString* source, MTLCompileOptions* options)
        : source_(source, base::scoped_policy::RETAIN),
          options_(options, base::scoped_policy::RETAIN) {}
    LibraryKey(const LibraryKey& other) = default;
    LibraryKey& operator=(const LibraryKey& other) = default;
    ~LibraryKey() = default;

    bool operator<(const LibraryKey& other) const {
      switch ([source_ compare:other.source_]) {
        case NSOrderedAscending:
          return true;
        case NSOrderedDescending:
          return false;
        case NSOrderedSame:
          break;
      }
#define COMPARE(x)                       \
  if ([options_ x] < [other.options_ x]) \
    return true;                         \
  if ([options_ x] > [other.options_ x]) \
    return false;
      COMPARE(fastMathEnabled);
      COMPARE(languageVersion);
#undef COMPARE
      // Skia doesn't set any preprocessor macros, and defining an order on two
      // NSDictionaries is a lot of code, so just assert that there are no
      // macros. Should this alleviate https://crbug.com/974219, then a more
      // robust cache should be implemented.
      DCHECK_EQ([[options_ preprocessorMacros] count], 0u);
      return false;
    }

   private:
    base::scoped_nsobject<NSString> source_;
    base::scoped_nsobject<MTLCompileOptions> options_;
  };
  struct LibraryData {
    LibraryData(id<MTLLibrary> library_, NSError* error_)
        : library(library_, base::scoped_policy::RETAIN),
          error(error_, base::scoped_policy::RETAIN) {}
    LibraryData(const LibraryData& other) = default;
    LibraryData& operator=(const LibraryData& other) = default;
    ~LibraryData() = default;

    base::scoped_nsprotocol<id<MTLLibrary>> library;
    base::scoped_nsobject<NSError> error;
  };

  std::map<LibraryKey, LibraryData> libraries_;
  DISALLOW_COPY_AND_ASSIGN(MTLLibraryCache);
};

// Disable protocol warnings and property synthesis warnings. Any unimplemented
// methods/properties in the MTLDevice protocol will be handled by the
// -forwardInvocation: method.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wprotocol"
#pragma clang diagnostic ignored "-Wobjc-protocol-property-synthesis"

@implementation MTLDeviceProxy
- (id)initWithDevice:(id<MTLDevice>)device {
  if (self = [super init]) {
    _device.reset(device, base::scoped_policy::RETAIN);
    _libraryCache = std::make_unique<MTLLibraryCache>();
  }
  return self;
}

- (void)setProgressReporter:(gl::ProgressReporter*)progressReporter {
  _progressReporter = progressReporter;
}

- (NSMethodSignature*)methodSignatureForSelector:(SEL)selector {
  // Technically, _device is of protocol MTLDevice which inherits from protocol
  // NSObject, and protocol NSObject does not have -methodSignatureForSelector:.
  // Assume that the implementing class derives from NSObject.
  return [base::mac::ObjCCastStrict<NSObject>(_device)
      methodSignatureForSelector:selector];
}

- (void)forwardInvocation:(NSInvocation*)invocation {
  // The number of methods on MTLDevice is finite and small, so this unbounded
  // cache is fine. std::map does not move elements on additions to the map, so
  // the requirement that strings passed to TRACE_EVENT0 don't move is
  // fulfilled.
  static base::NoDestructor<std::map<SEL, std::string>> invocationNames;
  auto& invocationName = (*invocationNames)[invocation.selector];
  if (invocationName.empty()) {
    invocationName =
        base::StringPrintf("-[MTLDevice %s]", sel_getName(invocation.selector));
  }

  TRACE_EVENT0("gpu", invocationName.c_str());
  gl::ScopedProgressReporter scoped_reporter(_progressReporter);
  [invocation invokeWithTarget:_device.get()];
}

- (nullable id<MTLLibrary>)
    newLibraryWithSource:(NSString*)source
                 options:(nullable MTLCompileOptions*)options
                   error:(__autoreleasing NSError**)error {
  TRACE_EVENT0("gpu", "-[MTLDevice newLibraryWithSource:options:error:]");

  // Capture the shader's source in a crash key in case newLibraryWithSource
  // hangs.
  // https://crbug.com/974219
  static crash_reporter::CrashKeyString<kShaderCrashDumpLength> shaderKey(
      "MTLShaderSource");
  std::string sourceAsSysString = base::SysNSStringToUTF8(source);
  if (sourceAsSysString.size() > kShaderCrashDumpLength)
    DLOG(WARNING) << "Truncating shader in crash log.";

  shaderKey.Set(sourceAsSysString);
  static crash_reporter::CrashKeyString<16> newLibraryCountKey(
      "MTLNewLibraryCount");
  newLibraryCountKey.Set(base::NumberToString(_libraryCache->CacheMissCount()));

  id<MTLLibrary> library = _libraryCache->NewLibraryWithSource(
      _device, source, options, error, _progressReporter);
  shaderKey.Clear();
  newLibraryCountKey.Clear();

  // Shaders from Skia will have either a vertexMain or fragmentMain function.
  // Save the source and a weak pointer to the function, so we can capture
  // the shader source in -newRenderPipelineStateWithDescriptor (see further
  // remarks in that function).
  base::scoped_nsprotocol<id<MTLFunction>> vertexFunction(
      [library newFunctionWithName:@"vertexMain"]);
  if (vertexFunction) {
    _vertexSourceFunction = vertexFunction;
    _vertexSource = sourceAsSysString;
  }
  base::scoped_nsprotocol<id<MTLFunction>> fragmentFunction(
      [library newFunctionWithName:@"fragmentMain"]);
  if (fragmentFunction) {
    _fragmentSourceFunction = fragmentFunction;
    _fragmentSource = sourceAsSysString;
  }

  return library;
}

- (nullable id<MTLRenderPipelineState>)
    newRenderPipelineStateWithDescriptor:
        (MTLRenderPipelineDescriptor*)descriptor
                                   error:(__autoreleasing NSError**)error {
  TRACE_EVENT0("gpu",
               "-[MTLDevice newRenderPipelineStateWithDescriptor:error:]");
  // Capture the vertex and shader source being used. Skia's use pattern is to
  // compile two MTLLibraries before creating a MTLRenderPipelineState -- one
  // with vertexMain and the other with fragmentMain. The two immediately
  // previous -newLibraryWithSource calls should have saved the sources for
  // these two functions.
  // https://crbug.com/974219
  static crash_reporter::CrashKeyString<kShaderCrashDumpLength> vertexShaderKey(
      "MTLVertexSource");
  if (_vertexSourceFunction == [descriptor vertexFunction])
    vertexShaderKey.Set(_vertexSource);
  else
    DLOG(WARNING) << "Failed to capture vertex shader.";
  static crash_reporter::CrashKeyString<kShaderCrashDumpLength>
      fragmentShaderKey("MTLFragmentSource");
  if (_fragmentSourceFunction == [descriptor fragmentFunction])
    fragmentShaderKey.Set(_fragmentSource);
  else
    DLOG(WARNING) << "Failed to capture fragment shader.";
  static crash_reporter::CrashKeyString<16> newLibraryCountKey(
      "MTLNewLibraryCount");
  newLibraryCountKey.Set(base::NumberToString(_libraryCache->CacheMissCount()));

  SCOPED_UMA_HISTOGRAM_TIMER("Gpu.MetalProxy.NewRenderPipelineStateTime");
  id<MTLRenderPipelineState> pipelineState = NewRenderPipelineStateWithRetry(
      _device, descriptor, error, _progressReporter);

  vertexShaderKey.Clear();
  fragmentShaderKey.Clear();
  newLibraryCountKey.Clear();
  return pipelineState;
}

@end

#pragma clang diagnostic pop
