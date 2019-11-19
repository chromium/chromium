// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_image_annotation/content/renderer/content_page_annotator_driver.h"

#include "base/base64.h"
#include "base/bind.h"
#include "base/optional.h"
#include "content/public/renderer/render_frame.h"
#include "crypto/sha2.h"
#include "services/image_annotation/public/mojom/image_annotation.mojom.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_node.h"

namespace page_image_annotation {

namespace {

namespace ia_mojom = image_annotation::mojom;

// The number of milliseconds to wait before traversing the DOM to find image
// elements.
constexpr int kDomCrawlDelayMs = 3000;

// Attempts to produce image metadata for the given element. Will produce a null
// value if the element has a missing or malformed src attribute.
base::Optional<PageAnnotator::ImageMetadata> ProduceMetadata(
    const GURL& page_url,
    const blink::WebElement element,
    const uint64_t node_id) {
  const std::string source_id = ContentPageAnnotatorDriver::GenerateSourceId(
      page_url, element.GetAttribute("src").Utf8());
  if (source_id.empty())
    return base::nullopt;

  return PageAnnotator::ImageMetadata{node_id, source_id};
}

mojo::PendingRemote<ia_mojom::Annotator> RequestAnnotator(
    content::RenderFrame* const render_frame) {
  mojo::PendingRemote<ia_mojom::Annotator> annotator;
  render_frame->GetBrowserInterfaceBroker()->GetInterface(
      annotator.InitWithNewPipeAndPassReceiver());
  return annotator;
}

}  // namespace

ContentPageAnnotatorDriver::ContentPageAnnotatorDriver(
    content::RenderFrame* const render_frame)
    : RenderFrameObserver(render_frame),
      RenderFrameObserverTracker<ContentPageAnnotatorDriver>(render_frame),
      next_node_id_(1),
      page_annotator_(RequestAnnotator(render_frame)) {}

ContentPageAnnotatorDriver::~ContentPageAnnotatorDriver() {}

// static
ContentPageAnnotatorDriver* ContentPageAnnotatorDriver::GetOrCreate(
    content::RenderFrame* const render_frame) {
  ContentPageAnnotatorDriver* const existing = Get(render_frame);
  if (existing)
    return existing;

  return new ContentPageAnnotatorDriver(render_frame);
}

PageAnnotator& ContentPageAnnotatorDriver::GetPageAnnotator() {
  return page_annotator_;
}

blink::WebElement ContentPageAnnotatorDriver::GetElement(
    const uint64_t node_id) {
  const auto lookup = tracked_elements_.find(node_id);
  if (lookup == tracked_elements_.end())
    return blink::WebElement();

  return lookup->second.second;
}

// static
std::string ContentPageAnnotatorDriver::GenerateSourceId(
    const GURL& page_url,
    const std::string& uri_fragment) {
  if (uri_fragment.empty())
    return std::string();

  const GURL src_url = page_url.Resolve(uri_fragment);
  if (!src_url.is_valid())
    return std::string();

  // Assign a source ID: either the URL of this image (if it can be resolved) or
  // a hash of its data URI.
  if (src_url.SchemeIs("data")) {
    const std::string& content = src_url.GetContent();

    if (!content.empty()) {
      // We use SHA256 since it has comparable (<2x) speed to e.g. crc32, but
      // has no known collisions (which could lead to cached results for another
      // image being returned for this one).
      std::string source_id;
      base::Base64Encode(crypto::SHA256HashString(content), &source_id);
      return source_id;
    }
  } else if (src_url.SchemeIs("http") || src_url.SchemeIs("https")) {
    return src_url.spec();
  }

  return std::string();
}

void ContentPageAnnotatorDriver::DidFinishDocumentLoad() {
  if (!render_frame()->IsMainFrame())
    return;

  // Cancel any pending DOM crawl.
  weak_ptr_factory_.InvalidateWeakPtrs();

  // Stop tracking old elements. After a page refresh, we're the only thing
  // keeping these elements alive (i.e. they are not still being displayed to
  // the user); we need to let Blink garbage collect them.
  for (const auto& entry : tracked_elements_) {
    page_annotator_.ImageRemoved(entry.first);
  }
  tracked_elements_.clear();

  // Schedule new DOM crawl after page has likely reached a stable state.
  //
  // TODO(crbug.com/916363): this approach is ad-hoc (e.g. uses a heuristic
  //                         delay to wait for a stable DOM) and can cause jank;
  //                         reinvestigate it once we are done prototyping the
  //                         feature.
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&ContentPageAnnotatorDriver::FindAndTrackImages,
                     weak_ptr_factory_.GetWeakPtr()),
      base::TimeDelta::FromMilliseconds(kDomCrawlDelayMs));
}

void ContentPageAnnotatorDriver::OnDestruct() {
  delete this;
}

void ContentPageAnnotatorDriver::FindImages(const GURL& page_url,
                                            blink::WebElement element) {
  if (element.ImageContents().isNull()) {
    // This element is not an image but it could have children that are.
    for (blink::WebNode child = element.FirstChild(); !child.IsNull();
         child = child.NextSibling()) {
      if (child.IsElementNode())
        FindImages(page_url, child.To<blink::WebElement>());
    }
  } else {
    // This element is an image; attempt to produce metadata for it and begin
    // tracking.
    const base::Optional<PageAnnotator::ImageMetadata> metadata =
        ProduceMetadata(page_url, element, next_node_id_);

    if (metadata.has_value())
      tracked_elements_.insert({next_node_id_++, {*metadata, element}});
  }
}

void ContentPageAnnotatorDriver::FindAndTrackImages() {
  const blink::WebDocument doc = render_frame()->GetWebFrame()->GetDocument();
  if (doc.IsNull() || doc.Body().IsNull())
    return;

  const GURL page_url(doc.Url().GetString().Utf8(), doc.Url().GetParsed(),
                      doc.Url().IsValid());
  FindImages(page_url, doc.Body());

  // Inform the PageAnnotator of the new images.
  for (const auto& entry : tracked_elements_) {
    page_annotator_.ImageAddedOrPossiblyModified(
        entry.second.first,
        base::BindRepeating(&ContentPageAnnotatorDriver::GetBitmapForId,
                            base::Unretained(this), entry.first));
  }
}

SkBitmap ContentPageAnnotatorDriver::GetBitmapForId(const uint64_t node_id) {
  return GetElement(node_id).ImageContents();
}

}  // namespace page_image_annotation
