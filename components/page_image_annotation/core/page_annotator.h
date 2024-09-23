// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_IMAGE_ANNOTATION_CORE_PAGE_ANNOTATOR_H_
#define COMPONENTS_PAGE_IMAGE_ANNOTATION_CORE_PAGE_ANNOTATOR_H_

#include <map>
#include <string>
#include <utility>

#include "base/compiler_specific.h"
#include "base/functional/callback.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/image_annotation/public/cpp/image_processor.h"
#include "services/image_annotation/public/mojom/image_annotation.mojom.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace page_image_annotation {

// Notifies clients of page images that can be annotated and forwards annotation
// requests for these images to the image annotation service.
//
// TODO(crbug.com/41432474): this class is not yet complete - add more logic
// (e.g.
//                         communication with the service).
class PageAnnotator {
 public:
  struct ImageMetadata {
    // A unique ID identifying an image on this page. Two separate images (even
    // with the same URL / pixels) on one page will be given separate node IDs.
    uint64_t node_id;

    // The URL or a hash of the data URI of this image. Two (identical) images
    // can have the same source ID.
    std::string source_id;

    // TODO(crbug.com/41432474): add other useful info (e.g. image dimensions).
  };

  // Clients (i.e. classes that annotate page images) should implement this
  // interface.
  class Observer : public base::CheckedObserver {
   public:
    ~Observer() override;

    // These methods are called during page lifecycle to notify the observer
    // about changes to page images.

    // Called exactly once per image, at the point that the image appears on the
    // page (or at the point that the observer subscribes to the page annotator,
    // if the image already exists on page).
    virtual void OnImageAdded(const ImageMetadata& image) = 0;

    // Called at the point that an image source is updated.
    virtual void OnImageModified(const ImageMetadata& image) = 0;

    // Called at the point that an image disappears from the page.
    virtual void OnImageRemoved(uint64_t node_id) = 0;

    // Called when annotation is complete (either successfully or
    // unsuccessfully) for a request made by this client.
    virtual void OnImageAnnotated(
        uint64_t node_id,
        image_annotation::mojom::AnnotateImageResultPtr result) = 0;
  };

  explicit PageAnnotator(
      mojo::PendingRemote<image_annotation::mojom::Annotator> annotator);

  PageAnnotator(const PageAnnotator&) = delete;
  PageAnnotator& operator=(const PageAnnotator&) = delete;

  ~PageAnnotator();

  // Request annotation of the given image via the image annotation service.
  // When annotation is complete (or fails), the OnImageAnnotated() method of
  // the observer is called.
  //
  // Must be called on a valid (i.e. added and not yet removed) node ID.
  void AnnotateImage(Observer* observer, uint64_t node_id);

  // Called by platform drivers.
  void ImageAddedOrPossiblyModified(
      const ImageMetadata& metadata,
      base::RepeatingCallback<SkBitmap()> pixels_callback);
  void ImageRemoved(uint64_t node_id);

  // An observer must outlive the PageAnnotator, or be destructed synchronously
  // with the PageAnnotator (e.g. at the same point in the document lifecycle)
  // and not reference the PageAnnotator in its destructor.
  void AddObserver(Observer* observer);

 private:
  // Add a new entry to |images_|.
  //
  // The lack of copy/move constructor for ImageProcessor makes this difficult,
  // but we limit the complexity to this method.
  void AddNewImage(const ImageMetadata& metadata,
                   base::RepeatingCallback<SkBitmap()> pixels_callback);

  // Callback passed to the image annotation service to receive image annotation
  // results.
  void NotifyObserver(Observer* observer,
                      uint64_t node_id,
                      image_annotation::mojom::AnnotateImageResultPtr result);

  mojo::Remote<image_annotation::mojom::Annotator> annotator_;

  base::ObserverList<Observer> observers_;

  std::map<uint64_t, std::pair<ImageMetadata, image_annotation::ImageProcessor>>
      images_;
};

}  // namespace page_image_annotation

#endif  // COMPONENTS_PAGE_IMAGE_ANNOTATION_CORE_PAGE_ANNOTATOR_H_
