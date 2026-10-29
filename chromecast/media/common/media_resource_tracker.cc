// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/common/media_resource_tracker.h"

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/task/bind_post_task.h"
#include "base/task/single_thread_task_runner.h"
#include "chromecast/public/cast_media_shlib.h"
#include "chromecast/public/volume_control.h"

namespace chromecast {
namespace media {

MediaResourceTracker::ScopedUsage::ScopedUsage(MediaResourceTracker* tracker)
    : tracker_(tracker) {
  CHECK(tracker_, base::NotFatalUntil::M163);
  CHECK(tracker_->media_task_runner_->BelongsToCurrentThread(),
        base::NotFatalUntil::M163);
  tracker_->IncrementUsageCount();
}

MediaResourceTracker::ScopedUsage::~ScopedUsage() {
  CHECK(tracker_->media_task_runner_->BelongsToCurrentThread(),
        base::NotFatalUntil::M163);
  tracker_->DecrementUsageCount();
}

MediaResourceTracker::MediaResourceTracker(
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> media_task_runner)
    : media_use_count_(0),
      media_lib_initialized_(false),
      delete_on_finalize_(false),
      ui_task_runner_(std::move(ui_task_runner)),
      media_task_runner_(std::move(media_task_runner)) {
  CHECK(ui_task_runner_, base::NotFatalUntil::M163);
  CHECK(media_task_runner_, base::NotFatalUntil::M163);
  CHECK(ui_task_runner_->BelongsToCurrentThread(), base::NotFatalUntil::M163);
}

MediaResourceTracker::~MediaResourceTracker() {}

void MediaResourceTracker::InitializeMediaLib() {
  CHECK(ui_task_runner_->BelongsToCurrentThread(), base::NotFatalUntil::M163);
  media_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&MediaResourceTracker::CallInitializeOnMediaThread,
                     base::Unretained(this)));
}

void MediaResourceTracker::FinalizeMediaLib(base::OnceClosure completion_cb) {
  CHECK(ui_task_runner_->BelongsToCurrentThread(), base::NotFatalUntil::M163);
  CHECK(!completion_cb.is_null(), base::NotFatalUntil::M163);

  media_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&MediaResourceTracker::MaybeCallFinalizeOnMediaThread,
                     base::Unretained(this), std::move(completion_cb)));
}

void MediaResourceTracker::FinalizeAndDestroy() {
  CHECK(ui_task_runner_->BelongsToCurrentThread(), base::NotFatalUntil::M163);

  media_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &MediaResourceTracker::MaybeCallFinalizeOnMediaThreadAndDeleteSelf,
          base::Unretained(this)));
}

void MediaResourceTracker::IncrementUsageCount() {
  CHECK(media_task_runner_->BelongsToCurrentThread(),
        base::NotFatalUntil::M163);
  CHECK(media_lib_initialized_, base::NotFatalUntil::M163);
  CHECK(!finalize_completion_cb_, base::NotFatalUntil::M163);
  media_use_count_++;
}

void MediaResourceTracker::DecrementUsageCount() {
  CHECK(media_task_runner_->BelongsToCurrentThread(),
        base::NotFatalUntil::M163);
  media_use_count_--;

  if (media_use_count_ == 0 &&
      (delete_on_finalize_ || finalize_completion_cb_)) {
    CallFinalizeOnMediaThread();
  }
}

void MediaResourceTracker::CallInitializeOnMediaThread() {
  CHECK(media_task_runner_->BelongsToCurrentThread(),
        base::NotFatalUntil::M163);
  if (media_lib_initialized_)
    return;

  DoInitializeMediaLib();
  media_lib_initialized_ = true;
}

void MediaResourceTracker::MaybeCallFinalizeOnMediaThread(
    base::OnceClosure completion_cb) {
  CHECK(media_task_runner_->BelongsToCurrentThread(),
        base::NotFatalUntil::M163);
  CHECK(!finalize_completion_cb_, base::NotFatalUntil::M163);

  finalize_completion_cb_ =
      base::BindPostTask(ui_task_runner_, std::move(completion_cb));
  if (!media_lib_initialized_) {
    if (finalize_completion_cb_)
      std::move(finalize_completion_cb_).Run();
    return;
  }

  // If there are things using media, we must wait for them to stop.
  // CallFinalize will get called later from DecrementUsageCount when
  // usage count drops to 0.
  if (media_use_count_ == 0)
    CallFinalizeOnMediaThread();
}

void MediaResourceTracker::MaybeCallFinalizeOnMediaThreadAndDeleteSelf() {
  CHECK(media_task_runner_->BelongsToCurrentThread(),
        base::NotFatalUntil::M163);

  if (!media_lib_initialized_) {
    ui_task_runner_->DeleteSoon(FROM_HERE, this);
    return;
  }

  delete_on_finalize_ = true;

  // If there are things using media, we must wait for them to stop.
  // CallFinalize will get called later from DecrementUsageCount when
  // usage count drops to 0.
  if (media_use_count_ == 0)
    CallFinalizeOnMediaThread();
}

void MediaResourceTracker::CallFinalizeOnMediaThread() {
  CHECK(media_task_runner_->BelongsToCurrentThread(),
        base::NotFatalUntil::M163);
  CHECK_EQ(media_use_count_, 0ul, base::NotFatalUntil::M163);
  CHECK(media_lib_initialized_, base::NotFatalUntil::M163);

  DoFinalizeMediaLib();
  media_lib_initialized_ = false;

  if (finalize_completion_cb_)
    std::move(finalize_completion_cb_).Run();

  if (delete_on_finalize_)
    ui_task_runner_->DeleteSoon(FROM_HERE, this);
}

void MediaResourceTracker::DoInitializeMediaLib() {
  base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
  media::CastMediaShlib::Initialize(cmd_line->argv());
  VolumeControl::Initialize(cmd_line->argv());
}

void MediaResourceTracker::DoFinalizeMediaLib() {
  CastMediaShlib::Finalize();
  VolumeControl::Finalize();
}

}  // namespace media
}  // namespace chromecast
