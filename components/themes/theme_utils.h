// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_THEMES_THEME_UTILS_H_
#define COMPONENTS_THEMES_THEME_UTILS_H_

#include "base/values.h"
#include "components/sync/protocol/theme_types.pb.h"

namespace themes {

// Converts a protobuf `NtpCustomBackground` message into a preference
// dictionary containing background metadata (URL, attributions, main color, etc.).
base::DictValue GetBackgroundDictFromProto(
    const sync_pb::NtpCustomBackground& ntp_background);

// Converts a background preference dictionary into a protobuf
// `NtpCustomBackground` message.
sync_pb::NtpCustomBackground GetProtoFromBackgroundDict(
    const base::DictValue& dict);

}  // namespace themes

#endif  // COMPONENTS_THEMES_THEME_UTILS_H_
