// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/birch/birch_most_visited_provider.h"

#include "ash/birch/birch_model.h"
#include "ash/shell.h"

namespace ash {

BirchMostVisitedProvider::BirchMostVisitedProvider() = default;

BirchMostVisitedProvider::~BirchMostVisitedProvider() = default;

void BirchMostVisitedProvider::RequestBirchDataFetch() {
  // TODO(jamescook): Implement this.
  Shell::Get()->birch_model()->SetMostVisitedItems({});
}

}  // namespace ash
