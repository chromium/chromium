// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_THUMBNAILS_THUMBNAIL_IMAGE_H_
#define CHROME_BROWSER_UI_THUMBNAILS_THUMBNAIL_IMAGE_H_

#include <stdint.h>

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/token.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_skia.h"

namespace base {
class TimeTicks;
}  // namespace base

// Stores compressed thumbnail data for a tab and can vend that data as an
// uncompressed image to observers.
class ThumbnailImage : public base::RefCountedThreadSafe<ThumbnailImage> {
 public:
  // Describes the readiness of the source page for thumbnail capture.
  enum class CaptureReadiness : int {
    // The page is not ready for capturing.
    kNotReady = 0,
    // Thumbnails can be captured, but the page might change. Captured frames
    // should not be used as the final thumbnail.
    kReadyForInitialCapture,
    // The page is fully loaded and a thumbnail can be captured that should be
    // representative of the page's final state. Dynamic elements might not be
    // in final position yet, but should settle fairly quickly (on the order of
    // a few seconds).
    kReadyForFinalCapture,
  };

  // Smart pointer to reference-counted compressed image data; in this case
  // JPEG format.
  using CompressedThumbnailData =
      scoped_refptr<base::RefCountedData<std::vector<uint8_t>>>;

  class Subscription {
   public:
    Subscription() = delete;
    ~Subscription();

    using UncompressedImageCallback =
        base::RepeatingCallback<void(gfx::ImageSkia)>;
    using CompressedImageCallback =
        base::RepeatingCallback<void(CompressedThumbnailData)>;

    // Set callbacks to receive image data. Subscribers are not allowed
    // to unsubscribe (by destroying |this|) from the callback. If
    // necessary, post a task to destroy it soon after.

    void SetUncompressedImageCallback(UncompressedImageCallback callback) {
      uncompressed_image_callback_ = std::move(callback);
    }

    void SetCompressedImageCallback(CompressedImageCallback callback) {
      compressed_image_callback_ = std::move(callback);
    }

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
    void SetSizeHint(const std::optional<gfx::Size>& size_hint) {
      size_hint_ = size_hint;
    }

   private:
    friend class ThumbnailImage;

    explicit Subscription(scoped_refptr<ThumbnailImage> thumbnail);

    scoped_refptr<ThumbnailImage> thumbnail_;
    std::optional<gfx::Size> size_hint_;

    UncompressedImageCallback uncompressed_image_callback_;
    CompressedImageCallback compressed_image_callback_;
  };

  // Represents the endpoint
  class Delegate {
   public:
    // Called whenever the thumbnail starts or stops being observed.
    // Because updating the thumbnail could be an expensive operation, it's
    // useful to track when there are no observers. Default behavior is no-op.
    virtual void ThumbnailImageBeingObservedChanged(bool is_being_observed) = 0;

    // Requests the backing tab's capture readiness from the delegate.
    // The default implementation returns kUnknown.
    virtual CaptureReadiness GetCaptureReadiness() const;

   protected:
    virtual ~Delegate();

   private:
    friend class ThumbnailImage;
    raw_ptr<ThumbnailImage> thumbnail_ = nullptr;
  };

  explicit ThumbnailImage(Delegate* delegate,
                          CompressedThumbnailData data = nullptr);

  ThumbnailImage(const ThumbnailImage&) = delete;
  ThumbnailImage& operator=(const ThumbnailImage&) = delete;

  bool has_data() const { return data_.get(); }

  // Gets the capture readiness of the backing tab.
  CaptureReadiness GetCaptureReadiness() const;

  // Subscribe to thumbnail updates. See |Subscription| to set a
  // callback and conigure additional options.
  //
  // Even if a callback is not set, the subscription influences
  // thumbnail capture. It should be destroyed when updates are not
  // needed. It is designed to be stored in std::optional, created and
  // destroyed as needed.
  std::unique_ptr<Subscription> Subscribe();

  // Sets the SkBitmap data and notifies observers with the resulting image.
  void AssignSkBitmap(SkBitmap bitmap,
                      std::optional<uint64_t> frame_id = std::nullopt);

  // Clears the currently set |data_|, for when the current thumbnail is no
  // longer valid to display.
  void ClearData();

  // Requests that a thumbnail image be made available to observers. Does not
  // guarantee that Observer::OnThumbnailImageAvailable() will be called, or how
  // long it will take, though in most cases it should happen very quickly.
  void RequestThumbnailImage();

  // Similar to RequestThumbnailImage() but requests only the compressed JPEG
  // data. Users should listen for a call to
  // Observer::OnCompressedThumbnailDataAvailable().
  void RequestCompressedThumbnailData();

  // Returns the size of the compressed data backing this thumbnail.
  // This size can be 0. Additionally, since this data is refcounted,
  // it's possible this returns 0 even if the data is still allocated. A
  // client can hold a reference to it after |this| drops its reference.
  size_t GetCompressedDataSizeInBytes() const;

  void set_async_operation_finished_callback_for_testing(
      base::RepeatingClosure callback) {
    async_operation_finished_callback_ = std::move(callback);
  }

  CompressedThumbnailData data() { return data_; }

 private:
  friend class Delegate;
  friend class ThumbnailImageTest;
  friend class base::RefCountedThreadSafe<ThumbnailImage>;

  virtual ~ThumbnailImage();

  void AssignJPEGData(base::Token thumbnail_id,
                      base::TimeTicks assign_sk_bitmap_time,
                      std::optional<uint64_t> frame_id_for_trace,
                      std::vector<uint8_t> data);
  bool ConvertJPEGDataToImageSkiaAndNotifyObservers();
  void NotifyUncompressedDataObservers(base::Token thumbnail_id,
                                       gfx::ImageSkia image);
  void NotifyCompressedDataObservers(CompressedThumbnailData data);

  static std::vector<uint8_t> CompressBitmap(SkBitmap bitmap,
                                             std::optional<uint64_t> frame_id);
  static gfx::ImageSkia UncompressImage(CompressedThumbnailData compressed);

  // Crops and returns a preview from a thumbnail of an entire web page. Uses
  // logic appropriate for fixed-aspect previews (e.g. hover cards).
  static gfx::ImageSkia CropPreviewImage(const gfx::ImageSkia& source_image,
                                         const gfx::Size& minimum_size);

  void HandleSubscriptionDestroyed(Subscription* subscription);

  raw_ptr<Delegate> delegate_;

  // This is a scoped_refptr to immutable data. Once set, the wrapped
  // data must not be modified; it is referenced by other threads.
  // |data_| itself can be changed as this does not affect references to
  // the old data.
  CompressedThumbnailData data_;

  // A randomly generated ID associated with each image assigned by
  // AssignSkBitmap().
  base::Token thumbnail_id_;

  // Subscriptions are inserted on |Subscribe()| calls and removed when
  // they are destroyed via callback. The order of subscriber
  // notification doesn't matter, so don't maintain any ordering. Since
  // the number of subscribers for a given thumbnail is expected to be
  // small, doing a linear search to remove a subscriber is fine.
  std::vector<raw_ptr<Subscription>> subscribers_;

  // Called when an asynchronous operation (such as encoding image data upon
  // assignment or decoding image data for observers) finishes or fails.
  // Intended for unit tests that want to wait for internal operations following
  // AssignSkBitmap() or RequestThumbnailImage() calls.
  base::RepeatingClosure async_operation_finished_callback_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<ThumbnailImage> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_THUMBNAILS_THUMBNAIL_IMAGE_H_
