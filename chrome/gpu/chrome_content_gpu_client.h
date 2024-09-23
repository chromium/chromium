// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_GPU_CHROME_CONTENT_GPU_CLIENT_H_
#define CHROME_GPU_CHROME_CONTENT_GPU_CLIENT_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/task/single_thread_task_runner.h"
#include "build/chromeos_buildflags.h"
#include "content/public/gpu/content_gpu_client.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
namespace arc {
class ProtectedBufferManager;
}  // namespace arc
#endif

namespace sampling_profiler {
class ThreadProfiler;
}

class ChromeContentGpuClient : public content::ContentGpuClient {
 public:
  ChromeContentGpuClient();

  ChromeContentGpuClient(const ChromeContentGpuClient&) = delete;
  ChromeContentGpuClient& operator=(const ChromeContentGpuClient&) = delete;

  ~ChromeContentGpuClient() override;

  // content::ContentGpuClient:
  void GpuServiceInitialized() override;
  void ExposeInterfacesToBrowser(
      const gpu::GpuPreferences& gpu_preferences,
      const gpu::GpuDriverBugWorkarounds& gpu_workarounds,
      mojo::BinderMap* binders) override;
  void PostSandboxInitialized() override;
  void PostIOThreadCreated(
      base::SingleThreadTaskRunner* io_task_runner) override;
  void PostCompositorThreadCreated(
      base::SingleThreadTaskRunner* task_runner) override;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  scoped_refptr<arc::ProtectedBufferManager> GetProtectedBufferManager();
#endif

 private:
  // Used to profile main thread startup.
  std::unique_ptr<sampling_profiler::ThreadProfiler> main_thread_profiler_;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  scoped_refptr<arc::ProtectedBufferManager> protected_buffer_manager_;
#endif
};

#endif  // CHROME_GPU_CHROME_CONTENT_GPU_CLIENT_H_
