// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/birch/birch_keyed_service.h"

#include <memory>
#include <optional>

#include "ash/birch/birch_model.h"
#include "ash/shell.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/birch/birch_file_suggest_provider.h"

namespace ash {

BirchKeyedService::BirchKeyedService(Profile* profile)
    : file_suggest_provider_(
          std::make_unique<BirchFileSuggestProvider>(profile)) {}

BirchKeyedService::~BirchKeyedService() = default;

}  // namespace ash
