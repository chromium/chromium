// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/graphics/cast_external_begin_frame_source.h"

#include "base/time/time.h"
#include "chromecast/graphics/cast_window_manager_aura.h"
#include "chromecast/graphics/cast_window_tree_host_aura.h"

namespace chromecast {
namespace {
constexpr base::TimeDelta kFrameInterval =
    base::TimeDelta::FromMilliseconds(250);
constexpr uint64_t kSourceId = viz::BeginFrameArgs::kManualSourceId;
}  // namespace

CastExternalBeginFrameSource::CastExternalBeginFrameSource(
    CastWindowManagerAura* cast_window_manager_aura)
    : cast_window_manager_aura_(cast_window_manager_aura),
      sequence_number_(viz::BeginFrameArgs::kStartingFrameNumber),
      weak_factory_(this) {
  IssueExternalBeginFrame();
}

CastExternalBeginFrameSource::~CastExternalBeginFrameSource() {}

void CastExternalBeginFrameSource::OnFrameComplete(
    const viz::BeginFrameAck& ack) {
  DCHECK_EQ(kSourceId, ack.source_id);
  timer_.Start(
      FROM_HERE, kFrameInterval,
      base::BindOnce(&CastExternalBeginFrameSource::IssueExternalBeginFrame,
                     base::Unretained(this)));
}

void CastExternalBeginFrameSource::IssueExternalBeginFrame() {
  const auto now = base::TimeTicks::Now();
  viz::BeginFrameArgs args = viz::BeginFrameArgs::Create(
      BEGINFRAME_FROM_HERE, kSourceId, sequence_number_, now,
      now + kFrameInterval, kFrameInterval, viz::BeginFrameArgs::NORMAL);
  ui::Compositor* compositor =
      cast_window_manager_aura_->window_tree_host()->compositor();
  compositor->context_factory_private()->IssueExternalBeginFrame(
      compositor, args, false,
      base::BindOnce(&CastExternalBeginFrameSource::OnFrameComplete,
                     weak_factory_.GetWeakPtr()));
  sequence_number_++;
}

}  // namespace chromecast
