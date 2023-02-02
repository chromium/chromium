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
#if BUILDFLAG(ENABLE_TAGGED_PDF)
  void SetAccessibilityTree(
      const ui::AXTreeUpdate& accessibility_tree) override;
#endif
  void CompositePageToPdf(
      uint64_t frame_guid,
      base::ReadOnlySharedMemoryRegion serialized_content,
      const ContentToFrameMap& subframe_content_map,
      mojom::PrintCompositor::CompositePageToPdfCallback callback) override;
  void CompositeDocumentToPdf(
      uint64_t frame_guid,
      base::ReadOnlySharedMemoryRegion serialized_content,
      const ContentToFrameMap& subframe_content_map,
      mojom::PrintCompositor::CompositeDocumentToPdfCallback callback) override;
  void PrepareForDocumentToPdf(
      mojom::PrintCompositor::PrepareForDocumentToPdfCallback callback)
      override;
  void CompleteDocumentToPdf(
      uint32_t page_count,
      mojom::PrintCompositor::CompleteDocumentToPdfCallback callback) override;
  void SetWebContentsURL(const GURL& url) override;
  void SetUserAgent(const std::string& user_agent) override;

 protected:
  // This is the uniform underlying type for both
  // mojom::PrintCompositor::CompositePageToPdfCallback and
  // mojom::PrintCompositor::CompositeDocumentToPdfCallback.
  using CompositeToPdfCallback =
      base::OnceCallback<void(PrintCompositor::Status,
                              base::ReadOnlySharedMemoryRegion)>;

  using PrepareForDocumentToPdfCallback =
      base::OnceCallback<void(PrintCompositor::Status)>;
  using CompleteDocumentToPdfCallback =
      base::OnceCallback<void(PrintCompositor::Status,
                              base::ReadOnlySharedMemoryRegion)>;

  // The core function for content composition and conversion to a pdf file.
  // Make this function virtual so tests can override it.
  virtual mojom::PrintCompositor::Status CompositeToPdf(
      base::span<const uint8_t> serialized_content,
      const ContentToFrameMap& subframe_content_map,
      base::ReadOnlySharedMemoryRegion* region);

  // Make these functions virtual so tests can override them.
  virtual void FulfillRequest(base::span<const uint8_t> serialized_content,
                              const ContentToFrameMap& subframe_content_map,
                              CompositeToPdfCallback callback);
  virtual void CompleteDocumentRequest(CompleteDocumentToPdfCallback callback);

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
                CompositeToPdfCallback callback);
    ~RequestInfo();

    // All pending frame ids whose content is not available but needed
    // for composition.
    base::flat_set<uint64_t> pending_subframes;

    CompositeToPdfCallback callback;
    bool is_concurrent_doc_composition = false;
  };

  // Stores the concurrent document composition information.
  struct DocumentInfo {
    DocumentInfo();
    ~DocumentInfo();

    SkDynamicMemoryWStream compositor_stream;
    sk_sp<SkDocument> doc;
    uint32_t pages_provided = 0;
    uint32_t pages_written = 0;
    uint32_t page_count = 0;
    CompleteDocumentToPdfCallback callback;
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
      CompositeToPdfCallback callback);
  void HandleDocumentCompletionRequest();

  // Composite the content of a subframe.
  void CompositeSubframe(FrameInfo* frame_info);

  PictureDeserializationContext GetPictureDeserializationContext(
      const ContentToFrameMap& subframe_content_map);

  mojo::Receiver<mojom::PrintCompositor> receiver_{this};

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
  std::unique_ptr<DocumentInfo> docinfo_;

  // If present, the accessibility tree for the document needed to
  // export a tagged (accessible) PDF.
  ui::AXTreeUpdate accessibility_tree_;
};

}  // namespace printing

#endif  // COMPONENTS_SERVICES_PRINT_COMPOSITOR_PRINT_COMPOSITOR_IMPL_H_
