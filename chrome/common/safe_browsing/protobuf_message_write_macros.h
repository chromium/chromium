// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_SAFE_BROWSING_PROTOBUF_MESSAGE_WRITE_MACROS_H_
#define CHROME_COMMON_SAFE_BROWSING_PROTOBUF_MESSAGE_WRITE_MACROS_H_

// Null out all the macros that need nulling.
#include "chrome/common/safe_browsing/ipc_protobuf_message_null_macros.h"

// Set up so next include will generate write methods.
#undef IPC_PROTOBUF_MESSAGE_TRAITS_BEGIN
#undef IPC_PROTOBUF_MESSAGE_TRAITS_OPTIONAL_FUNDAMENTAL_MEMBER
#undef IPC_PROTOBUF_MESSAGE_TRAITS_OPTIONAL_COMPLEX_MEMBER
#undef IPC_PROTOBUF_MESSAGE_TRAITS_REPEATED_COMPLEX_MEMBER
#undef IPC_PROTOBUF_MESSAGE_TRAITS_END

#define IPC_PROTOBUF_MESSAGE_TRAITS_BEGIN(message_name) \
  void ParamTraits<message_name>::Write(base::Pickle* m, const param_type& p) {
#define IPC_PROTOBUF_MESSAGE_TRAITS_OPTIONAL_COMPLEX_MEMBER \
  IPC_PROTOBUF_MESSAGE_TRAITS_OPTIONAL_FUNDAMENTAL_MEMBER
#define IPC_PROTOBUF_MESSAGE_TRAITS_OPTIONAL_FUNDAMENTAL_MEMBER(name) \
  if (p.has_##name()) {                                               \
    m->WriteBool(true);                                               \
    WriteParam(m, p.name());                                          \
  } else {                                                            \
    m->WriteBool(false);                                              \
  }
#define IPC_PROTOBUF_MESSAGE_TRAITS_REPEATED_COMPLEX_MEMBER(name) \
  WriteParam(m, p.name());
#define IPC_PROTOBUF_MESSAGE_TRAITS_END() }

#endif  // CHROME_COMMON_SAFE_BROWSING_PROTOBUF_MESSAGE_WRITE_MACROS_H_
