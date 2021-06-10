// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/external_constants.h"

#include <memory>
#include <utility>

namespace updater {

ExternalConstants::ExternalConstants(
    std::unique_ptr<ExternalConstants> next_provider)
    : next_provider_(std::move(next_provider)) {}

ExternalConstants::~ExternalConstants() = default;

}  // namespace updater
