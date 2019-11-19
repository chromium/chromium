// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/gpu/metal_api_proxy.h"

#include "base/debug/crash_logging.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
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
class API_AVAILABLE(macos(10.11)) AsyncMetalState
    : public base::RefCountedThreadSafe<AsyncMetalState> {
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

id<MTLLibrary> API_AVAILABLE(macos(10.11))
    NewLibraryWithRetry(id<MTLDevice> device,
                        NSString* source,
                        MTLCompileOptions* options,
                        __autoreleasing NSError** error) {
  // Request and wait on an asynchronous shader compilation. If the compilation
  // does not return within kRetryPeriod, then re-issue the compilation request.
  // The value of kRetryPeriod is the 98th percentile of
  // Gpu.MetalProxy.NewLibraryTime.
  SCOPED_UMA_HISTOGRAM_TIMER("Gpu.MetalProxy.NewLibraryTime");
  const base::TimeDelta kRetryPeriod = base::TimeDelta::FromMilliseconds(50);

  auto state = base::MakeRefCounted<AsyncMetalState>();
  for (size_t attempt = 0;; ++attempt) {
    // The completion handler will signal the condition variable we will wait
    // on. Note that completionHandler will hold a reference to |state|.
    MTLNewLibraryCompletionHandler completionHandler = ^(id<MTLLibrary> library,
                                                         NSError* error) {
      base::AutoLock lock(state->lock);
      if (!state->has_result) {
        UMA_HISTOGRAM_COUNTS_100("Gpu.MetalProxy.NewLibraryAttempt", attempt);
        state->has_result = true;
        state->library = [library retain];
        state->error = [error retain];
        state->condition_variable.Signal();
      }
    };

    // Request asynchronous compilation. Note that |completionHandler| may be
    // called from within this function call, or it may be called from a
    // different thread.
    [device newLibraryWithSource:source
                         options:options
               completionHandler:completionHandler];

    // Wait for any of the previous calls to complete.
    base::AutoLock lock(state->lock);
    state->condition_variable.TimedWait(kRetryPeriod);

    // If we have results from any attempt, use them.
    if (state->has_result) {
      *error = [state->error autorelease];
      return state->library;
    }

    // Otherwise, try compiling the shader again. Keep re-trying forever until
    // the watchdog timer kills the process.
  }
}

id<MTLRenderPipelineState> API_AVAILABLE(macos(10.11))
    NewRenderPipelineStateWithRetry(id<MTLDevice> device,
                                    MTLRenderPipelineDescriptor* descriptor,
                                    __autoreleasing NSError** error) {
  // This function is almost-identical to the above NewLibraryWithRetry. See
  // comments in that function.
  // The value of kRetryPeriod is the 99th percentile of
  // Gpu.MetalProxy.NewRenderPipelineStateTime.
  SCOPED_UMA_HISTOGRAM_TIMER("Gpu.MetalProxy.NewRenderPipelineStateTime");
  const base::TimeDelta kRetryPeriod = base::TimeDelta::FromMilliseconds(50);
  auto state = base::MakeRefCounted<AsyncMetalState>();
  for (size_t attempt = 0;; ++attempt) {
    MTLNewRenderPipelineStateCompletionHandler completionHandler =
        ^(id<MTLRenderPipelineState> render_pipeline_state, NSError* error) {
          base::AutoLock lock(state->lock);
          if (!state->has_result) {
            UMA_HISTOGRAM_COUNTS_100(
                "Gpu.MetalProxy.NewRenderPipelineStateAttempt", attempt);
            state->has_result = true;
            state->render_pipeline_state = [render_pipeline_state retain];
            state->error = [error retain];
            state->condition_variable.Signal();
          }
        };
    [device newRenderPipelineStateWithDescriptor:descriptor
                               completionHandler:completionHandler];
    base::AutoLock lock(state->lock);
    state->condition_variable.TimedWait(kRetryPeriod);
    if (state->has_result) {
      *error = [state->error autorelease];
      return state->render_pipeline_state;
    }
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
class API_AVAILABLE(macos(10.11)) MTLLibraryCache {
 public:
  MTLLibraryCache() = default;
  ~MTLLibraryCache() = default;

  id<MTLLibrary> NewLibraryWithSource(id<MTLDevice> device,
                                      NSString* source,
                                      MTLCompileOptions* options,
                                      __autoreleasing NSError** error) {
    LibraryKey key(source, options);
    auto found = libraries_.find(key);
    if (found != libraries_.end()) {
      const LibraryData& data = found->second;
      *error = [[data.error retain] autorelease];
      return [data.library retain];
    }
    SCOPED_UMA_HISTOGRAM_TIMER("Gpu.MetalProxy.NewLibraryTime");
    id<MTLLibrary> library =
        NewLibraryWithRetry(device, source, options, error);
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

@implementation MTLDeviceProxy
- (id)initWithDevice:(id<MTLDevice>)device {
  if (self = [super init]) {
    device_.reset(device, base::scoped_policy::RETAIN);
    libraryCache_ = std::make_unique<MTLLibraryCache>();
  }
  return self;
}

- (void)setProgressReporter:(gl::ProgressReporter*)progressReporter {
  progressReporter_ = progressReporter;
}

// Wrappers that add a gl::ScopedProgressReporter around calls to the true
// MTLDevice. For a given method, the method name is fn, return type is R, the
// argument types are A0,A1,A2,A3, and the argument names are a0,a1,a2,a3.
#define PROXY_METHOD0(R, fn) \
  -(R)fn {                   \
    return [device_ fn];     \
  }
#define PROXY_METHOD1(R, fn, A0) \
  -(R)fn : (A0)a0 {              \
    return [device_ fn:a0];      \
  }
#define PROXY_METHOD2(R, fn, A0, a1, A1) \
  -(R)fn : (A0)a0 a1 : (A1)a1 {          \
    return [device_ fn:a0 a1:a1];        \
  }
#define PROXY_METHOD3(R, fn, A0, a1, A1, a2, A2) \
  -(R)fn : (A0)a0 a1 : (A1)a1 : (A2)a2 {          \
    return [device_ fn:a0 a1:a1 a2:a2];        \
  }
#define PROXY_METHOD0_SLOW(R, fn)                                  \
  -(R)fn {                                                         \
    TRACE_EVENT0("gpu", "-[MTLDevice " #fn "]");                   \
    gl::ScopedProgressReporter scoped_reporter(progressReporter_); \
    return [device_ fn];                                           \
  }
#define PROXY_METHOD1_SLOW(R, fn, A0)                              \
  -(R)fn : (A0)a0 {                                                \
    TRACE_EVENT0("gpu", "-[MTLDevice " #fn ":]");                  \
    gl::ScopedProgressReporter scoped_reporter(progressReporter_); \
    return [device_ fn:a0];                                        \
  }
#define PROXY_METHOD2_SLOW(R, fn, A0, a1, A1)                      \
  -(R)fn : (A0)a0 a1 : (A1)a1 {                                    \
    TRACE_EVENT0("gpu", "-[MTLDevice " #fn ":" #a1 ":]");          \
    gl::ScopedProgressReporter scoped_reporter(progressReporter_); \
    return [device_ fn:a0 a1:a1];                                  \
  }
#define PROXY_METHOD3_SLOW(R, fn, A0, a1, A1, a2, A2)              \
  -(R)fn : (A0)a0 a1 : (A1)a1 a2 : (A2)a2 {                        \
    TRACE_EVENT0("gpu", "-[MTLDevice " #fn ":" #a1 ":" #a2 ":]");  \
    gl::ScopedProgressReporter scoped_reporter(progressReporter_); \
    return [device_ fn:a0 a1:a1 a2:a2];                            \
  }
#define PROXY_METHOD4_SLOW(R, fn, A0, a1, A1, a2, A2, a3, A3)             \
  -(R)fn : (A0)a0 a1 : (A1)a1 a2 : (A2)a2 a3 : (A3)a3 {                   \
    TRACE_EVENT0("gpu", "-[MTLDevice " #fn ":" #a1 ":" #a2 ":" #a3 ":]"); \
    gl::ScopedProgressReporter scoped_reporter(progressReporter_);        \
    return [device_ fn:a0 a1:a1 a2:a2 a3:a3];                             \
  }

// Disable availability warnings for the calls to |device_| in the macros. (The
// relevant availability guards are already present in the MTLDevice protocol
// for |self|).
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunguarded-availability"

// Wrapped implementation of the MTLDevice protocol in which some methods
// have a gl::ScopedProgressReporter. The methods implemented using macros
// with the _SLOW suffix are the ones that create a gl::ScopedProgressReporter.
// The rule of thumb is that methods that could potentially do a GPU allocation
// or a shader compilation are marked as SLOW.
PROXY_METHOD0(NSString*, name)
PROXY_METHOD0(uint64_t, registryID)
PROXY_METHOD0(MTLSize, maxThreadsPerThreadgroup)
PROXY_METHOD0(BOOL, isLowPower)
PROXY_METHOD0(BOOL, isHeadless)
PROXY_METHOD0(BOOL, isRemovable)
PROXY_METHOD0(uint64_t, recommendedMaxWorkingSetSize)
PROXY_METHOD0(BOOL, isDepth24Stencil8PixelFormatSupported)
PROXY_METHOD0(MTLReadWriteTextureTier, readWriteTextureSupport)
PROXY_METHOD0(MTLArgumentBuffersTier, argumentBuffersSupport)
PROXY_METHOD0(BOOL, areRasterOrderGroupsSupported)
PROXY_METHOD0(NSUInteger, currentAllocatedSize)
PROXY_METHOD0(NSUInteger, maxThreadgroupMemoryLength)
PROXY_METHOD0(BOOL, areProgrammableSamplePositionsSupported)
PROXY_METHOD0_SLOW(nullable id<MTLCommandQueue>, newCommandQueue)
PROXY_METHOD1_SLOW(nullable id<MTLCommandQueue>,
                   newCommandQueueWithMaxCommandBufferCount,
                   NSUInteger)
PROXY_METHOD1(MTLSizeAndAlign,
              heapTextureSizeAndAlignWithDescriptor,
              MTLTextureDescriptor*)
PROXY_METHOD2(MTLSizeAndAlign,
              heapBufferSizeAndAlignWithLength,
              NSUInteger,
              options,
              MTLResourceOptions)
PROXY_METHOD1_SLOW(nullable id<MTLHeap>,
                   newHeapWithDescriptor,
                   MTLHeapDescriptor*)
PROXY_METHOD2_SLOW(nullable id<MTLBuffer>,
                   newBufferWithLength,
                   NSUInteger,
                   options,
                   MTLResourceOptions)
PROXY_METHOD3_SLOW(nullable id<MTLBuffer>,
                   newBufferWithBytes,
                   const void*,
                   length,
                   NSUInteger,
                   options,
                   MTLResourceOptions)
PROXY_METHOD4_SLOW(nullable id<MTLBuffer>,
                   newBufferWithBytesNoCopy,
                   void*,
                   length,
                   NSUInteger,
                   options,
                   MTLResourceOptions,
                   deallocator,
                   void (^__nullable)(void* pointer, NSUInteger length))
PROXY_METHOD1(nullable id<MTLDepthStencilState>,
              newDepthStencilStateWithDescriptor,
              MTLDepthStencilDescriptor*)
PROXY_METHOD1_SLOW(nullable id<MTLTexture>,
                   newTextureWithDescriptor,
                   MTLTextureDescriptor*)
PROXY_METHOD3_SLOW(nullable id<MTLTexture>,
                   newTextureWithDescriptor,
                   MTLTextureDescriptor*,
                   iosurface,
                   IOSurfaceRef,
                   plane,
                   NSUInteger)
PROXY_METHOD1_SLOW(nullable id<MTLSamplerState>,
                   newSamplerStateWithDescriptor,
                   MTLSamplerDescriptor*)
PROXY_METHOD0_SLOW(nullable id<MTLLibrary>, newDefaultLibrary)
PROXY_METHOD2_SLOW(nullable id<MTLLibrary>,
                   newDefaultLibraryWithBundle,
                   NSBundle*,
                   error,
                   __autoreleasing NSError**)
PROXY_METHOD2_SLOW(nullable id<MTLLibrary>,
                   newLibraryWithFile,
                   NSString*,
                   error,
                   __autoreleasing NSError**)
PROXY_METHOD2_SLOW(nullable id<MTLLibrary>,
                   newLibraryWithURL,
                   NSURL*,
                   error,
                   __autoreleasing NSError**)
PROXY_METHOD2_SLOW(nullable id<MTLLibrary>,
                   newLibraryWithData,
                   dispatch_data_t,
                   error,
                   __autoreleasing NSError**)

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
  newLibraryCountKey.Set(base::NumberToString(libraryCache_->CacheMissCount()));

  gl::ScopedProgressReporter scoped_reporter(progressReporter_);
  id<MTLLibrary> library =
      libraryCache_->NewLibraryWithSource(device_, source, options, error);
  shaderKey.Clear();
  newLibraryCountKey.Clear();

  // Shaders from Skia will have either a vertexMain or fragmentMain function.
  // Save the source and a weak pointer to the function, so we can capture
  // the shader source in -newRenderPipelineStateWithDescriptor (see further
  // remarks in that function).
  base::scoped_nsprotocol<id<MTLFunction>> vertexFunction(
      [library newFunctionWithName:@"vertexMain"]);
  if (vertexFunction) {
    vertexSourceFunction_ = vertexFunction;
    vertexSource_ = sourceAsSysString;
  }
  base::scoped_nsprotocol<id<MTLFunction>> fragmentFunction(
      [library newFunctionWithName:@"fragmentMain"]);
  if (fragmentFunction) {
    fragmentSourceFunction_ = fragmentFunction;
    fragmentSource_ = sourceAsSysString;
  }

  return library;
}
PROXY_METHOD3_SLOW(void,
                   newLibraryWithSource,
                   NSString*,
                   options,
                   nullable MTLCompileOptions*,
                   completionHandler,
                   MTLNewLibraryCompletionHandler)
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
  if (vertexSourceFunction_ == [descriptor vertexFunction])
    vertexShaderKey.Set(vertexSource_);
  else
    DLOG(WARNING) << "Failed to capture vertex shader.";
  static crash_reporter::CrashKeyString<kShaderCrashDumpLength>
      fragmentShaderKey("MTLFragmentSource");
  if (fragmentSourceFunction_ == [descriptor fragmentFunction])
    fragmentShaderKey.Set(fragmentSource_);
  else
    DLOG(WARNING) << "Failed to capture fragment shader.";
  static crash_reporter::CrashKeyString<16> newLibraryCountKey(
      "MTLNewLibraryCount");
  newLibraryCountKey.Set(base::NumberToString(libraryCache_->CacheMissCount()));

  gl::ScopedProgressReporter scoped_reporter(progressReporter_);
  SCOPED_UMA_HISTOGRAM_TIMER("Gpu.MetalProxy.NewRenderPipelineStateTime");
  id<MTLRenderPipelineState> pipelineState =
      NewRenderPipelineStateWithRetry(device_, descriptor, error);

  vertexShaderKey.Clear();
  fragmentShaderKey.Clear();
  newLibraryCountKey.Clear();
  return pipelineState;
}
PROXY_METHOD4_SLOW(nullable id<MTLRenderPipelineState>,
                   newRenderPipelineStateWithDescriptor,
                   MTLRenderPipelineDescriptor*,
                   options,
                   MTLPipelineOption,
                   reflection,
                   MTLAutoreleasedRenderPipelineReflection* __nullable,
                   error,
                   __autoreleasing NSError**)
PROXY_METHOD2_SLOW(void,
                   newRenderPipelineStateWithDescriptor,
                   MTLRenderPipelineDescriptor*,
                   completionHandler,
                   MTLNewRenderPipelineStateCompletionHandler)
PROXY_METHOD3_SLOW(void,
                   newRenderPipelineStateWithDescriptor,
                   MTLRenderPipelineDescriptor*,
                   options,
                   MTLPipelineOption,
                   completionHandler,
                   MTLNewRenderPipelineStateWithReflectionCompletionHandler)
PROXY_METHOD2_SLOW(nullable id<MTLComputePipelineState>,
                   newComputePipelineStateWithFunction,
                   id<MTLFunction>,
                   error,
                   __autoreleasing NSError**)
PROXY_METHOD4_SLOW(nullable id<MTLComputePipelineState>,
                   newComputePipelineStateWithFunction,
                   id<MTLFunction>,
                   options,
                   MTLPipelineOption,
                   reflection,
                   MTLAutoreleasedComputePipelineReflection* __nullable,
                   error,
                   __autoreleasing NSError**)
PROXY_METHOD2_SLOW(void,
                   newComputePipelineStateWithFunction,
                   id<MTLFunction>,
                   completionHandler,
                   MTLNewComputePipelineStateCompletionHandler)
PROXY_METHOD3_SLOW(void,
                   newComputePipelineStateWithFunction,
                   id<MTLFunction>,
                   options,
                   MTLPipelineOption,
                   completionHandler,
                   MTLNewComputePipelineStateWithReflectionCompletionHandler)
PROXY_METHOD4_SLOW(nullable id<MTLComputePipelineState>,
                   newComputePipelineStateWithDescriptor,
                   MTLComputePipelineDescriptor*,
                   options,
                   MTLPipelineOption,
                   reflection,
                   MTLAutoreleasedComputePipelineReflection* __nullable,
                   error,
                   __autoreleasing NSError**)
PROXY_METHOD3_SLOW(void,
                   newComputePipelineStateWithDescriptor,
                   MTLComputePipelineDescriptor*,
                   options,
                   MTLPipelineOption,
                   completionHandler,
                   MTLNewComputePipelineStateWithReflectionCompletionHandler)
PROXY_METHOD0_SLOW(nullable id<MTLFence>, newFence)
PROXY_METHOD1(BOOL, supportsFeatureSet, MTLFeatureSet)
PROXY_METHOD1(BOOL, supportsTextureSampleCount, NSUInteger)
PROXY_METHOD1(NSUInteger,
              minimumLinearTextureAlignmentForPixelFormat,
              MTLPixelFormat)
PROXY_METHOD2(void,
              getDefaultSamplePositions,
              MTLSamplePosition*,
              count,
              NSUInteger)
PROXY_METHOD1_SLOW(nullable id<MTLArgumentEncoder>,
                   newArgumentEncoderWithArguments,
                   NSArray<MTLArgumentDescriptor*>*)

#if defined(MAC_OS_X_VERSION_10_14)
PROXY_METHOD1_SLOW(nullable id<MTLTexture>,
                   newSharedTextureWithDescriptor,
                   MTLTextureDescriptor*)
PROXY_METHOD1_SLOW(nullable id<MTLTexture>,
                   newSharedTextureWithHandle,
                   MTLSharedTextureHandle*)
PROXY_METHOD1(NSUInteger,
              minimumTextureBufferAlignmentForPixelFormat,
              MTLPixelFormat)
PROXY_METHOD0(NSUInteger, maxBufferLength)
PROXY_METHOD0(NSUInteger, maxArgumentBufferSamplerCount)
PROXY_METHOD3_SLOW(nullable id<MTLIndirectCommandBuffer>,
                   newIndirectCommandBufferWithDescriptor,
                   MTLIndirectCommandBufferDescriptor*,
                   maxCommandCount,
                   NSUInteger,
                   options,
                   MTLResourceOptions)
PROXY_METHOD0(nullable id<MTLEvent>, newEvent)
PROXY_METHOD0(nullable id<MTLSharedEvent>, newSharedEvent)
PROXY_METHOD1(nullable id<MTLSharedEvent>,
              newSharedEventWithHandle,
              MTLSharedEventHandle*)
#endif  // MAC_OS_X_VERSION_10_14

#if defined(MAC_OS_X_VERSION_10_15)
PROXY_METHOD0(BOOL, hasUnifiedMemory)
PROXY_METHOD0(MTLDeviceLocation, location)
PROXY_METHOD0(NSUInteger, locationNumber)
PROXY_METHOD0(uint64_t, maxTransferRate)
PROXY_METHOD0(BOOL, areBarycentricCoordsSupported)
PROXY_METHOD0(BOOL, supportsShaderBarycentricCoordinates)
PROXY_METHOD0(uint64_t, peerGroupID)
PROXY_METHOD0(uint32_t, peerIndex)
PROXY_METHOD0(uint32_t, peerCount)
PROXY_METHOD0(nullable NSArray<id<MTLCounterSet>>*, counterSets)
PROXY_METHOD1(BOOL, supportsFamily, MTLGPUFamily)
PROXY_METHOD2_SLOW(nullable id<MTLCounterSampleBuffer>,
                   newCounterSampleBufferWithDescriptor,
                   MTLCounterSampleBufferDescriptor*,
                   error,
                   NSError**)
PROXY_METHOD2(void, sampleTimestamps, NSUInteger*, gpuTimestamp, NSUInteger*)
#endif  // MAC_OS_X_VERSION_10_15

#pragma clang diagnostic pop
@end
