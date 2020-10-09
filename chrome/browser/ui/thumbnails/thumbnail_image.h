// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_THUMBNAILS_THUMBNAIL_IMAGE_H_
#define CHROME_BROWSER_UI_THUMBNAILS_THUMBNAIL_IMAGE_H_

#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/optional.h"
#include "base/sequence_checker.h"
#include "ui/gfx/image/image_skia.h"

namespace base {
class TimeTicks;
}  // namespace base

// Stores compressed thumbnail data for a tab and can vend that data as an
// uncompressed image to observers.
class ThumbnailImage : public base::RefCounted<ThumbnailImage> {
 public:
  // Smart pointer to reference-counted compressed image data; in this case
  // JPEG format.
  using CompressedThumbnailData =
      scoped_refptr<base::RefCountedData<std::vector<uint8_t>>>;

  // Observes uncompressed and/or compressed versions of the thumbnail image as
  // they are available.
  class Observer : public base::CheckedObserver {
   public:
    // Receives uncompressed thumbnail image data. Default is no-op.
    virtual void OnThumbnailImageAvailable(gfx::ImageSkia thumbnail_image);

    // Receives compressed thumbnail image data. Default is no-op.
    virtual void OnCompressedThumbnailDataAvailable(
        CompressedThumbnailData thumbnail_data);

    // Provides a desired aspect ratio and minimum size that the observer will
    // accept. If not specified, or if available thumbnail data is smaller in
    // either dimension than this value, OnThumbnailImageAvailable will be
    // called with an uncropped image. If this value is specified, and the
    // available image is larger, the image passed to OnThumbnailImageAvailable
    // will be cropped to the same aspect ratio (but otherwise unchanged,
    // including scale).
    //
    // OnCompressedThumbnailDataAvailable is not affected by this value.
    //
    // This method is used to ensure that except for very small thumbnails, the
    // image passed to OnThumbnailImageAvailable fits the needs of the observer
    // for display purposes, without the observer having to further crop the
    // image. The default is unspecified.
    virtual base::Optional<gfx::Size> GetThumbnailSizeHint() const;
  };

  // Represents the endpoint
  class Delegate {
   public:
    // Called whenever the thumbnail starts or stops being observed.
    // Because updating the thumbnail could be an expensive operation, it's
    // useful to track when there are no observers. Default behavior is no-op.
    virtual void ThumbnailImageBeingObservedChanged(bool is_being_observed) = 0;

   protected:
    virtual ~Delegate();

   private:
    friend class ThumbnailImage;
    ThumbnailImage* thumbnail_ = nullptr;
  };

  explicit ThumbnailImage(Delegate* delegate);

  bool has_data() const { return data_.get(); }

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);
  bool HasObserver(const Observer* observer) const;

  // Sets the SkBitmap data and notifies observers with the resulting image.
  void AssignSkBitmap(SkBitmap bitmap);

  // Requests that a thumbnail image be made available to observers. Does not
  // guarantee that Observer::OnThumbnailImageAvailable() will be called, or how
  // long it will take, though in most cases it should happen very quickly.
  void RequestThumbnailImage();

  // Similar to RequestThumbnailImage() but requests only the compressed JPEG
  // data. Users should listen for a call to
  // Observer::OnCompressedThumbnailDataAvailable().
  void RequestCompressedThumbnailData();

  void set_async_operation_finished_callback_for_testing(
      base::RepeatingClosure callback) {
    async_operation_finished_callback_ = std::move(callback);
  }

 private:
  friend class Delegate;
  friend class ThumbnailImageTest;
  friend class base::RefCounted<ThumbnailImage>;

  virtual ~ThumbnailImage();

  void AssignJPEGData(base::TimeTicks assign_sk_bitmap_time,
                      std::vector<uint8_t> data);
  bool ConvertJPEGDataToImageSkiaAndNotifyObservers();
  void NotifyUncompressedDataObservers(gfx::ImageSkia image);
  void NotifyCompressedDataObservers(CompressedThumbnailData data);

  static std::vector<uint8_t> CompressBitmap(SkBitmap bitmap);
  static gfx::ImageSkia UncompressImage(CompressedThumbnailData compressed);

  // Crops and returns a preview from a thumbnail of an entire web page. Uses
  // logic appropriate for fixed-aspect previews (e.g. hover cards).
  static gfx::ImageSkia CropPreviewImage(const gfx::ImageSkia& source_image,
                                         const gfx::Size& minimum_size);

  Delegate* delegate_;

  CompressedThumbnailData data_;

  base::ObserverList<Observer> observers_;

  // Called when an asynchronous operation (such as encoding image data upon
  // assignment or decoding image data for observers) finishes or fails.
  // Intended for unit tests that want to wait for internal operations following
  // AssignSkBitmap() or RequestThumbnailImage() calls.
  base::RepeatingClosure async_operation_finished_callback_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<ThumbnailImage> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ThumbnailImage);
};

#endif  // CHROME_BROWSER_UI_THUMBNAILS_THUMBNAIL_IMAGE_H_
