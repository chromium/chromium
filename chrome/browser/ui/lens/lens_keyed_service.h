// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_LENS_LENS_KEYED_SERVICE_H_
#define CHROME_BROWSER_UI_LENS_LENS_KEYED_SERVICE_H_

#include "components/keyed_service/core/keyed_service.h"

// This class counts how many times the Lens EDU action chip has been shown in
// the current session. It is created on a per profile basis.
class LensKeyedService : public KeyedService {
 public:
  LensKeyedService();
  LensKeyedService(const LensKeyedService&) = delete;
  LensKeyedService& operator=(const LensKeyedService&) = delete;

  ~LensKeyedService() override;

  void IncrementActionChipShownCount();
  int GetActionChipShownCount();
  void SetActionChipShownCount(int value);

 private:
  int action_chip_shown_count_ = 0;
};

#endif  // CHROME_BROWSER_UI_LENS_LENS_KEYED_SERVICE_H_
