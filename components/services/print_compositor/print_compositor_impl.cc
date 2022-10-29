// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/print_compositor/print_compositor_impl.h"

#include <tuple>
#include <utility>

#include "base/containers/contains.h"
#include "base/logging.h"
#include "base/memory/discardable_memory.h"
#include "base/ranges/algorithm.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "components/crash/core/common/crash_key.h"
#include "components/discardable_memory/client/client_discardable_shared_memory_manager.h"
#include "components/services/print_compositor/public/cpp/print_service_mojo_types.h"
#include "content/public/utility/utility_thread.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "printing/common/metafile_utils.h"
#include "third_party/blink/public/platform/web_image_generator.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkDocument.h"
#include "third_party/skia/include/core/SkGraphics.h"
#include "third_party/skia/include/core/SkSerialProcs.h"
#include "third_party/skia/src/utils/SkMultiPictureDocument.h"
#include "ui/accessibility/ax_tree_update.h"

#if BUILDFLAG(IS_WIN)
#include "content/public/child/dwrite_font_proxy_init_win.h"
#elif BUILDFLAG(IS_APPLE)
#include "third_party/blink/public/platform/platform.h"
#include "third_party/skia/include/core/SkFontMgr.h"
#elif BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_ANDROID)
#include "third_party/blink/public/platform/platform.h"
#endif

namespace printing {

PrintCompositorImpl::PrintCompositorImpl(
    mojo::PendingReceiver<mojom::PrintCompositor> receiver,
    bool initialize_environment,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner)
    : io_task_runner_(std::move(io_task_runner)) {
  if (receiver)
    receiver_.Bind(std::move(receiver));

  if (!initialize_environment)
    return;

#if BUILDFLAG(IS_WIN)
  // Initialize direct write font proxy so skia can use it.
  content::InitializeDWriteFontProxy();
#endif

  // Hook up blink's codecs so skia can call them.
  SkGraphics::SetImageGeneratorFromEncodedDataFactory(
      blink::WebImageGenerator::CreateAsSkImageGenerator);

#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_ANDROID)
  content::UtilityThread::Get()->EnsureBlinkInitializedWithSandboxSupport();
  // Check that we have sandbox support on this platform.
  DCHECK(blink::Platform::Current()->GetSandboxSupport());
#else
  content::UtilityThread::Get()->EnsureBlinkInitialized();
#endif

#if BUILDFLAG(IS_APPLE)
  // Check that font access is granted.
  // This doesn't do comprehensive tests to make sure fonts can work properly.
  // It is just a quick and simple check to catch things like improper sandbox
  // policy setup.
  DCHECK(SkFontMgr::RefDefault()->countFamilies());
#endif
}

PrintCompositorImpl::~PrintCompositorImpl() {
#if BUILDFLAG(IS_WIN)
  content::UninitializeDWriteFontProxy();
#endif
}

void PrintCompositorImpl::SetDiscardableSharedMemoryManager(
    mojo::PendingRemote<
        discardable_memory::mojom::DiscardableSharedMemoryManager> manager) {
  // Set up discardable memory manager.
  mojo::PendingRemote<discardable_memory::mojom::DiscardableSharedMemoryManager>
      manager_remote(std::move(manager));
  discardable_shared_memory_manager_ = base::MakeRefCounted<
      discardable_memory::ClientDiscardableSharedMemoryManager>(
      std::move(manager_remote), io_task_runner_);
  base::DiscardableMemoryAllocator::SetInstance(
      discardable_shared_memory_manager_.get());
}

void PrintCompositorImpl::NotifyUnavailableSubframe(uint64_t frame_guid) {
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

void PrintCompositorImpl::AddSubframeContent(
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
  frame_info_map_.emplace(frame_guid, std::make_unique<FrameInfo>(
                                          mapping.GetMemoryAsSpan<uint8_t>(),
                                          subframe_content_map));

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

#if BUILDFLAG(ENABLE_TAGGED_PDF)
void PrintCompositorImpl::SetAccessibilityTree(
    const ui::AXTreeUpdate& accessibility_tree) {
  accessibility_tree_ = accessibility_tree;
}
#endif

void PrintCompositorImpl::CompositePageToPdf(
    uint64_t frame_guid,
    base::ReadOnlySharedMemoryRegion serialized_content,
    const ContentToFrameMap& subframe_content_map,
    mojom::PrintCompositor::CompositePageToPdfCallback callback) {
  TRACE_EVENT0("print", "PrintCompositorImpl::CompositePageToPdf");
  if (docinfo_)
    docinfo_->pages_provided++;
  HandleCompositionRequest(frame_guid, std::move(serialized_content),
                           subframe_content_map, std::move(callback));
}

void PrintCompositorImpl::CompositeDocumentToPdf(
    uint64_t frame_guid,
    base::ReadOnlySharedMemoryRegion serialized_content,
    const ContentToFrameMap& subframe_content_map,
    mojom::PrintCompositor::CompositeDocumentToPdfCallback callback) {
  TRACE_EVENT0("print", "PrintCompositorImpl::CompositeDocumentToPdf");
  DCHECK(!docinfo_);
  HandleCompositionRequest(frame_guid, std::move(serialized_content),
                           subframe_content_map, std::move(callback));
}

void PrintCompositorImpl::PrepareForDocumentToPdf(
    mojom::PrintCompositor::PrepareForDocumentToPdfCallback callback) {
  DCHECK(!docinfo_);
  docinfo_ = std::make_unique<DocumentInfo>();
  std::move(callback).Run(mojom::PrintCompositor::Status::kSuccess);
}

void PrintCompositorImpl::CompleteDocumentToPdf(
    uint32_t page_count,
    mojom::PrintCompositor::CompleteDocumentToPdfCallback callback) {
  DCHECK(docinfo_);
  DCHECK_GT(page_count, 0U);
  docinfo_->page_count = page_count;
  docinfo_->callback = std::move(callback);

  if (!docinfo_->doc) {
    docinfo_->doc = MakePdfDocument(creator_, accessibility_tree_,
                                    &docinfo_->compositor_stream);
  }

  HandleDocumentCompletionRequest();
}

void PrintCompositorImpl::SetWebContentsURL(const GURL& url) {
  // Record the most recent url we tried to print. This should be sufficient
  // for users using print preview by default.
  static crash_reporter::CrashKeyString<1024> crash_key("main-frame-url");
  crash_key.Set(url.spec());
}

void PrintCompositorImpl::SetUserAgent(const std::string& user_agent) {
  if (!user_agent.empty())
    creator_ = user_agent;
}

void PrintCompositorImpl::UpdateRequestsWithSubframeInfo(
    uint64_t frame_guid,
    const std::vector<uint64_t>& pending_subframes) {
  // Check for each request's pending list.
  for (auto it = requests_.begin(); it != requests_.end();) {
    auto& request = *it;
    // If the request needs this frame, we can remove the dependency, but
    // update with this frame's pending list.
    auto& pending_list = request->pending_subframes;
    if (pending_list.erase(frame_guid)) {
      base::ranges::copy(pending_subframes,
                         std::inserter(pending_list, pending_list.end()));
    }

    // If the request still has pending frames, or isn't at the front of the
    // request queue (and thus could be dependent upon content from their
    // data stream), then keep waiting.
    const bool fulfill_request =
        it == requests_.begin() && pending_list.empty();
    if (!fulfill_request) {
      ++it;
      continue;
    }

    // Fulfill the request now.
    FulfillRequest(request->serialized_content, request->subframe_content_map,
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
  }
}

bool PrintCompositorImpl::IsReadyToComposite(
    uint64_t frame_guid,
    const ContentToFrameMap& subframe_content_map,
    base::flat_set<uint64_t>* pending_subframes) const {
  pending_subframes->clear();
  base::flat_set<uint64_t> visited_frames = {frame_guid};
  CheckFramesForReadiness(subframe_content_map, pending_subframes,
                          &visited_frames);
  return pending_subframes->empty();
}

void PrintCompositorImpl::CheckFramesForReadiness(
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

void PrintCompositorImpl::HandleCompositionRequest(
    uint64_t frame_guid,
    base::ReadOnlySharedMemoryRegion serialized_content,
    const ContentToFrameMap& subframe_content_map,
    CompositeToPdfCallback callback) {
  base::ReadOnlySharedMemoryMapping mapping = serialized_content.Map();
  if (!mapping.IsValid()) {
    DLOG(ERROR) << "HandleCompositionRequest: Cannot map input.";
    std::move(callback).Run(mojom::PrintCompositor::Status::kHandleMapError,
                            base::ReadOnlySharedMemoryRegion());
    return;
  }

  base::flat_set<uint64_t> pending_subframes;
  if (IsReadyToComposite(frame_guid, subframe_content_map,
                         &pending_subframes)) {
    // This request has all the necessary subframes.
    // Due to typeface serialization caching, need to ensure that previous
    // requests have already been processed, otherwise this request could
    // fail by trying to use a typeface which hasn't been deserialized yet.
    if (requests_.empty()) {
      FulfillRequest(mapping.GetMemoryAsSpan<uint8_t>(), subframe_content_map,
                     std::move(callback));
      return;
    }
  }

  // When it is not ready yet, keep its information and
  // wait until all dependent subframes are ready.
  auto iter = frame_info_map_.find(frame_guid);
  if (iter == frame_info_map_.end())
    frame_info_map_[frame_guid] = std::make_unique<FrameInfo>();

  requests_.push_back(std::make_unique<RequestInfo>(
      mapping.GetMemoryAsSpan<uint8_t>(), subframe_content_map,
      std::move(pending_subframes), std::move(callback)));
}

void PrintCompositorImpl::HandleDocumentCompletionRequest() {
  if (docinfo_->pages_written == docinfo_->page_count) {
    CompleteDocumentRequest(std::move(docinfo_->callback));
    return;
  }
  // Just need to wait on pages to percolate through processing, callback will
  // be handled from UpdateRequestsWithSubframeInfo() once the pending requests
  // have finished.
}

mojom::PrintCompositor::Status PrintCompositorImpl::CompositeToPdf(
    base::span<const uint8_t> serialized_content,
    const ContentToFrameMap& subframe_content_map,
    base::ReadOnlySharedMemoryRegion* region) {
  TRACE_EVENT0("print", "PrintCompositorImpl::CompositeToPdf");

  PictureDeserializationContext subframes =
      GetPictureDeserializationContext(subframe_content_map);

  // Read in content and convert it into pdf.
  SkMemoryStream stream(serialized_content.data(), serialized_content.size());
  int page_count = SkMultiPictureDocumentReadPageCount(&stream);
  if (!page_count) {
    DLOG(ERROR) << "CompositeToPdf: No page is read.";
    return mojom::PrintCompositor::Status::kContentFormatError;
  }

  std::vector<SkDocumentPage> pages(page_count);
  SkDeserialProcs procs = DeserializationProcs(&subframes, &typefaces_);
  if (!SkMultiPictureDocumentRead(&stream, pages.data(), page_count, &procs)) {
    DLOG(ERROR) << "CompositeToPdf: Page reading failed.";
    return mojom::PrintCompositor::Status::kContentFormatError;
  }

  SkDynamicMemoryWStream wstream;
  sk_sp<SkDocument> doc =
      MakePdfDocument(creator_, ui::AXTreeUpdate(), &wstream);

  for (const auto& page : pages) {
    TRACE_EVENT0("print", "PrintCompositorImpl::CompositeToPdf draw page");
    SkCanvas* canvas = doc->beginPage(page.fSize.width(), page.fSize.height());
    canvas->drawPicture(page.fPicture);
    doc->endPage();
    if (docinfo_) {
      // Create document PDF if needed.
      if (!docinfo_->doc) {
        docinfo_->doc = MakePdfDocument(creator_, accessibility_tree_,
                                        &docinfo_->compositor_stream);
      }

      // Collect this page into document PDF.
      SkCanvas* canvas_doc =
          docinfo_->doc->beginPage(page.fSize.width(), page.fSize.height());
      canvas_doc->drawPicture(page.fPicture);
      docinfo_->doc->endPage();
      docinfo_->pages_written++;
    }
  }
  doc->close();

  base::MappedReadOnlyRegion region_mapping =
      base::ReadOnlySharedMemoryRegion::Create(wstream.bytesWritten());
  if (!region_mapping.IsValid()) {
    DLOG(ERROR) << "CompositeToPdf: Cannot create new shared memory region.";
    return mojom::PrintCompositor::Status::kHandleMapError;
  }

  wstream.copyToAndReset(region_mapping.mapping.memory());
  *region = std::move(region_mapping.region);
  return mojom::PrintCompositor::Status::kSuccess;
}

void PrintCompositorImpl::CompositeSubframe(FrameInfo* frame_info) {
  frame_info->composited = true;

  // Composite subframes first.
  PictureDeserializationContext subframes =
      GetPictureDeserializationContext(frame_info->subframe_content_map);

  // Composite the entire frame.
  SkMemoryStream stream(frame_info->serialized_content.data(),
                        frame_info->serialized_content.size());
  SkDeserialProcs procs =
      DeserializationProcs(&subframes, &frame_info->typefaces);
  frame_info->content = SkPicture::MakeFromStream(&stream, &procs);
}

PrintCompositorImpl::PictureDeserializationContext
PrintCompositorImpl::GetPictureDeserializationContext(
    const ContentToFrameMap& subframe_content_map) {
  PictureDeserializationContext subframes;
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

void PrintCompositorImpl::FulfillRequest(
    base::span<const uint8_t> serialized_content,
    const ContentToFrameMap& subframe_content_map,
    CompositeToPdfCallback callback) {
  base::ReadOnlySharedMemoryRegion region;
  auto status =
      CompositeToPdf(serialized_content, subframe_content_map, &region);
  std::move(callback).Run(status, std::move(region));
}

void PrintCompositorImpl::CompleteDocumentRequest(
    CompleteDocumentToPdfCallback callback) {
  mojom::PrintCompositor::Status status;
  base::ReadOnlySharedMemoryRegion region;

  docinfo_->doc->close();

  base::MappedReadOnlyRegion region_mapping =
      base::ReadOnlySharedMemoryRegion::Create(
          docinfo_->compositor_stream.bytesWritten());
  if (region_mapping.IsValid()) {
    docinfo_->compositor_stream.copyToAndReset(region_mapping.mapping.memory());
    region = std::move(region_mapping.region);
    status = mojom::PrintCompositor::Status::kSuccess;
  } else {
    DLOG(ERROR) << "CompleteDocumentRequest: "
                << "Cannot create new shared memory region.";
    status = mojom::PrintCompositor::Status::kHandleMapError;
  }

  std::move(callback).Run(status, std::move(region));
}

PrintCompositorImpl::FrameContentInfo::FrameContentInfo(
    base::span<const uint8_t> content,
    const ContentToFrameMap& map)
    : serialized_content(content.begin(), content.end()),
      subframe_content_map(map) {}

PrintCompositorImpl::FrameContentInfo::FrameContentInfo() = default;

PrintCompositorImpl::FrameContentInfo::~FrameContentInfo() = default;

PrintCompositorImpl::DocumentInfo::DocumentInfo() = default;

PrintCompositorImpl::DocumentInfo::~DocumentInfo() = default;

PrintCompositorImpl::RequestInfo::RequestInfo(
    base::span<const uint8_t> content,
    const ContentToFrameMap& content_info,
    const base::flat_set<uint64_t>& pending_subframes,
    mojom::PrintCompositor::CompositePageToPdfCallback callback)
    : FrameContentInfo(content, content_info),
      pending_subframes(pending_subframes),
      callback(std::move(callback)) {}

PrintCompositorImpl::RequestInfo::~RequestInfo() = default;

}  // namespace printing
