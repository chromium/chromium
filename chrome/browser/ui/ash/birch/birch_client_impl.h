// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_BIRCH_BIRCH_CLIENT_IMPL_H_
#define CHROME_BROWSER_UI_ASH_BIRCH_BIRCH_CLIENT_IMPL_H_

#include "ash/ash_export.h"
#include "ash/birch/birch_client.h"
#include "base/memory/raw_ptr.h"

class Profile;

namespace ash {

// Implementation of the birch client.
class BirchClientImpl : public BirchClient {
 public:
  explicit BirchClientImpl(Profile* profile);
  BirchClientImpl(const BirchClientImpl&) = delete;
  BirchClientImpl& operator=(const BirchClientImpl&) = delete;
  ~BirchClientImpl() override;

  // BirchClient:
  void RequestBirchDataFetch() override;

 private:
  const raw_ptr<Profile> profile_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_ASH_BIRCH_BIRCH_CLIENT_IMPL_H_
