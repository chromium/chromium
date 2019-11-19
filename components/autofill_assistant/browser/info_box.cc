// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/info_box.h"

#include <unordered_set>

namespace autofill_assistant {

InfoBox::InfoBox(const ShowInfoBoxProto& proto) : proto_(proto) {}

base::Value InfoBox::GetDebugContext() const {
  base::Value dict(base::Value::Type::DICTIONARY);
  if (!info_box().image_path().empty())
    dict.SetKey("image_path", base::Value(info_box().image_path()));
  if (!info_box().explanation().empty())
    dict.SetKey("explanation", base::Value(info_box().explanation()));
  return dict;
}

}  // namespace autofill_assistant
