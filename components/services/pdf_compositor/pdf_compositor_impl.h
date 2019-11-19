// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_PDF_COMPOSITOR_PDF_COMPOSITOR_IMPL_H_
#define COMPONENTS_SERVICES_PDF_COMPOSITOR_PDF_COMPOSITOR_IMPL_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/macros.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"
#include "base/optional.h"
#include "components/services/pdf_compositor/public/cpp/pdf_service_mojo_types.h"
#include "components/services/pdf_compositor/public/mojom/pdf_compositor.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "third_party/skia/include/core/SkPicture.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/core/SkStream.h"

class SkDocument;

namespace base {
class SingleThreadTaskRunner;
}

namespace discardable_memory {
class ClientDiscardableSharedMemoryManager;
}

namespace printing {

class PdfCompositorImpl : public mojom::PdfCompositor {
 public:
  // Creates an instance with an optional Mojo receiver (may be null) and
  // optional initialization of the runtime environment necessary for
  // compositing operations. |io_task_runner| is used for shared memory
  // management, if and only if |SetDiscardableSharedMemoryManager()| is
  // eventually called, which may not be the case in unit tests. In practice,
  // |initialize_environment| is only false in unit tests.
  PdfCompositorImpl(mojo::PendingReceiver<mojom::PdfCompositor> receiver,
                    bool initialize_environment,
                    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner);
  ~PdfCompositorImpl() override;

  // mojom::PdfCompositor
  void SetDiscardableSharedMemoryManager(
      mojo::PendingRemote<
          discardable_memory::mojom::DiscardableSharedMemoryManager> manager)
      override;
  void NotifyUnavailableSubframe(uint64_t frame_guid) override;
  void AddSubframeContent(
      uint64_t frame_guid,
      base::ReadOnlySharedMemoryRegion serialized_content,
      const ContentToFrameMap& subframe_content_map) override;
  void CompositePageToPdf(
      uint64_t frame_guid,
      base::ReadOnlySharedMemoryRegion serialized_content,
      const ContentToFrameMap& subframe_content_map,
      mojom::PdfCompositor::CompositePageToPdfCallback callback) override;
  void CompositeDocumentToPdf(
      uint64_t frame_guid,
      base::ReadOnlySharedMemoryRegion serialized_content,
      const ContentToFrameMap& subframe_content_map,
      mojom::PdfCompositor::CompositeDocumentToPdfCallback callback) override;
  void PrepareForDocumentToPdf(
      mojom::PdfCompositor::PrepareForDocumentToPdfCallback callback) override;
  void CompleteDocumentToPdf(
      uint32_t page_count,
      mojom::PdfCompositor::CompleteDocumentToPdfCallback callback) override;
  void SetWebContentsURL(const GURL& url) override;
  void SetUserAgent(const std::string& user_agent) override;

 protected:
  // This is the uniform underlying type for both
  // mojom::PdfCompositor::CompositePageToPdfCallback and
  // mojom::PdfCompositor::CompositeDocumentToPdfCallback.
  using CompositeToPdfCallback =
      base::OnceCallback<void(PdfCompositor::Status,
                              base::ReadOnlySharedMemoryRegion)>;

  using PrepareForDocumentToPdfCallback =
      base::OnceCallback<void(PdfCompositor::Status)>;
  using CompleteDocumentToPdfCallback =
      base::OnceCallback<void(PdfCompositor::Status,
                              base::ReadOnlySharedMemoryRegion)>;

  // The core function for content composition and conversion to a pdf file.
  // Make this function virtual so tests can override it.
  virtual mojom::PdfCompositor::Status CompositeToPdf(
      base::ReadOnlySharedMemoryMapping shared_mem,
      const ContentToFrameMap& subframe_content_map,
      base::ReadOnlySharedMemoryRegion* region);

  // Make these functions virtual so tests can override them.
  virtual void FulfillRequest(
      base::ReadOnlySharedMemoryMapping serialized_content,
      const ContentToFrameMap& subframe_content_map,
      CompositeToPdfCallback callback);
  virtual void CompleteDocumentRequest(CompleteDocumentToPdfCallback callback);

 private:
  FRIEND_TEST_ALL_PREFIXES(PdfCompositorImplTest, IsReadyToComposite);
  FRIEND_TEST_ALL_PREFIXES(PdfCompositorImplTest, MultiLayerDependency);
  FRIEND_TEST_ALL_PREFIXES(PdfCompositorImplTest, DependencyLoop);
  friend class MockCompletionPdfCompositorImpl;

  // The map needed during content deserialization. It stores the mapping
  // between content id and its actual content.
  using DeserializationContext = base::flat_map<uint32_t, sk_sp<SkPicture>>;

  // Base structure to store a frame's content and its subframe
  // content information.
  struct FrameContentInfo {
    FrameContentInfo(base::ReadOnlySharedMemoryMapping content,
                     const ContentToFrameMap& map);
    FrameContentInfo();
    ~FrameContentInfo();

    // Serialized SkPicture content of this frame.
    base::ReadOnlySharedMemoryMapping serialized_content;

    // Frame content after composition with subframe content.
    sk_sp<SkPicture> content;

    // Subframe content id and its corresponding frame guid.
    ContentToFrameMap subframe_content_map;
  };

  // Other than content, it also stores the status during frame composition.
  struct FrameInfo : public FrameContentInfo {
    FrameInfo();
    ~FrameInfo();

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
    RequestInfo(base::ReadOnlySharedMemoryMapping content,
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
    // Create the DocumentInfo object, which also creates a corresponding Skia
    // document object.
    explicit DocumentInfo(const std::string& creator);
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

  // Document content composition support functions when document is compiled
  // using individual pages' content.  These are not used when document is
  // composited with a separate metafile object.
  mojom::PdfCompositor::Status PrepareForDocumentToPdf();
  mojom::PdfCompositor::Status UpdateDocumentMetadata(uint32_t page_count);
  mojom::PdfCompositor::Status CompleteDocumentToPdf(
      base::ReadOnlySharedMemoryRegion* region);

  // Composite the content of a subframe.
  void CompositeSubframe(FrameInfo* frame_info);

  DeserializationContext GetDeserializationContext(
      const ContentToFrameMap& subframe_content_map);

  mojo::Receiver<mojom::PdfCompositor> receiver_{this};

  const scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;
  std::unique_ptr<discardable_memory::ClientDiscardableSharedMemoryManager>
      discardable_shared_memory_manager_;

  // The creator of this service.
  // Currently contains the service creator's user agent string if given,
  // otherwise just use string "Chromium".
  std::string creator_ = "Chromium";

  // Keep track of all frames' information indexed by frame id.
  FrameMap frame_info_map_;

  std::vector<std::unique_ptr<RequestInfo>> requests_;
  std::unique_ptr<DocumentInfo> docinfo_;

  DISALLOW_COPY_AND_ASSIGN(PdfCompositorImpl);
};

}  // namespace printing

#endif  // COMPONENTS_SERVICES_PDF_COMPOSITOR_PDF_COMPOSITOR_IMPL_H_
