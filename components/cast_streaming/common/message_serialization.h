// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CAST_STREAMING_COMMON_MESSAGE_SERIALIZATION_H_
#define COMPONENTS_CAST_STREAMING_COMMON_MESSAGE_SERIALIZATION_H_

#include <string>
#include <string_view>

namespace cast_streaming {

// TODO(b/156118960): Remove all these when Cast messages are handled by Open
// Screen.
extern const char kMirroringNamespace[];
extern const char kRemotingNamespace[];
extern const char kSystemNamespace[];
extern const char kInjectNamespace[];
extern const char kMediaNamespace[];

extern const char kKeySenderId[];
extern const char kKeyNamespace[];
extern const char kKeyData[];
extern const char kKeyType[];
extern const char kKeyRequestId[];
extern const char kKeyCode[];
extern const char kKeyStatus[];

extern const char kValueSystemSenderId[];
extern const char kValueWrapped[];
extern const char kValueError[];
extern const char kValueMediaPlay[];
extern const char kValueMediaPause[];
extern const char kValueMediaGetStatus[];
extern const char kValueMediaStatus[];

extern const char kValueWrappedError[];
extern const char kValueInjectNotSupportedError[];

extern const char kInitialConnectMessage[];

// Parses |buffer| data into |sender_id|, |message_namespace| and |message|.
// Returns true on success.
bool DeserializeCastMessage(std::string_view buffer,
                            std::string* sender_id,
                            std::string* message_namespace,
                            std::string* message);

// Creates a message string out of the |sender_id|, |message_namespace| and
// |message|.
std::string SerializeCastMessage(const std::string& sender_id,
                                 const std::string& message_namespace,
                                 const std::string& message);

}  // namespace cast_streaming

#endif  // COMPONENTS_CAST_STREAMING_COMMON_MESSAGE_SERIALIZATION_H_
