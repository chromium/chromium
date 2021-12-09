// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CAST_STREAMING_PUBLIC_CONSTANTS_H_
#define COMPONENTS_CAST_STREAMING_PUBLIC_CONSTANTS_H_

namespace cast_streaming {

// Name of the binding with which |ReceiverSession| communicates. Used for
// setting up the MessagePort.
extern const char kCastTransportBindingName[];

}  // namespace cast_streaming

#endif  // COMPONENTS_CAST_STREAMING_PUBLIC_CONSTANTS_H_
