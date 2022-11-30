// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_items_collection/core/open_params.h"

namespace offline_items_collection {

OpenParams::OpenParams(LaunchLocation location)
    : launch_location(location), open_in_incognito(false) {}

OpenParams::OpenParams(const OpenParams& other) = default;

OpenParams::~OpenParams() = default;

}  // namespace offline_items_collection
