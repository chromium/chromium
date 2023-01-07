// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_IMAGE_ANNOTATION_CONTENT_RENDERER_CONTENT_PAGE_ANNOTATOR_DRIVER_H_
#define COMPONENTS_PAGE_IMAGE_ANNOTATION_CONTENT_RENDERER_CONTENT_PAGE_ANNOTATOR_DRIVER_H_

#include <map>
#include <utility>

#include "base/gtest_prod_util.h"
#include "components/page_image_annotation/core/page_annotator.h"
#include "content/public/renderer/render_frame_observer.h"
#include "content/public/renderer/render_frame_observer_tracker.h"
#include "third_party/blink/public/web/web_element.h"
#include "ui/base/page_transition_types.h"

namespace page_image_annotation {

// This class holds a PageAnnotator for a given RenderFrame and feeds it
// information about images as they appear / disappear from the page.
//
// This class can also be used to access the Content-level interface (i.e.
// blink::WebElement) for DOM nodes with associated images.
class ContentPageAnnotatorDriver
    : public content::RenderFrameObserver,
      public content::RenderFrameObserverTracker<ContentPageAnnotatorDriver> {
 public:
  ContentPageAnnotatorDriver(const ContentPageAnnotatorDriver&) = delete;
  ContentPageAnnotatorDriver& operator=(const ContentPageAnnotatorDriver&) =
      delete;

  ~ContentPageAnnotatorDriver() override;

  static ContentPageAnnotatorDriver* GetOrCreate(
      content::RenderFrame* render_frame);

  // Given a page URL and a URI fragment (which could possibly be an absolute
  // URL, relative URL or data URI), produces a source ID.
  //
  // The source ID of a URL is the absolute version of the URL. The source ID of
  // a data URI is the base64 encoded SHA256 hash of the data string.
  //
  // If a source ID cannot be generated (e.g. the URI fragment is malformed),
  // the empty string is returned.
  static std::string GenerateSourceId(const GURL& page_url,
                                      const std::string& uri_fragment);

  PageAnnotator& GetPageAnnotator();

  // Returns the element associated with the given node ID. If there is no such
  // element, returns a null blink::WebElement.
  blink::WebElement GetElement(uint64_t node_id);

 private:
  // We delete ourselves on frame destruction, so disallow construction on the
  // stack.
  ContentPageAnnotatorDriver(content::RenderFrame* render_frame);

  // content::RenderFrameObserver:
  void DidDispatchDOMContentLoadedEvent() override;
  void OnDestruct() override;

  // Traverse the DOM starting at the given node, and add all elements with
  // associated images to the |tracked_elements_| map.
  void FindImages(const GURL& page_url, blink::WebElement element);

  // Traverse the DOM for elements with associated images, add these elements to
  // |tracked_elements_|, and send element info to the PageAnnotator.
  void FindAndTrackImages();

  // Return the bitmap associated with the given node ID.
  SkBitmap GetBitmapForId(uint64_t node_id);

  // The next ID to assign to a DOM node.
  uint64_t next_node_id_;

  // The current set of tracked DOM nodes.
  std::map<uint64_t, std::pair<PageAnnotator::ImageMetadata, blink::WebElement>>
      tracked_elements_;

  PageAnnotator page_annotator_;

  base::WeakPtrFactory<ContentPageAnnotatorDriver> weak_ptr_factory_{this};
};

}  // namespace page_image_annotation

#endif  //  COMPONENTS_PAGE_IMAGE_ANNOTATION_CONTENT_RENDERER_CONTENT_PAGE_ANNOTATOR_DRIVER_H_
