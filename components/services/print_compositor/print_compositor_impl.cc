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
#include "components/enterprise/buildflags/buildflags.h"
#include "components/services/print_compositor/public/cpp/print_service_mojo_types.h"
#include "content/public/utility/utility_thread.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "printing/common/metafile_utils.h"
#include "printing/mojom/print.mojom.h"
#include "skia/ext/font_utils.h"
#include "third_party/blink/public/platform/web_image_generator.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkDocument.h"
#include "third_party/skia/include/core/SkGraphics.h"
#include "third_party/skia/include/core/SkSerialProcs.h"
#include "third_party/skia/include/docs/SkMultiPictureDocument.h"
#include "ui/accessibility/ax_tree_update.h"

#if BUILDFLAG(IS_WIN)
#include "content/public/child/dwrite_font_proxy_init_win.h"
#include "printing/backend/win_helper.h"  // nogncheck
#elif BUILDFLAG(IS_APPLE)
#include "third_party/blink/public/platform/platform.h"
#include "third_party/skia/include/core/SkFontMgr.h"
#elif BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_ANDROID)
#include "third_party/blink/public/platform/platform.h"
#endif

#if BUILDFLAG(ENTERPRISE_WATERMARK)
#include "components/enterprise/watermarking/features.h"  // nogncheck
#include "components/enterprise/watermarking/watermark.h"  // nogncheck
#endif

using MojoDiscardableSharedMemoryManager =
    discardable_memory::mojom::DiscardableSharedMemoryManager;

namespace printing {

namespace {

#if BUILDFLAG(ENTERPRISE_WATERMARK)
// Print UX requirement for watermarking. Values are in pixels.
constexpr int kWatermarkBlockWidth = 350;
constexpr float kTextSize = 24.0f;
#endif

sk_sp<SkDocument> MakeDocument(
    const std::string& creator,
    const std::string& title,
    ui::AXTreeUpdate* accessibility_tree,
    mojom::GenerateDocumentOutline generate_document_outline,
    mojom::PrintCompositor::DocumentType document_type,
    SkWStream& stream) {
#if BUILDFLAG(IS_WIN)
  if (document_type == mojom::PrintCompositor::DocumentType::kXPS) {
    return MakeXpsDocument(&stream);
  }
#endif
  CHECK_EQ(document_type, mojom::PrintCompositor::DocumentType::kPDF);
  return MakePdfDocument(
      creator, title,
      accessibility_tree ? *accessibility_tree : ui::AXTreeUpdate(),
      generate_document_outline, &stream);
}

#if BUILDFLAG(ENTERPRISE_WATERMARK)
void DrawEnterpriseWatermark(SkCanvas* canvas, SkSize size) {
  if (!base::FeatureList::IsEnabled(
          enterprise_watermark::features::kEnablePrintWatermark)) {
    return;
  }

  // TODO(b/356446812): For now, use this hard-coded string to facilitate
  // implementing UI tests. We must update the PrintCompositor mojo interface in
  // order to pass the watermark string here from the browser process.
  std::string text = "Private! Confidential!\n2024-05-24\nexample@gmail.com";
  enterprise_watermark::DrawWatermark(canvas, size, text, kWatermarkBlockWidth,
                                      kTextSize);
}
#endif

void DrawPage(SkDocument* doc, const SkDocumentPage& page) {
  SkCanvas* canvas = doc->beginPage(page.fSize.width(), page.fSize.height());
  canvas->drawPicture(page.fPicture);
#if BUILDFLAG(ENTERPRISE_WATERMARK)
  DrawEnterpriseWatermark(canvas, page.fSize);
#endif
  doc->endPage();
}

}  // namespace

PrintCompositorImpl::PrintCompositorImpl(
    mojo::PendingReceiver<mojom::PrintCompositor> receiver,
    bool initialize_environment,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner)
    : io_task_runner_(std::move(io_task_runner)) {
  if (receiver) {
    receiver_.Bind(std::move(receiver));

    mojo::PendingRemote<MojoDiscardableSharedMemoryManager> manager_remote;
    content::ChildThread::Get()->BindHostReceiver(
        manager_remote.InitWithNewPipeAndPassReceiver());
    DCHECK(io_task_runner_);
    discardable_shared_memory_manager_ = base::MakeRefCounted<
        discardable_memory::ClientDiscardableSharedMemoryManager>(
        std::move(manager_remote), io_task_runner_);
    base::DiscardableMemoryAllocator::SetInstance(
        discardable_shared_memory_manager_.get());
  }

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
  DCHECK(skia::DefaultFontMgr()->countFamilies());
#endif
}

PrintCompositorImpl::~PrintCompositorImpl() {
#if BUILDFLAG(IS_WIN)
  content::UninitializeDWriteFontProxy();
#endif
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

void PrintCompositorImpl::SetAccessibilityTree(
    const ui::AXTreeUpdate& accessibility_tree) {
  accessibility_tree_ = accessibility_tree;
}

void PrintCompositorImpl::CompositePage(
    uint64_t frame_guid,
    base::ReadOnlySharedMemoryRegion serialized_content,
    const ContentToFrameMap& subframe_content_map,
    mojom::PrintCompositor::CompositePageCallback callback) {
  TRACE_EVENT0("print", "PrintCompositorImpl::CompositePage");
  // This function is always called to composite a page to PDF.
  HandleCompositionRequest(
      frame_guid, std::move(serialized_content), subframe_content_map,
      mojom::PrintCompositor::DocumentType::kPDF, std::move(callback));
}

void PrintCompositorImpl::CompositeDocument(
    uint64_t frame_guid,
    base::ReadOnlySharedMemoryRegion serialized_content,
    const ContentToFrameMap& subframe_content_map,
    mojom::PrintCompositor::DocumentType document_type,
    mojom::PrintCompositor::CompositeDocumentCallback callback) {
  TRACE_EVENT0("print", "PrintCompositorImpl::CompositeDocument");
  CHECK(!doc_info_);
  HandleCompositionRequest(frame_guid, std::move(serialized_content),
                           subframe_content_map, document_type,
                           std::move(callback));
}

void PrintCompositorImpl::PrepareToCompositeDocument(
    mojom::PrintCompositor::DocumentType document_type,
    mojom::PrintCompositor::PrepareToCompositeDocumentCallback callback) {
  CHECK(!doc_info_);
#if BUILDFLAG(IS_WIN)
  if (document_type == mojom::PrintCompositor::DocumentType::kXPS) {
    xps_initializer_ = std::make_unique<ScopedXPSInitializer>();
  }
#endif
  doc_info_ = std::make_unique<DocumentInfo>(document_type);
  std::move(callback).Run(mojom::PrintCompositor::Status::kSuccess);
}

void PrintCompositorImpl::FinishDocumentComposition(
    uint32_t page_count,
    mojom::PrintCompositor::FinishDocumentCompositionCallback callback) {
  CHECK(doc_info_);
  DCHECK_GT(page_count, 0U);
  doc_info_->page_count = page_count;
  doc_info_->callback = std::move(callback);

  if (!doc_info_->doc) {
    doc_info_->doc = MakeDocument(
        creator_, title_, &accessibility_tree_, generate_document_outline_,
        doc_info_->document_type, doc_info_->compositor_stream);
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
                   request->document_type, std::move(request->callback));

    // Check for a collected print preview document that was waiting on
    // this page to finish.
    if (doc_info_) {
      if (doc_info_->page_count &&
          (doc_info_->pages_written == doc_info_->page_count)) {
        FinishDocumentRequest(std::move(doc_info_->callback));
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
    mojom::PrintCompositor::DocumentType document_type,
    CompositePagesCallback callback) {
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
                     document_type, std::move(callback));
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
      std::move(pending_subframes), document_type, std::move(callback)));
}

void PrintCompositorImpl::HandleDocumentCompletionRequest() {
  if (doc_info_->pages_written == doc_info_->page_count) {
    FinishDocumentRequest(std::move(doc_info_->callback));
    return;
  }
  // Just need to wait on pages to percolate through processing, callback will
  // be handled from UpdateRequestsWithSubframeInfo() once the pending requests
  // have finished.
}

mojom::PrintCompositor::Status PrintCompositorImpl::CompositePages(
    base::span<const uint8_t> serialized_content,
    const ContentToFrameMap& subframe_content_map,
    base::ReadOnlySharedMemoryRegion* region,
    mojom::PrintCompositor::DocumentType document_type) {
  TRACE_EVENT0("print", "PrintCompositorImpl::CompositePages");

  PictureDeserializationContext subframes =
      GetPictureDeserializationContext(subframe_content_map);

  // Read in content and convert it into pdf.
  SkMemoryStream stream(serialized_content.data(), serialized_content.size());
  int page_count = SkMultiPictureDocument::ReadPageCount(&stream);
  if (!page_count) {
    DLOG(ERROR) << "CompositePages: No page is read.";
    return mojom::PrintCompositor::Status::kContentFormatError;
  }

  std::vector<SkDocumentPage> pages(page_count);
  SkDeserialProcs procs = DeserializationProcs(&subframes, &typefaces_);
  if (!SkMultiPictureDocument::Read(&stream, pages.data(), page_count,
                                    &procs)) {
    DLOG(ERROR) << "CompositePages: Page reading failed.";
    return mojom::PrintCompositor::Status::kContentFormatError;
  }

  // Create PDF document providing accessibility data early if concurrent
  // document composition is not in effect, i.e. when handling
  // CompositeDocumentToPdf() call.
  SkDynamicMemoryWStream wstream;
  sk_sp<SkDocument> doc =
      MakeDocument(creator_, title_, doc_info_ ? nullptr : &accessibility_tree_,
                   generate_document_outline_, document_type, wstream);

  if (doc_info_) {
    // Create full document if needed.
    if (!doc_info_->doc) {
      doc_info_->doc = MakeDocument(
          creator_, title_, &accessibility_tree_, generate_document_outline_,
          doc_info_->document_type, doc_info_->compositor_stream);
    }
  }

  for (const auto& page : pages) {
    TRACE_EVENT0("print", "PrintCompositorImpl::CompositePages draw page");
    DrawPage(doc.get(), page);

    if (doc_info_) {
      // Optionally draw this page into the full document in `doc_info_` as
      // well.
      DrawPage(doc_info_->doc.get(), page);
      doc_info_->pages_written++;
    }
  }
  doc->close();

  base::MappedReadOnlyRegion region_mapping =
      base::ReadOnlySharedMemoryRegion::Create(wstream.bytesWritten());
  if (!region_mapping.IsValid()) {
    DLOG(ERROR) << "CompositePages: Cannot create new shared memory region.";
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
    mojom::PrintCompositor::DocumentType document_type,
    CompositePagesCallback callback) {
  base::ReadOnlySharedMemoryRegion region;
  auto status = CompositePages(serialized_content, subframe_content_map,
                               &region, document_type);
  std::move(callback).Run(status, std::move(region));
}

void PrintCompositorImpl::FinishDocumentRequest(
    FinishDocumentCompositionCallback callback) {
  mojom::PrintCompositor::Status status;
  base::ReadOnlySharedMemoryRegion region;

  doc_info_->doc->close();

  base::MappedReadOnlyRegion region_mapping =
      base::ReadOnlySharedMemoryRegion::Create(
          doc_info_->compositor_stream.bytesWritten());
  if (region_mapping.IsValid()) {
    doc_info_->compositor_stream.copyToAndReset(
        region_mapping.mapping.memory());
    region = std::move(region_mapping.region);
    status = mojom::PrintCompositor::Status::kSuccess;
  } else {
    DLOG(ERROR) << "FinishDocumentRequest: "
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

// TODO(crbug.com/40100562) Make use of `document_type` parameter once
// `MakeXpsDocument()` is available.
PrintCompositorImpl::DocumentInfo::DocumentInfo(
    mojom::PrintCompositor::DocumentType document_type)
    : document_type(document_type) {}

PrintCompositorImpl::DocumentInfo::~DocumentInfo() = default;

PrintCompositorImpl::RequestInfo::RequestInfo(
    base::span<const uint8_t> content,
    const ContentToFrameMap& content_info,
    const base::flat_set<uint64_t>& pending_subframes,
    mojom::PrintCompositor::DocumentType document_type,
    mojom::PrintCompositor::CompositePageCallback callback)
    : FrameContentInfo(content, content_info),
      pending_subframes(pending_subframes),
      document_type(document_type),
      callback(std::move(callback)) {}

PrintCompositorImpl::RequestInfo::~RequestInfo() = default;

void PrintCompositorImpl::SetGenerateDocumentOutline(
    mojom::GenerateDocumentOutline generate_document_outline) {
  generate_document_outline_ = generate_document_outline;
}

void PrintCompositorImpl::SetTitle(const std::string& title) {
  title_ = title;
}

}  // namespace printing
