// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_group_visual_data.h"

#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "third_party/skia/include/utils/SkRandom.h"

TabGroupVisualData::TabGroupVisualData() {
  title_ = base::ASCIIToUTF16("");

  static SkRandom rand;
  color_ = rand.nextU() | 0xff000000;
}

TabGroupVisualData::TabGroupVisualData(base::string16 title, SkColor color)
    : title_(title), color_(color) {}
