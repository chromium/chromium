// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/time_serialization.h"

namespace ntp_snippets {

int64_t SerializeTime(const base::Time& time) {
  return (time - base::Time()).InMicroseconds();
}

base::Time DeserializeTime(int64_t serialized_time) {
  return base::Time() + base::Microseconds(serialized_time);
}

}  // namespace ntp_snippets
