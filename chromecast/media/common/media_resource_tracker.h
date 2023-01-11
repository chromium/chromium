// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_COMMON_MEDIA_RESOURCE_TRACKER_H_
#define CHROMECAST_MEDIA_COMMON_MEDIA_RESOURCE_TRACKER_H_

#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/task/sequenced_task_runner_helpers.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace chromecast {
namespace media {

// Keeps track of internal usage of resources that need access to underlying
// media playback hardware.  Some example users are the CMA pipeline, and the
// CDMs.  When it's time to release media resources, this class can be used
// to be wait and receive notification when all such users have stopped.
//
// Application should have one MediaResourceTracker instance and perform all
// CastMediaShlib::Initialize/Finalize through this interface.
// Threading model and lifetime:
// * This class interacts on both UI and media threads (task runners required
//   by ctor to perform thread hopping and checks). See function-level comments
//   on which thread to use for which operations.
// * The application should instantiate a single MediaResourceTracker instance.
//   Destruction should be performed by calling FinalizeAndDestroy from the UI
//   thread.
class MediaResourceTracker {
 public:
  // Helper class to manage media resource usage count.
  // Create an instance of this class when a media resource is created.
  // Delete the instance *after* the media resource is deleted.
  // This class is not thread-safe. It must be created and deleted on
  // |MediaResourceTracker::media_task_runner_|.
  class ScopedUsage {
   public:
    ScopedUsage(MediaResourceTracker* tracker);

    ScopedUsage(const ScopedUsage&) = delete;
    ScopedUsage& operator=(const ScopedUsage&) = delete;

    ~ScopedUsage();

   private:
    MediaResourceTracker* tracker_;
  };

  MediaResourceTracker(
      scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> media_task_runner);

  MediaResourceTracker(const MediaResourceTracker&) = delete;
  MediaResourceTracker& operator=(const MediaResourceTracker&) = delete;

  // Media resource acquire implementation. Must call on ui thread; runs
  // CastMediaShlib::Initialize on media thread.  Safe to call even if media lib
  // already initialized.
  void InitializeMediaLib();

  // Media resource release implementation:
  // (1) Waits for usage count to drop to zero
  // (2) Calls CastMediaShlib::Finalize on media thread
  // (3) Calls completion_cb
  // Must be called on UI thread.  Only one Finalize request may be in flight
  // at a time. |completion_cb| must not be null.
  void FinalizeMediaLib(base::OnceClosure completion_cb);

  // Shutdown process:
  // (1) Waits for usage count to drop to zero
  // (2) Calls CastMediaShlib::Finalize on media thread
  // (3) Deletes this object
  // Must be called on UI thread. No further calls should be made on UI thread
  // after this.
  void FinalizeAndDestroy();

  // Users of media resource (e.g. CMA pipeline) should call these when they
  // start and stop using media calls (must be called on media thread).
  void IncrementUsageCount();
  void DecrementUsageCount();

 private:
  friend class base::DeleteHelper<MediaResourceTracker>;
  friend class TestMediaResourceTracker;
  virtual ~MediaResourceTracker();

  // Tasks posted to media thread
  void CallInitializeOnMediaThread();
  void MaybeCallFinalizeOnMediaThread(base::OnceClosure completion_cb);
  void MaybeCallFinalizeOnMediaThreadAndDeleteSelf();
  void CallFinalizeOnMediaThread();

  // Hooks for testing
  virtual void DoInitializeMediaLib();
  virtual void DoFinalizeMediaLib();

  // Accessed on media thread + ctor
  size_t media_use_count_;
  bool media_lib_initialized_;
  base::OnceClosure finalize_completion_cb_;
  bool delete_on_finalize_;

  scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> media_task_runner_;
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_COMMON_MEDIA_RESOURCE_TRACKER_H_
