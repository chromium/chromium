// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MESSAGES_ANDROID_MESSAGE_ENUMS_H_
#define COMPONENTS_MESSAGES_ANDROID_MESSAGE_ENUMS_H_

namespace messages {

// List of constants describing the reasons why the message was dismissed.
// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.messages
enum class DismissReason {
  // Dismiss reasons that are fully controlled by clients (i.e. are not used
  // inside the Messages implementation are marked "Controlled by client" on
  // comments.

  UNKNOWN = 0,
  // A message was dismissed by clicking on the primary action button.
  PRIMARY_ACTION = 1,
  // Controlled by client: A message was dismissed by clicking on the secondary
  // action button.
  SECONDARY_ACTION = 2,
  // A message was automatically dismissed based on timer.
  TIMER = 3,
  // A message was dismissed by user gesture.
  GESTURE = 4,
  // Controlled by client: A message was dismissed in response to switching
  // tabs.
  TAB_SWITCHED = 5,
  // Controlled by client: A message was dismissed as a result of closing a tab.
  TAB_DESTROYED = 6,
  // Controlled by client: A message is dismissed because the activity is
  // destroyed.
  ACTIVITY_DESTROYED = 7,
  // A message was dismissed due to the destruction of the corresponding scopes.
  SCOPE_DESTROYED = 8
};

// The constants of message scope type.
//
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.messages
enum class MessageScopeType { WINDOW = 0, WEB_CONTENTS = 1, NAVIGATION = 2 };

}  // namespace messages

#endif  // COMPONENTS_MESSAGES_ANDROID_MESSAGE_ENUMS_H_
