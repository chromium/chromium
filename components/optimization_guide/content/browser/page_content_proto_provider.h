// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CONTENT_BROWSER_PAGE_CONTENT_PROTO_PROVIDER_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CONTENT_BROWSER_PAGE_CONTENT_PROTO_PROVIDER_H_

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "base/unguessable_token.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "content/public/browser/document_user_data.h"
#include "content/public/browser/page_user_data.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/weak_document_ptr.h"
#include "content/public/browser/web_contents_observer.h"
#include "third_party/blink/public/mojom/content_extraction/ai_page_content.mojom-forward.h"
#include "third_party/blink/public/mojom/content_extraction/ai_page_content_metadata.mojom.h"
#include "ui/gfx/geometry/rect.h"

namespace content {
class NavigationHandle;
class Page;
class WebContents;
}

namespace optimization_guide {
// See AIPageContentOptions in ai_page_content.mojom for documentation of
// `on_critical_path`.
blink::mojom::AIPageContentOptionsPtr DefaultAIPageContentOptions(
    bool on_critical_path);
blink::mojom::AIPageContentOptionsPtr ActionableAIPageContentOptions(
    bool on_critical_path);

// A DocumentUserData that stores a serialized unguessable token for a given
// RenderFrameHost.
class DocumentIdentifierUserData
    : public content::DocumentUserData<DocumentIdentifierUserData> {
 public:
  explicit DocumentIdentifierUserData(content::RenderFrameHost* rfh);
  ~DocumentIdentifierUserData() override;

  const base::UnguessableToken& token() const { return token_; }
  std::string serialized_token() const { return serialized_token_; }

  static std::optional<std::string> GetDocumentIdentifier(
      content::GlobalRenderFrameHostToken token);

 private:
  const base::UnguessableToken token_;
  const std::string serialized_token_;

  friend DocumentUserData;
  DOCUMENT_USER_DATA_KEY_DECL();
};

// A PageUserData that tracks extraction concurrency and navigation segment
// counts for `GetAIPageContent()`.
class AIPageContentMetricsLogger
    : public content::PageUserData<AIPageContentMetricsLogger>,
      public content::WebContentsObserver {
 public:
  explicit AIPageContentMetricsLogger(content::Page& page);
  ~AIPageContentMetricsLogger() override;

  // Called when a GetAIPageContent() extraction starts. Returns a completion
  // callback to be run when the extraction is done.
  [[nodiscard]] base::OnceCallback<void(bool success)> OnExtractionStarted(
      const blink::mojom::AIPageContentOptions& options);

  // content::WebContentsObserver:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

 private:
  friend class content::PageUserData<AIPageContentMetricsLogger>;

  void RecordAndResetBetweenNavigationMetric();
  void OnExtractionFinished(uint64_t request_id, bool success);

  int active_extractions_count_ = 0;
  uint64_t next_request_id_ = 0;
  base::flat_map<uint64_t, blink::mojom::AIPageContentOptionsPtr>
      active_extraction_options_;
  base::TimeTicks last_successful_extraction_time_;

  int total_extractions_started_ = 0;
  int extractions_since_last_navigation_ = 0;

  PAGE_USER_DATA_KEY_DECL();

  base::WeakPtrFactory<AIPageContentMetricsLogger> weak_ptr_factory_{this};
};

// The result of a call to GetAIPageContent.  It contains the
// AnnotatedPageContent proto and metadata about the page content.
struct AIPageContentResult {
  AIPageContentResult();
  AIPageContentResult(const AIPageContentResult& other) = delete;
  AIPageContentResult(AIPageContentResult&& other);
  AIPageContentResult& operator=(const AIPageContentResult& other) = delete;
  AIPageContentResult& operator=(AIPageContentResult&& other);
  ~AIPageContentResult();

  optimization_guide::proto::AnnotatedPageContent proto;
  blink::mojom::PageMetadataPtr metadata;
  // A map from a serialized unguessable token to the document pointer.
  // Callers should use this to map the frame identifiers in the proto to the
  // right frame host.
  base::flat_map<std::string, content::WeakDocumentPtr> document_identifiers;
  // Final screenshot redaction boxes. The converter folds together renderer
  // password boxes plus browser-derived sensitive payment and OTP boxes.
  std::vector<gfx::Rect> visible_bounding_boxes_for_redaction;
};

// Provides AIPageContentResult (AnnotatedPageContent proto and metadata) for
// the primary page displayed in a WebContents or an error string on failure.
using AIPageContentResultOrError =
    base::expected<AIPageContentResult, std::string>;
using OnAIPageContentDone =
    base::OnceCallback<void(AIPageContentResultOrError)>;
void GetAIPageContent(content::WebContents* web_contents,
                      blink::mojom::AIPageContentOptionsPtr options,
                      OnAIPageContentDone done_callback);

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CONTENT_BROWSER_PAGE_CONTENT_PROTO_PROVIDER_H_
