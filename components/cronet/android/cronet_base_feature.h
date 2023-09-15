// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRONET_ANDROID_CRONET_BASE_FEATURE_H_
#define COMPONENTS_CRONET_ANDROID_CRONET_BASE_FEATURE_H_

#include "components/cronet/android/proto/base_feature_overrides.pb.h"

namespace cronet {

// Applies the base::Feature overrides in `overrides`, making them globally
// accessible through the standard base::Feature API.
//
// Note that this function mutates global state, and will affect the behavior
// of code accessing base::Features after this call.
//
// If base::Feature is already initialized, this function logs a warning and
// does nothing. (This is not supposed to happen in production, but it can
// happen in the context of some native tests that end up indirectly calling
// this under a base::test::ScopedFeatureList.)
void ApplyBaseFeatureOverrides(
    const ::org::chromium::net::httpflags::BaseFeatureOverrides& overrides);

}  // namespace cronet

#endif  // COMPONENTS_CRONET_ANDROID_CRONET_BASE_FEATURE_H_
