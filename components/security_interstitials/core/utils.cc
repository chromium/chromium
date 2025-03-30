// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/security_interstitials/core/utils.h"

#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"

namespace security_interstitials {

void AdjustFontSize(base::Value::Dict& load_time_data,
                    float font_size_multiplier) {
  std::string* value = load_time_data.FindString("fontsize");
  CHECK(value);
  std::string old_size = *value;
  // `old_size` should be in form of "75%".
  CHECK(old_size.size() > 1 && old_size.back() == '%');
  double new_size = 75.0;
  bool converted =
      base::StringToDouble(old_size.substr(0, old_size.size() - 1), &new_size);
  CHECK(converted);
  new_size *= font_size_multiplier;
  load_time_data.Set("fontsize", base::StringPrintf("%.0lf%%", new_size));
}

}  // namespace security_interstitials
