// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_STORAGE_PRESSURE_BUBBLE_H_
#define CHROME_BROWSER_UI_STORAGE_PRESSURE_BUBBLE_H_

#include "url/origin.h"

namespace chrome {

// Shows a BubbleView that alerts the user about storage pressure.
void ShowStoragePressureBubble(const url::Origin& origin);

}  // namespace chrome

#endif  // CHROME_BROWSER_UI_STORAGE_PRESSURE_BUBBLE_H_
