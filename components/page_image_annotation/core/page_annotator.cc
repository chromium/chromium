// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_image_annotation/core/page_annotator.h"

#include "base/bind.h"

namespace page_image_annotation {

namespace ia_mojom = image_annotation::mojom;

PageAnnotator::Observer::~Observer() {}

PageAnnotator::PageAnnotator(mojo::PendingRemote<ia_mojom::Annotator> annotator)
    : annotator_(std::move(annotator)) {}

PageAnnotator::~PageAnnotator() {}

void PageAnnotator::ImageAddedOrPossiblyModified(
    const ImageMetadata& metadata,
    base::RepeatingCallback<SkBitmap()> pixels_callback) {
  const auto lookup = images_.find(metadata.node_id);

  if (lookup == images_.end()) {
    // This is an image addition.

    AddNewImage(metadata, std::move(pixels_callback));

    for (Observer& observer : observers_) {
      observer.OnImageAdded(metadata);
    }
  } else if (lookup->second.first.source_id != metadata.source_id) {
    // We already have older data for this node ID; this is an update.

    images_.erase(lookup);
    AddNewImage(metadata, std::move(pixels_callback));

    for (Observer& observer : observers_) {
      observer.OnImageModified(metadata);
    }
  }
}

void PageAnnotator::ImageRemoved(const uint64_t node_id) {
  images_.erase(node_id);

  for (Observer& observer : observers_) {
    observer.OnImageRemoved(node_id);
  }
}

void PageAnnotator::AnnotateImage(Observer* const observer,
                                  const uint64_t node_id) {
  DCHECK(observers_.HasObserver(observer));

  const auto lookup = images_.find(node_id);
  if (lookup == images_.end())
    return;

  // TODO(crbug.com/916363): get a user's preferred language and pass it here.
  annotator_->AnnotateImage(
      lookup->second.first.source_id,
      std::string() /* description_language_tag */,
      lookup->second.second.GetPendingRemote(),
      base::BindOnce(&PageAnnotator::NotifyObserver, base::Unretained(this),
                     observer, node_id));
}

void PageAnnotator::AddObserver(Observer* const observer) {
  observers_.AddObserver(observer);

  // The new observer has not received any previous messages; inform them now of
  // all existing images.
  for (const auto& image : images_) {
    observer->OnImageAdded(image.second.first);
  }
}

void PageAnnotator::AddNewImage(
    const ImageMetadata& metadata,
    base::RepeatingCallback<SkBitmap()> pixels_callback) {
  images_.emplace(std::piecewise_construct,
                  std::forward_as_tuple(metadata.node_id),
                  std::forward_as_tuple(metadata, std::move(pixels_callback)));
}

void PageAnnotator::NotifyObserver(Observer* const observer,
                                   const uint64_t node_id,
                                   ia_mojom::AnnotateImageResultPtr result) {
  observer->OnImageAnnotated(node_id, std::move(result));
}

}  // namespace page_image_annotation
