// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CAST_RECEIVER_COMMON_PUBLIC_STATUS_H_
#define COMPONENTS_CAST_RECEIVER_COMMON_PUBLIC_STATUS_H_

namespace cast_receiver {

// TODO(crbug.com/1360597): Define a Status object with a bool() override in
// place of this typedef to provide better feedback signals without impacting
// any existing functionality.
typedef bool Status;

}  // namespace cast_receiver

#endif  // COMPONENTS_CAST_RECEIVER_COMMON_PUBLIC_STATUS_H_
