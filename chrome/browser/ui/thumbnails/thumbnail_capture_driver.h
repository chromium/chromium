// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_THUMBNAILS_THUMBNAIL_CAPTURE_DRIVER_H_
#define CHROME_BROWSER_UI_THUMBNAILS_THUMBNAIL_CAPTURE_DRIVER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "chrome/browser/ui/thumbnails/thumbnail_readiness_tracker.h"
#include "chrome/browser/ui/thumbnails/thumbnail_scheduler.h"

class ThumbnailCaptureDriver : public ThumbnailScheduler::TabCapturer {
 public:
  class Client {
   public:
    // Asks the client to prepare for capture. The client should reply
    // by calling SetCanCapture(true) when ready.
    virtual void RequestCapture() = 0;

    // Begin capturing and updating the thumbnail immediately. This will
    // not be called unless SetCanCapture(true) was called. The client
    // should call GotFrame() whenever it gets a new frame from capture.
    virtual void StartCapture() = 0;

    // Stop capturing and cancel previous RequestCapture() call.
    virtual void StopCapture() = 0;

   protected:
    // Not deleted through interface pointer.
    ~Client() = default;
  };

  using PageReadiness = ThumbnailReadinessTracker::Readiness;

  explicit ThumbnailCaptureDriver(Client* client,
                                  ThumbnailScheduler* scheduler);
  ~ThumbnailCaptureDriver() override;

  // Update the capture state machine with new data.
  void UpdatePageReadiness(PageReadiness page_readiness);
  void UpdatePageVisibility(bool page_visible);
  void UpdateThumbnailVisibility(bool thumbnail_visible);

  // Can be called whenever. Will not issue a Client::StartCapture()
  // call if this is false. If set to false during capture, assumes the
  // capture stopped but the request is still outstanding. On the next
  // call with true, may immediately issue a Client::StartCapture()
  // call.
  void SetCanCapture(bool can_capture);

  // Notify scheduler a frame was received during capture.
  void GotFrame();

  // ThumbnailScheduler:
  void SetCapturePermittedByScheduler(bool scheduled) override;

  // Determines how long to wait for final capture, and how many times
  // to retry if one is not received. Exposed for testing.
  static constexpr base::TimeDelta kCooldownDelay = base::Milliseconds(500);
  static constexpr size_t kMaxCooldownRetries = 3;

 private:
  // How far along we are in the capture lifecycle for a given page.
  enum class CaptureState : int {
    // We have not started capturing the current page.
    kNoCapture = 0,
    // We have asked to capture but haven't started yet.
    kCaptureRequested,
    // We are currently capturing.
    kCapturing,
    // We are attempting to capture our last frame.
    kCooldown,
    // We have a good capture of the final page.
    kHaveFinalCapture,
  };

  void UpdateSchedulingPriority();
  void UpdateCaptureState();
  void StartCooldown();
  void OnCooldownEnded();

  const raw_ptr<Client> client_;
  const raw_ptr<ThumbnailScheduler> scheduler_;

  PageReadiness page_readiness_ = PageReadiness::kNotReady;
  bool page_visible_ = false;
  bool thumbnail_visible_ = false;
  bool can_capture_ = false;
  bool scheduled_ = false;

  CaptureState capture_state_ = CaptureState::kNoCapture;

  // Has a frame been captured during cooldown?
  bool captured_cooldown_frame_ = false;
  size_t cooldown_retry_count_ = 0U;

  base::RetainingOneShotTimer cooldown_timer_;

  base::WeakPtrFactory<ThumbnailCaptureDriver> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_THUMBNAILS_THUMBNAIL_CAPTURE_DRIVER_H_
