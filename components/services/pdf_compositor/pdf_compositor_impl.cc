// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/pdf_compositor/pdf_compositor_impl.h"

#include <algorithm>
#include <tuple>
#include <utility>

#include "base/logging.h"
#include "base/memory/discardable_memory.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "build/build_config.h"
#include "components/crash/core/common/crash_key.h"
#include "components/discardable_memory/client/client_discardable_shared_memory_manager.h"
#include "components/services/pdf_compositor/public/cpp/pdf_service_mojo_types.h"
#include "content/public/utility/utility_thread.h"
#include "mojo/public/cpp/base/shared_memory_utils.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "printing/common/metafile_utils.h"
#include "third_party/blink/public/platform/web_image_generator.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkDocument.h"
#include "third_party/skia/include/core/SkGraphics.h"
#include "third_party/skia/include/core/SkSerialProcs.h"
#include "third_party/skia/src/utils/SkMultiPictureDocument.h"

#if defined(OS_WIN)
#include "content/public/child/dwrite_font_proxy_init_win.h"
#elif defined(OS_MACOSX)
#include "third_party/blink/public/platform/platform.h"
#include "third_party/skia/include/core/SkFontMgr.h"
#elif defined(OS_POSIX) && !defined(OS_ANDROID)
#include "third_party/blink/public/platform/platform.h"
#endif

namespace printing {

PdfCompositorImpl::PdfCompositorImpl(
    mojo::PendingReceiver<mojom::PdfCompositor> receiver,
    bool initialize_environment,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner)
    : io_task_runner_(std::move(io_task_runner)) {
  if (receiver)
    receiver_.Bind(std::move(receiver));

  if (!initialize_environment)
    return;

#if defined(OS_WIN)
  // Initialize direct write font proxy so skia can use it.
  content::InitializeDWriteFontProxy();
#endif

  // Hook up blink's codecs so skia can call them.
  SkGraphics::SetImageGeneratorFromEncodedDataFactory(
      blink::WebImageGenerator::CreateAsSkImageGenerator);

#if defined(OS_POSIX) && !defined(OS_ANDROID)
  content::UtilityThread::Get()->EnsureBlinkInitializedWithSandboxSupport();
  // Check that we have sandbox support on this platform.
  DCHECK(blink::Platform::Current()->GetSandboxSupport());
#else
  content::UtilityThread::Get()->EnsureBlinkInitialized();
#endif

#if defined(OS_MACOSX)
  // Check that font access is granted.
  // This doesn't do comprehensive tests to make sure fonts can work properly.
  // It is just a quick and simple check to catch things like improper sandbox
  // policy setup.
  DCHECK(SkFontMgr::RefDefault()->countFamilies());
#endif
}

PdfCompositorImpl::~PdfCompositorImpl() {
#if defined(OS_WIN)
  content::UninitializeDWriteFontProxy();
#endif
}

void PdfCompositorImpl::SetDiscardableSharedMemoryManager(
    mojo::PendingRemote<
        discardable_memory::mojom::DiscardableSharedMemoryManager> manager) {
  // Set up discardable memory manager.
  mojo::PendingRemote<discardable_memory::mojom::DiscardableSharedMemoryManager>
      manager_remote(std::move(manager));
  discardable_shared_memory_manager_ = std::make_unique<
      discardable_memory::ClientDiscardableSharedMemoryManager>(
      std::move(manager_remote), io_task_runner_);
  base::DiscardableMemoryAllocator::SetInstance(
      discardable_shared_memory_manager_.get());
}

void PdfCompositorImpl::NotifyUnavailableSubframe(uint64_t frame_guid) {
  // Add this frame into the map.
  DCHECK(!base::Contains(frame_info_map_, frame_guid));
  auto& frame_info =
      frame_info_map_.emplace(frame_guid, std::make_unique<FrameInfo>())
          .first->second;
  frame_info->composited = true;
  // Set content to be nullptr so it will be replaced by an empty picture during
  // deserialization of its parent.
  frame_info->content = nullptr;

  // Update the requests in case any of them might be waiting for this frame.
  UpdateRequestsWithSubframeInfo(frame_guid, std::vector<uint64_t>());
}

void PdfCompositorImpl::AddSubframeContent(
    uint64_t frame_guid,
    base::ReadOnlySharedMemoryRegion serialized_content,
    const ContentToFrameMap& subframe_content_map) {
  base::ReadOnlySharedMemoryMapping mapping = serialized_content.Map();
  if (!mapping.IsValid()) {
    NotifyUnavailableSubframe(frame_guid);
    return;
  }

  // Add this frame and its serialized content.
  DCHECK(!base::Contains(frame_info_map_, frame_guid));
  auto& frame_info =
      frame_info_map_.emplace(frame_guid, std::make_unique<FrameInfo>())
          .first->second;
  frame_info->serialized_content = std::move(mapping);

  // Copy the subframe content information.
  frame_info->subframe_content_map = subframe_content_map;

  // If there is no request, we do nothing more.
  // Otherwise, we need to check whether any request actually waits on this
  // frame content.
  if (requests_.empty())
    return;

  // Get the pending list which is a list of subframes this frame needs
  // but are still unavailable.
  std::vector<uint64_t> pending_subframes;
  for (auto& subframe_content : subframe_content_map) {
    auto subframe_guid = subframe_content.second;
    if (!base::Contains(frame_info_map_, subframe_guid))
      pending_subframes.push_back(subframe_guid);
  }

  // Update the requests in case any of them is waiting for this frame.
  UpdateRequestsWithSubframeInfo(frame_guid, pending_subframes);
}

void PdfCompositorImpl::CompositePageToPdf(
    uint64_t frame_guid,
    base::ReadOnlySharedMemoryRegion serialized_content,
    const ContentToFrameMap& subframe_content_map,
    mojom::PdfCompositor::CompositePageToPdfCallback callback) {
  if (docinfo_)
    docinfo_->pages_provided++;
  HandleCompositionRequest(frame_guid, std::move(serialized_content),
                           subframe_content_map, std::move(callback));
}

void PdfCompositorImpl::CompositeDocumentToPdf(
    uint64_t frame_guid,
    base::ReadOnlySharedMemoryRegion serialized_content,
    const ContentToFrameMap& subframe_content_map,
    mojom::PdfCompositor::CompositeDocumentToPdfCallback callback) {
  DCHECK(!docinfo_);
  HandleCompositionRequest(frame_guid, std::move(serialized_content),
                           subframe_content_map, std::move(callback));
}

void PdfCompositorImpl::PrepareForDocumentToPdf(
    mojom::PdfCompositor::PrepareForDocumentToPdfCallback callback) {
  DCHECK(!docinfo_);
  docinfo_ = std::make_unique<DocumentInfo>(creator_);
  std::move(callback).Run(mojom::PdfCompositor::Status::kSuccess);
}

void PdfCompositorImpl::CompleteDocumentToPdf(
    uint32_t page_count,
    mojom::PdfCompositor::CompleteDocumentToPdfCallback callback) {
  DCHECK(docinfo_);
  DCHECK_GT(page_count, 0U);
  docinfo_->page_count = page_count;
  docinfo_->callback = std::move(callback);
  HandleDocumentCompletionRequest();
}

void PdfCompositorImpl::SetWebContentsURL(const GURL& url) {
  // Record the most recent url we tried to print. This should be sufficient
  // for users using print preview by default.
  static crash_reporter::CrashKeyString<1024> crash_key("main-frame-url");
  crash_key.Set(url.spec());
}

void PdfCompositorImpl::SetUserAgent(const std::string& user_agent) {
  if (!user_agent.empty())
    creator_ = user_agent;
}

void PdfCompositorImpl::UpdateRequestsWithSubframeInfo(
    uint64_t frame_guid,
    const std::vector<uint64_t>& pending_subframes) {
  // Check for each request's pending list.
  for (auto it = requests_.begin(); it != requests_.end();) {
    auto& request = *it;
    // If the request needs this frame, we can remove the dependency, but
    // update with this frame's pending list.
    auto& pending_list = request->pending_subframes;
    if (pending_list.erase(frame_guid)) {
      std::copy(pending_subframes.begin(), pending_subframes.end(),
                std::inserter(pending_list, pending_list.end()));
      if (pending_list.empty()) {
        // If the request isn't waiting on any subframes then it is ready.
        // Fulfill the request now.
        FulfillRequest(std::move(request->serialized_content),
                       request->subframe_content_map,
                       std::move(request->callback));

        // Check for a collected print preview document that was waiting on
        // this page to finish.
        if (docinfo_) {
          if (docinfo_->page_count &&
              (docinfo_->pages_written == docinfo_->page_count)) {
            CompleteDocumentRequest(std::move(docinfo_->callback));
          }
        }
        it = requests_.erase(it);
        continue;
      }
    }
    // If the request still has pending frames, keep waiting.
    ++it;
  }
}

bool PdfCompositorImpl::IsReadyToComposite(
    uint64_t frame_guid,
    const ContentToFrameMap& subframe_content_map,
    base::flat_set<uint64_t>* pending_subframes) const {
  pending_subframes->clear();
  base::flat_set<uint64_t> visited_frames = {frame_guid};
  CheckFramesForReadiness(subframe_content_map, pending_subframes,
                          &visited_frames);
  return pending_subframes->empty();
}

void PdfCompositorImpl::CheckFramesForReadiness(
    const ContentToFrameMap& subframe_content_map,
    base::flat_set<uint64_t>* pending_subframes,
    base::flat_set<uint64_t>* visited) const {
  for (auto& subframe_content : subframe_content_map) {
    auto subframe_guid = subframe_content.second;
    // If this frame has been checked, skip it.
    if (!visited->insert(subframe_guid).second)
      continue;

    auto iter = frame_info_map_.find(subframe_guid);
    if (iter == frame_info_map_.end()) {
      pending_subframes->insert(subframe_guid);
    } else {
      CheckFramesForReadiness(iter->second->subframe_content_map,
                              pending_subframes, visited);
    }
  }
}

void PdfCompositorImpl::HandleCompositionRequest(
    uint64_t frame_guid,
    base::ReadOnlySharedMemoryRegion serialized_content,
    const ContentToFrameMap& subframe_content_map,
    CompositeToPdfCallback callback) {
  base::ReadOnlySharedMemoryMapping mapping = serialized_content.Map();
  if (!mapping.IsValid()) {
    DLOG(ERROR) << "HandleCompositionRequest: Cannot map input.";
    std::move(callback).Run(mojom::PdfCompositor::Status::kHandleMapError,
                            base::ReadOnlySharedMemoryRegion());
    return;
  }

  base::flat_set<uint64_t> pending_subframes;
  if (IsReadyToComposite(frame_guid, subframe_content_map,
                         &pending_subframes)) {
    FulfillRequest(std::move(mapping), subframe_content_map,
                   std::move(callback));
    return;
  }

  // When it is not ready yet, keep its information and
  // wait until all dependent subframes are ready.
  auto iter = frame_info_map_.find(frame_guid);
  if (iter == frame_info_map_.end())
    frame_info_map_[frame_guid] = std::make_unique<FrameInfo>();

  requests_.push_back(std::make_unique<RequestInfo>(
      std::move(mapping), subframe_content_map, std::move(pending_subframes),
      std::move(callback)));
}

void PdfCompositorImpl::HandleDocumentCompletionRequest() {
  if (docinfo_->pages_written == docinfo_->page_count) {
    CompleteDocumentRequest(std::move(docinfo_->callback));
    return;
  }
  // Just need to wait on pages to percolate through processing, callback will
  // be handled from UpdateRequestsWithSubframeInfo() once the pending requests
  // have finished.
}

mojom::PdfCompositor::Status PdfCompositorImpl::CompositeToPdf(
    base::ReadOnlySharedMemoryMapping shared_mem,
    const ContentToFrameMap& subframe_content_map,
    base::ReadOnlySharedMemoryRegion* region) {
  if (!shared_mem.IsValid()) {
    DLOG(ERROR) << "CompositeToPdf: Invalid input.";
    return mojom::PdfCompositor::Status::kHandleMapError;
  }

  DeserializationContext subframes =
      GetDeserializationContext(subframe_content_map);

  // Read in content and convert it into pdf.
  SkMemoryStream stream(shared_mem.memory(), shared_mem.size());
  int page_count = SkMultiPictureDocumentReadPageCount(&stream);
  if (!page_count) {
    DLOG(ERROR) << "CompositeToPdf: No page is read.";
    return mojom::PdfCompositor::Status::kContentFormatError;
  }

  std::vector<SkDocumentPage> pages(page_count);
  SkDeserialProcs procs = DeserializationProcs(&subframes);
  if (!SkMultiPictureDocumentRead(&stream, pages.data(), page_count, &procs)) {
    DLOG(ERROR) << "CompositeToPdf: Page reading failed.";
    return mojom::PdfCompositor::Status::kContentFormatError;
  }

  SkDynamicMemoryWStream wstream;
  sk_sp<SkDocument> doc = MakePdfDocument(creator_, &wstream);

  for (const auto& page : pages) {
    SkCanvas* canvas = doc->beginPage(page.fSize.width(), page.fSize.height());
    canvas->drawPicture(page.fPicture);
    doc->endPage();
    if (docinfo_) {
      // Also collect this page into document PDF.
      SkCanvas* canvas_doc =
          docinfo_->doc->beginPage(page.fSize.width(), page.fSize.height());
      canvas_doc->drawPicture(page.fPicture);
      docinfo_->doc->endPage();
      docinfo_->pages_written++;
    }
  }
  doc->close();

  base::MappedReadOnlyRegion region_mapping =
      mojo::CreateReadOnlySharedMemoryRegion(wstream.bytesWritten());
  if (!region_mapping.IsValid()) {
    DLOG(ERROR) << "CompositeToPdf: Cannot create new shared memory region.";
    return mojom::PdfCompositor::Status::kHandleMapError;
  }

  wstream.copyToAndReset(region_mapping.mapping.memory());
  *region = std::move(region_mapping.region);
  return mojom::PdfCompositor::Status::kSuccess;
}

mojom::PdfCompositor::Status PdfCompositorImpl::CompleteDocumentToPdf(
    base::ReadOnlySharedMemoryRegion* region) {
  docinfo_->doc->close();

  base::MappedReadOnlyRegion region_mapping =
      mojo::CreateReadOnlySharedMemoryRegion(
          docinfo_->compositor_stream.bytesWritten());
  if (!region_mapping.IsValid()) {
    DLOG(ERROR)
        << "CompleteDocumentToPdf: Cannot create new shared memory region.";
    return mojom::PdfCompositor::Status::kHandleMapError;
  }

  docinfo_->compositor_stream.copyToAndReset(region_mapping.mapping.memory());
  *region = std::move(region_mapping.region);
  return mojom::PdfCompositor::Status::kSuccess;
}

void PdfCompositorImpl::CompositeSubframe(FrameInfo* frame_info) {
  frame_info->composited = true;

  // Composite subframes first.
  DeserializationContext subframes =
      GetDeserializationContext(frame_info->subframe_content_map);

  // Composite the entire frame.
  SkMemoryStream stream(frame_info->serialized_content.memory(),
                        frame_info->serialized_content.size());
  SkDeserialProcs procs = DeserializationProcs(&subframes);
  frame_info->content = SkPicture::MakeFromStream(&stream, &procs);
}

PdfCompositorImpl::DeserializationContext
PdfCompositorImpl::GetDeserializationContext(
    const ContentToFrameMap& subframe_content_map) {
  DeserializationContext subframes;
  for (auto& content_info : subframe_content_map) {
    uint32_t content_id = content_info.first;
    uint64_t frame_guid = content_info.second;
    auto iter = frame_info_map_.find(frame_guid);
    if (iter == frame_info_map_.end())
      continue;

    FrameInfo* frame_info = iter->second.get();
    if (!frame_info->composited)
      CompositeSubframe(frame_info);
    subframes[content_id] = frame_info->content;
  }
  return subframes;
}

void PdfCompositorImpl::FulfillRequest(
    base::ReadOnlySharedMemoryMapping serialized_content,
    const ContentToFrameMap& subframe_content_map,
    CompositeToPdfCallback callback) {
  base::ReadOnlySharedMemoryRegion region;
  auto status = CompositeToPdf(std::move(serialized_content),
                               subframe_content_map, &region);
  std::move(callback).Run(status, std::move(region));
}

void PdfCompositorImpl::CompleteDocumentRequest(
    CompleteDocumentToPdfCallback callback) {
  base::ReadOnlySharedMemoryRegion region;
  auto status = CompleteDocumentToPdf(&region);
  std::move(callback).Run(status, std::move(region));
}

PdfCompositorImpl::FrameContentInfo::FrameContentInfo(
    base::ReadOnlySharedMemoryMapping content,
    const ContentToFrameMap& map)
    : serialized_content(std::move(content)), subframe_content_map(map) {}

PdfCompositorImpl::FrameContentInfo::FrameContentInfo() = default;

PdfCompositorImpl::FrameContentInfo::~FrameContentInfo() = default;

PdfCompositorImpl::FrameInfo::FrameInfo() = default;

PdfCompositorImpl::FrameInfo::~FrameInfo() = default;

PdfCompositorImpl::DocumentInfo::DocumentInfo(const std::string& creator)
    : doc(MakePdfDocument(creator, &compositor_stream)) {}

PdfCompositorImpl::DocumentInfo::~DocumentInfo() = default;

PdfCompositorImpl::RequestInfo::RequestInfo(
    base::ReadOnlySharedMemoryMapping content,
    const ContentToFrameMap& content_info,
    const base::flat_set<uint64_t>& pending_subframes,
    mojom::PdfCompositor::CompositePageToPdfCallback callback)
    : FrameContentInfo(std::move(content), content_info),
      pending_subframes(pending_subframes),
      callback(std::move(callback)) {}

PdfCompositorImpl::RequestInfo::~RequestInfo() = default;

}  // namespace printing
