// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/paint_preview_compositor/paint_preview_compositor_collection_impl.h"

#include <utility>

#include "base/features.h"
#include "base/memory/discardable_memory.h"
#include "base/memory/discardable_memory_allocator.h"
#include "base/system/sys_info.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"
#include "components/crash/core/common/crash_key.h"
#include "content/public/utility/utility_thread.h"
#include "skia/ext/font_utils.h"
#include "third_party/skia/include/core/SkFontMgr.h"
#include "third_party/skia/include/core/SkGraphics.h"
#include "third_party/skia/include/ports/SkFontConfigInterface.h"

#if BUILDFLAG(IS_WIN)
#include "content/public/child/dwrite_font_proxy_init_win.h"
#elif BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#include "components/services/font/public/cpp/font_loader.h"
#endif

namespace paint_preview {

namespace {
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS)
// A parameter to exclude or not exclude PaintPreviewCompositor from
// PartialLowModeOnMidRangeDevices. This is used to see how
// PaintPreviewCompositor affects
// Startup.Android.Cold.TimeToFirstVisibleContent.
const base::FeatureParam<bool> kPartialLowEndModeExcludePaintPreviewCompositor{
    &base::features::kPartialLowEndModeOnMidRangeDevices,
    "exclude-paint-preview-compositor", false};
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS)

// Record whether the compositor is in shutdown. Discardable memory allocations
// manifest as OOMs during shutdown due to failure to send IPC messages. By
// recording whether the process is shutting down it is possible to determine if
// the OOM is actionable or just a consequence of the process no longer having
// IPC access.
crash_reporter::CrashKeyString<32> g_in_shutdown_key(
    "paint-preview-compositor-in-shutdown");
}  // namespace

PaintPreviewCompositorCollectionImpl::PaintPreviewCompositorCollectionImpl(
    mojo::PendingReceiver<mojom::PaintPreviewCompositorCollection> receiver,
    bool initialize_environment,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner)
    : initialize_environment_(initialize_environment),
      io_task_runner_(std ::move(io_task_runner)) {
  g_in_shutdown_key.Set("false");
  if (receiver)
    receiver_.Bind(std::move(receiver));

  // Adapted from content::InitializeSkia().
  // TODO(crbug.com/40178027): Tune these limits.
  constexpr int kMB = 1024 * 1024;
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS)
  bool is_low_end_mode =
      base::SysInfo::IsLowEndDeviceOrPartialLowEndModeEnabled(
          kPartialLowEndModeExcludePaintPreviewCompositor);
  SkGraphics::SetFontCacheLimit(is_low_end_mode ? kMB : 8 * kMB);
  SkGraphics::SetResourceCacheTotalByteLimit(is_low_end_mode ? 32 * kMB
                                                             : 64 * kMB);
  SkGraphics::SetResourceCacheSingleAllocationByteLimit(16 * kMB);
#else
  SkGraphics::SetResourceCacheSingleAllocationByteLimit(64 * kMB);
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS)

  if (!initialize_environment_)
    return;

    // Initialize font access for Skia.
#if BUILDFLAG(IS_WIN)
  content::InitializeDWriteFontProxy();
#elif BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  mojo::PendingRemote<font_service::mojom::FontService> font_service;
  content::UtilityThread::Get()->BindHostReceiver(
      font_service.InitWithNewPipeAndPassReceiver());
  SkFontConfigInterface::SetGlobal(
      sk_make_sp<font_service::FontLoader>(std::move(font_service)));
#endif
  // TODO(crbug.com/40106998): Determine if EnsureBlinkInitialized*() does any
  // other initialization we require. Possibly for other platforms (e.g. MacOS,
  // Android). In theory, WebSandboxSupport isn't required since we subset and
  // load all required fonts into the Skia Pictures for portability so they are
  // all local; however, this may be required for initialization on MacOS?

  // TODO(crbug.com/40102887): PDF compositor initializes Blink to leverage some
  // codecs for images. This is a huge overhead and shouldn't be necessary for
  // us. However, this may break some formats (WEBP?) so we may need to force
  // encoding to PNG or we could provide our own codec implementations.

  // Init this on the background thread for a startup performance improvement.
  base::ThreadPool::PostTask(FROM_HERE,
                             base::BindOnce([] { skia::DefaultFontMgr(); }));

  // Sanity check that fonts are working.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  // No WebSandbox is provided on Linux so the local fonts aren't accessible.
  // This is fine since since the subsetted fonts are provided in the SkPicture.
  // However, we still need to check that the SkFontMgr starts as it is used by
  // Skia when handling the SkPicture.
  DCHECK(skia::DefaultFontMgr());
#else
  DCHECK(skia::DefaultFontMgr()->countFamilies());
#endif
}

PaintPreviewCompositorCollectionImpl::~PaintPreviewCompositorCollectionImpl() {
  g_in_shutdown_key.Set("true");
#if BUILDFLAG(IS_WIN)
  content::UninitializeDWriteFontProxy();
#endif
}

void PaintPreviewCompositorCollectionImpl::SetDiscardableSharedMemoryManager(
    mojo::PendingRemote<
        discardable_memory::mojom::DiscardableSharedMemoryManager> manager) {
  mojo::PendingRemote<discardable_memory::mojom::DiscardableSharedMemoryManager>
      manager_remote(std::move(manager));
  discardable_shared_memory_manager_ = base::MakeRefCounted<
      discardable_memory::ClientDiscardableSharedMemoryManager>(
      std::move(manager_remote), io_task_runner_);
  base::DiscardableMemoryAllocator::SetInstance(
      discardable_shared_memory_manager_.get());
}

void PaintPreviewCompositorCollectionImpl::CreateCompositor(
    mojo::PendingReceiver<mojom::PaintPreviewCompositor> receiver,
    PaintPreviewCompositorCollectionImpl::CreateCompositorCallback callback) {
  DCHECK(discardable_shared_memory_manager_ || !initialize_environment_);
  base::UnguessableToken token = base::UnguessableToken::Create();
  compositors_.insert(
      {token,
       std::make_unique<PaintPreviewCompositorImpl>(
           std::move(receiver), discardable_shared_memory_manager_,
           base::BindOnce(&PaintPreviewCompositorCollectionImpl::OnDisconnect,
                          weak_ptr_factory_.GetWeakPtr(), token))});
  std::move(callback).Run(token);
}

void PaintPreviewCompositorCollectionImpl::OnMemoryPressure(
    base::MemoryPressureListener::MemoryPressureLevel memory_pressure_level) {
  if (memory_pressure_level >=
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE) {
    SkGraphics::PurgeAllCaches();
    if (discardable_shared_memory_manager_) {
      discardable_shared_memory_manager_->ReleaseFreeMemory();
    }
  }
}

void PaintPreviewCompositorCollectionImpl::ListCompositors(
    ListCompositorsCallback callback) {
  std::vector<base::UnguessableToken> ids;
  ids.reserve(compositors_.size());
  for (const auto& compositor : compositors_)
    ids.push_back(compositor.first);
  std::move(callback).Run(std::move(ids));
}

void PaintPreviewCompositorCollectionImpl::OnDisconnect(
    const base::UnguessableToken& id) {
  compositors_.erase(id);
}

}  // namespace paint_preview
