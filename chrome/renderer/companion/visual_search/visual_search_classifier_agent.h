// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_COMPANION_VISUAL_SEARCH_VISUAL_SEARCH_CLASSIFIER_AGENT_H_
#define CHROME_RENDERER_COMPANION_VISUAL_SEARCH_VISUAL_SEARCH_CLASSIFIER_AGENT_H_

#include "base/files/file.h"
#include "base/files/memory_mapped_file.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/common/companion/visual_search.mojom.h"
#include "chrome/renderer/companion/visual_search/visual_search_eligibility.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_frame_observer.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace companion::visual_search {

using ClassificationResultsAndStats =
    std::pair<std::vector<SingleImageFeaturesAndBytes>,
              mojom::ClassificationStatsPtr>;

class VisualSearchClassifierAgent : public content::RenderFrameObserver,
                                    mojom::VisualSuggestionsRequestHandler {
 public:
  static VisualSearchClassifierAgent* Create(
      content::RenderFrame* render_frame);

  VisualSearchClassifierAgent(const VisualSearchClassifierAgent&) = delete;
  VisualSearchClassifierAgent& operator=(const VisualSearchClassifierAgent&) =
      delete;

  ~VisualSearchClassifierAgent() override;

  // RenderFrameObserver implementation:
  void OnDestruct() override;
  void DidFinishLoad() override;

  // VisualSuggestionsRequestHandler implementation:
  // This method is the main entrypoint which triggers visual classification.
  // This is ultimately going to be called via Mojom IPC from the browser
  // process.
  void StartVisualClassification(
      base::File visual_model,
      const std::string& config_proto,
      mojo::PendingRemote<mojom::VisualSuggestionsResultHandler> result_handler)
      override;

  // Callback used to find incoming receiver to reference in this class.
  void OnRendererAssociatedRequest(
      mojo::PendingAssociatedReceiver<mojom::VisualSuggestionsRequestHandler>
          receiver);

 private:
  explicit VisualSearchClassifierAgent(content::RenderFrame* render_frame);

  // Private method used to post result from long-running visual classification
  // tasks that runs in the background thread. This method should run in the
  // same thread that triggered the classification task (i.e. main thread).
  void OnClassificationDone(ClassificationResultsAndStats results);

  // Handler method used when agent request model from browser.
  void HandleGetModelCallback(base::File file, const std::string& config);

  // Used to track whether there is an ongoing classification task, if so, we
  // drop the incoming request.
  bool is_classifying_ = false;

  // Used to track that we are retrying visual classification because our
  // first attempt did not find any images in the DOM.
  bool is_retrying_ = false;

  // Pointer to RenderFrame used for DOM traversal and extract image bytes.
  content::RenderFrame* render_frame_ = nullptr;

  // Using a memory-mapped file to reduce memory consumption of model bytes.
  base::MemoryMappedFile visual_model_;

  mojo::AssociatedReceiver<mojom::VisualSuggestionsRequestHandler> receiver_{
      this};

  // The result handler is used to give us a path back to results. It
  // typically will lead to a Mojom IPC call back to the browser process.
  mojo::Remote<mojom::VisualSuggestionsResultHandler> result_handler_;

  // Remote pipe for fetching model and metadata from the browser process.
  mojo::Remote<mojom::VisualSuggestionsModelProvider> model_provider_;

  // Pointer factory necessary for scheduling tasks on different threads.
  base::WeakPtrFactory<VisualSearchClassifierAgent> weak_ptr_factory_{this};
};

}  // namespace companion::visual_search

#endif  // CHROME_RENDERER_COMPANION_VISUAL_SEARCH_VISUAL_SEARCH_CLASSIFIER_AGENT_H_
