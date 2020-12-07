// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MESSAGES_ANDROID_MESSAGE_UTILS_BRIDGE_H_
#define COMPONENTS_MESSAGES_ANDROID_MESSAGE_UTILS_BRIDGE_H_

namespace messages {

// C++ counterpart to MessageUtilsBridge.java.
class MessageUtilsBridge {
 public:
  static bool IsA11yEnabled();
};

}  // namespace messages

#endif  // COMPONENTS_MESSAGES_ANDROID_MESSAGE_UTILS_BRIDGE_H_