// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Protobuf Messages over IPC
//
// Protobuf messages are registered with IPC_PROTOBUF_MESSAGE_TRAITS_BEGIN() and
// friends in much the same way as other externally-defined structs (see
// ipc/ipc_message_macros.h). These macros also cause only registration of the
// protobuf message type IPC with message generation. Within matching calls to
// _BEGIN() and _END(), one may use:
// - IPC_PROTOBUF_MESSAGE_TRAITS_OPTIONAL_FUNDAMENTAL_MEMBER() to register an
//   optional field of fundamental type (any scalar message field type save
//   "string" and "bytes").
// - IPC_PROTOBUF_MESSAGE_TRAITS_OPTIONAL_COMPLEX_MEMBER() to register an
//   optional field of complex type (scalar message field type "string" or
//   "bytes", or another message type).
// - IPC_PROTOBUF_MESSAGE_TRAITS_REPEATED_COMPLEX_MEMBER() to register a
//   repeated field of complex type (scalar message field type "string" or
//   "bytes", or another message type).
//
// Enum types in protobuf messages are registered with
// IPC_ENUM_TRAITS_VALIDATE() as with any other enum. In this case, the
// validation expression should be the _IsValid() function provided by the
// generated protobuf code. For example:
//
//     IPC_ENUM_TRAITS_VALIDATE(MyEnumType, MyEnumType_IsValid(value))

#ifndef CHROME_COMMON_SAFE_BROWSING_IPC_PROTOBUF_MESSAGE_MACROS_H_
#define CHROME_COMMON_SAFE_BROWSING_IPC_PROTOBUF_MESSAGE_MACROS_H_

#include <string>

#define IPC_PROTOBUF_MESSAGE_TRAITS_BEGIN(message_name)             \
  namespace IPC {                                                   \
  template <>                                                       \
  struct IPC_MESSAGE_EXPORT ParamTraits<message_name> {             \
    typedef message_name param_type;                                \
    static void Write(base::Pickle* m, const param_type& p);        \
    static bool Read(const base::Pickle* m,                         \
                     base::PickleIterator* iter,                    \
                     param_type* p);                                \
    static void Log(const param_type& p, std::string* l);           \
                                                                    \
   private:                                                         \
    template <class P>                                              \
    static bool ReadParamF(const base::Pickle* m,                   \
                           base::PickleIterator* iter,              \
                           param_type* p,                           \
                           void (param_type::*setter_function)(P)); \
  };                                                                \
  }  // namespace IPC

#define IPC_PROTOBUF_MESSAGE_TRAITS_OPTIONAL_FUNDAMENTAL_MEMBER(name)
#define IPC_PROTOBUF_MESSAGE_TRAITS_OPTIONAL_COMPLEX_MEMBER(name)
#define IPC_PROTOBUF_MESSAGE_TRAITS_REPEATED_COMPLEX_MEMBER(name)
#define IPC_PROTOBUF_MESSAGE_TRAITS_END()

#endif  // CHROME_COMMON_SAFE_BROWSING_IPC_PROTOBUF_MESSAGE_MACROS_H_
