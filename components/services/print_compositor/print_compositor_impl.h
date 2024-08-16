// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_PRINT_COMPOSITOR_PRINT_COMPOSITOR_IMPL_H_
#define COMPONENTS_SERVICES_PRINT_COMPOSITOR_PRINT_COMPOSITOR_IMPL_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/containers/span.h"
#include "base/gtest_prod_util.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"
#include "build/build_config.h"
#include "components/services/print_compositor/public/cpp/print_service_mojo_types.h"
#include "components/services/print_compositor/public/mojom/print_compositor.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "printing/buildflags/buildflags.h"
#include "third_party/skia/include/core/SkPicture.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/core/SkStream.h"
#include "third_party/skia/include/core/SkTypeface.h"
#include "ui/accessibility/ax_tree_update.h"

class SkDocument;

namespace base {
class SingleThreadTaskRunner;
}

namespace discardable_memory {
class ClientDiscardableSharedMemoryManager;
}

namespace printing {

#if BUILDFLAG(IS_WIN)
class ScopedXPSInitializer;
#endif

class PrintCompositorImpl : public mojom::PrintCompositor {
 public:
  // Creates an instance with an optional Mojo receiver (may be null) and
  // optional initialization of the runtime environment necessary for
  // compositing operations. `io_task_runner` is used for shared memory
  // management, if and only if there is a receiver, which may not be the case
  // in unit tests. In practice, `initialize_environment` is only false in unit
  // tests.
  PrintCompositorImpl(
      mojo::PendingReceiver<mojom::PrintCompositor> receiver,
      bool initialize_environment,
      scoped_refptr<base::SingleThreadTaskRunner> io_task_runner);

  PrintCompositorImpl(const PrintCompositorImpl&) = delete;
  PrintCompositorImpl& operator=(const PrintCompositorImpl&) = delete;

  ~PrintCompositorImpl() override;

  // mojom::PrintCompositor
  void NotifyUnavailableSubframe(uint64_t frame_guid) override;
  void AddSubframeContent(
      uint64_t frame_guid,
      base::ReadOnlySharedMemoryRegion serialized_content,
      const ContentToFrameMap& subframe_content_map) override;
  void SetAccessibilityTree(
      const ui::AXTreeUpdate& accessibility_tree) override;
  void CompositePage(
      uint64_t frame_guid,
      base::ReadOnlySharedMemoryRegion serialized_content,
      const ContentToFrameMap& subframe_content_map,
      mojom::PrintCompositor::CompositePageCallback callback) override;
  void CompositeDocument(
      uint64_t frame_guid,
      base::ReadOnlySharedMemoryRegion serialized_content,
      const ContentToFrameMap& subframe_content_map,
      mojom::PrintCompositor::DocumentType document_type,
      mojom::PrintCompositor::CompositeDocumentCallback callback) override;
  void PrepareToCompositeDocument(
      mojom::PrintCompositor::DocumentType document_type,
      mojom::PrintCompositor::PrepareToCompositeDocumentCallback callback)
      override;
  void FinishDocumentComposition(
      uint32_t page_count,
      mojom::PrintCompositor::FinishDocumentCompositionCallback callback)
      override;
  void SetWebContentsURL(const GURL& url) override;
  void SetUserAgent(const std::string& user_agent) override;
  void SetGenerateDocumentOutline(
      mojom::GenerateDocumentOutline generate_document_outline) override;
  void SetTitle(const std::string& title) override;

 protected:
  // This is the uniform underlying type for both
  // mojom::PrintCompositor::CompositePageCallback and
  // mojom::PrintCompositor::CompositeDocumentCallback.
  using CompositePagesCallback =
      base::OnceCallback<void(PrintCompositor::Status,
                              base::ReadOnlySharedMemoryRegion)>;

  using PrepareForDocumentCompositionCallback =
      base::OnceCallback<void(PrintCompositor::Status)>;
  using FinishDocumentCompositionCallback =
      base::OnceCallback<void(PrintCompositor::Status,
                              base::ReadOnlySharedMemoryRegion)>;

  // The core function for content composition and conversion to a PDF file,
  // and possibly also into a full document PDF/XPS file.
  // Make this function virtual so tests can override it.
  virtual mojom::PrintCompositor::Status CompositePages(
      base::span<const uint8_t> serialized_content,
      const ContentToFrameMap& subframe_content_map,
      base::ReadOnlySharedMemoryRegion* region,
      mojom::PrintCompositor::DocumentType document_type);

  // Make these functions virtual so tests can override them.
  virtual void FulfillRequest(
      base::span<const uint8_t> serialized_content,
      const ContentToFrameMap& subframe_content_map,
      mojom::PrintCompositor::DocumentType document_type,
      CompositePagesCallback callback);
  virtual void FinishDocumentRequest(
      FinishDocumentCompositionCallback callback);

 private:
  FRIEND_TEST_ALL_PREFIXES(PrintCompositorImplTest, IsReadyToComposite);
  FRIEND_TEST_ALL_PREFIXES(PrintCompositorImplTest, MultiLayerDependency);
  FRIEND_TEST_ALL_PREFIXES(PrintCompositorImplTest, DependencyLoop);
  friend class MockCompletionPrintCompositorImpl;

  // The map needed during content deserialization. It stores the mapping
  // between content id and its actual content.
  using PictureDeserializationContext =
      base::flat_map<uint32_t, sk_sp<SkPicture>>;
  using TypefaceDeserializationContext =
      base::flat_map<uint32_t, sk_sp<SkTypeface>>;

  // Base structure to store a frame's content and its subframe
  // content information.
  struct FrameContentInfo {
    FrameContentInfo(base::span<const uint8_t> content,
                     const ContentToFrameMap& map);
    FrameContentInfo();
    ~FrameContentInfo();

    // Serialized SkPicture content of this frame.
    std::vector<uint8_t> serialized_content;

    // Frame content after composition with subframe content.
    sk_sp<SkPicture> content;

    // Subframe content id and its corresponding frame guid.
    ContentToFrameMap subframe_content_map;

    // Typefaces used within scope of this frame.
    TypefaceDeserializationContext typefaces;
  };

  // Other than content, it also stores the status during frame composition.
  struct FrameInfo : public FrameContentInfo {
    using FrameContentInfo::FrameContentInfo;

    // The following fields are used for storing composition status.
    // Set to true when this frame's |serialized_content| is composed with
    // subframe content and the final result is stored in |content|.
    bool composited = false;
  };

  // Stores the mapping between frame's global unique ids and their
  // corresponding frame information.
  using FrameMap = base::flat_map<uint64_t, std::unique_ptr<FrameInfo>>;

  // Stores the page or document's request information.
  struct RequestInfo : public FrameContentInfo {
    RequestInfo(base::span<const uint8_t> content,
                const ContentToFrameMap& content_info,
                const base::flat_set<uint64_t>& pending_subframes,
                mojom::PrintCompositor::DocumentType document_type,
                CompositePagesCallback callback);
    ~RequestInfo();

    // All pending frame ids whose content is not available but needed
    // for composition.
    base::flat_set<uint64_t> pending_subframes;

    mojom::PrintCompositor::DocumentType document_type;
    CompositePagesCallback callback;
  };

  // Stores the concurrent document composition information.
  //
  // While PrintCompositorImpl is creating a document for every page it is
  // compositing, it can reuse the same page info to concurrently create the
  // full document with all pages. Only used when PrepareToCompositeDocument()
  // gets called.
  struct DocumentInfo {
    explicit DocumentInfo(mojom::PrintCompositor::DocumentType document_type);
    ~DocumentInfo();

    SkDynamicMemoryWStream compositor_stream;
    sk_sp<SkDocument> doc;
    mojom::PrintCompositor::DocumentType document_type;
    uint32_t pages_written = 0;
    uint32_t page_count = 0;
    FinishDocumentCompositionCallback callback;
  };

  // Check whether any request is waiting for the specific subframe, if so,
  // update its dependecy with the subframe's pending child frames.
  void UpdateRequestsWithSubframeInfo(
      uint64_t frame_guid,
      const std::vector<uint64_t>& pending_subframes);

  // Check whether the frame with a list of subframe content is ready to
  // composite. If not, return all unavailable frames' ids in
  // |pending_subframes|.
  bool IsReadyToComposite(uint64_t frame_guid,
                          const ContentToFrameMap& subframe_content_map,
                          base::flat_set<uint64_t>* pending_subframes) const;

  // Recursively check all the subframes in |subframe_content_map| and put those
  // not ready in |pending_subframes|.
  void CheckFramesForReadiness(const ContentToFrameMap& subframe_content_map,
                               base::flat_set<uint64_t>* pending_subframes,
                               base::flat_set<uint64_t>* visited) const;

  // The internal implementation for handling page and documentation composition
  // requests.
  void HandleCompositionRequest(
      uint64_t frame_guid,
      base::ReadOnlySharedMemoryRegion serialized_content,
      const ContentToFrameMap& subframe_content_ids,
      mojom::PrintCompositor::DocumentType document_type,
      CompositePagesCallback callback);
  void HandleDocumentCompletionRequest();

  // Composite the content of a subframe.
  void CompositeSubframe(FrameInfo* frame_info);

  PictureDeserializationContext GetPictureDeserializationContext(
      const ContentToFrameMap& subframe_content_map);

  mojo::Receiver<mojom::PrintCompositor> receiver_{this};

#if BUILDFLAG(IS_WIN)
  std::unique_ptr<ScopedXPSInitializer> xps_initializer_;
#endif

  const scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;
  scoped_refptr<discardable_memory::ClientDiscardableSharedMemoryManager>
      discardable_shared_memory_manager_;

  // The creator of this service.
  // Currently contains the service creator's user agent string if given,
  // otherwise just use string "Chromium".
  std::string creator_ = "Chromium";

  // Keep track of all frames' information indexed by frame id.
  FrameMap frame_info_map_;

  // Context for dealing with all typefaces encountered across multiple pages.
  TypefaceDeserializationContext typefaces_;

  std::vector<std::unique_ptr<RequestInfo>> requests_;
  std::unique_ptr<DocumentInfo> doc_info_;

  // If present, the accessibility tree for the document needed to
  // export a tagged (accessible) PDF.
  ui::AXTreeUpdate accessibility_tree_;

  // How (or if) to generate a document outline.
  mojom::GenerateDocumentOutline generate_document_outline_ =
      mojom::GenerateDocumentOutline::kNone;

  // The title of the document.
  std::string title_;
};

}  // namespace printing

#endif  // COMPONENTS_SERVICES_PRINT_COMPOSITOR_PRINT_COMPOSITOR_IMPL_H_
