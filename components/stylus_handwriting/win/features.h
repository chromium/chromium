// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_STYLUS_HANDWRITING_WIN_FEATURES_H_
#define COMPONENTS_STYLUS_HANDWRITING_WIN_FEATURES_H_

#include "base/component_export.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace stylus_handwriting::win {

// Enables stylus writing for Windows platforms.
COMPONENT_EXPORT(STYLUS_HANDWRITING_WIN)
BASE_DECLARE_FEATURE(kStylusHandwritingWin);
COMPONENT_EXPORT(STYLUS_HANDWRITING_WIN)
extern bool IsStylusHandwritingWinEnabled();

// Enables remote configuration of how many "proximate" character bounding boxes
// are collected via field trials.
// The Windows Handwriting API will collect these bounds through two TSF APIs:
// - ITextStoreACP::GetTextExt
// - ITextStoreACP::GetACPFromPoint
//
// The more context available to the Windows Handwriting API, the more
// accurately it can determine whether the user performed a gesture, as well as
// which content the gesture should affect. For web contents these bounds are
// computed in the Renderer process, then sent over mojom to the Browser process
// so they can be cached long enough for the Windows Handwriting API to consume.
//
// For very large documents, this can be a significant cost, both in CPU time
// and memory, and can increase input latency for handwriting.
// ProximateBoundsCollectionHalfLimit is an optimization, limiting the amount of
// context collected at the cost of accuracy.
//
// The "half limit" is applied in both directions from a "pivot" character
// offset, therefore both positive and negative values have the same effect.
// A "half limit" of 0 disables bounds collection, disabling gesture support.
COMPONENT_EXPORT(STYLUS_HANDWRITING_WIN)
BASE_DECLARE_FEATURE(kProximateBoundsCollection);
// This is exposed for tests only, prefer `ProximateBoundsCollectionHalfLimit()`
COMPONENT_EXPORT(STYLUS_HANDWRITING_WIN)
BASE_DECLARE_FEATURE_PARAM(int, kProximateBoundsCollectionHalfLimit);
// The amount of character bounding boxes to collect in both directions from a
// "pivot" character offset.
COMPONENT_EXPORT(STYLUS_HANDWRITING_WIN)
extern uint32_t ProximateBoundsCollectionHalfLimit();

}  // namespace stylus_handwriting::win

#endif  // COMPONENTS_STYLUS_HANDWRITING_WIN_FEATURES_H_
