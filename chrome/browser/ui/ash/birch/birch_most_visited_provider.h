// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_BIRCH_BIRCH_MOST_VISITED_PROVIDER_H_
#define CHROME_BROWSER_UI_ASH_BIRCH_BIRCH_MOST_VISITED_PROVIDER_H_

#include "ash/birch/birch_data_provider.h"

namespace ash {

// Managed the most frequently visited URLs for the birch feature. URLs are sent
// to 'BirchModel' to be stored.
class BirchMostVisitedProvider : public BirchDataProvider {
 public:
  BirchMostVisitedProvider();
  BirchMostVisitedProvider(const BirchMostVisitedProvider&) = delete;
  BirchMostVisitedProvider& operator=(const BirchMostVisitedProvider&) = delete;
  ~BirchMostVisitedProvider() override;

  // BirchDataProvider:
  void RequestBirchDataFetch() override;
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_ASH_BIRCH_BIRCH_MOST_VISITED_PROVIDER_H_
