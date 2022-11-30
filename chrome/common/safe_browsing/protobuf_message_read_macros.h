// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_SAFE_BROWSING_PROTOBUF_MESSAGE_READ_MACROS_H_
#define CHROME_COMMON_SAFE_BROWSING_PROTOBUF_MESSAGE_READ_MACROS_H_

// Null out all the macros that need nulling.
#include "chrome/common/safe_browsing/ipc_protobuf_message_null_macros.h"

// Set up so next include will generate read methods.
#undef IPC_PROTOBUF_MESSAGE_TRAITS_BEGIN
#undef IPC_PROTOBUF_MESSAGE_TRAITS_OPTIONAL_FUNDAMENTAL_MEMBER
#undef IPC_PROTOBUF_MESSAGE_TRAITS_OPTIONAL_COMPLEX_MEMBER
#undef IPC_PROTOBUF_MESSAGE_TRAITS_REPEATED_COMPLEX_MEMBER
#undef IPC_PROTOBUF_MESSAGE_TRAITS_END

#define IPC_PROTOBUF_MESSAGE_TRAITS_BEGIN(message_name)                 \
  template <class P>                                                    \
  bool ParamTraits<message_name>::ReadParamF(                           \
      const base::Pickle* m, base::PickleIterator* iter, param_type* p, \
      void (param_type::*setter_function)(P)) {                         \
    P value;                                                            \
    if (!ReadParam(m, iter, &value))                                    \
      return false;                                                     \
    (p->*setter_function)(value);                                       \
    return true;                                                        \
  }                                                                     \
  bool ParamTraits<message_name>::Read(                                 \
      const base::Pickle* m, base::PickleIterator* iter, param_type* p) {
#define IPC_PROTOBUF_MESSAGE_TRAITS_OPTIONAL_FUNDAMENTAL_MEMBER(name) \
    {                                                                 \
      bool is_present;                                                \
      if (!iter->ReadBool(&is_present))                               \
        return false;                                                 \
      if (!is_present)                                                \
        p->clear_##name();                                            \
      else if (!ReadParamF(m, iter, p, &param_type::set_##name))      \
        return false;                                                 \
    }

#define IPC_PROTOBUF_MESSAGE_TRAITS_OPTIONAL_COMPLEX_MEMBER(name)     \
    {                                                                 \
      bool is_present;                                                \
      if (!iter->ReadBool(&is_present))                               \
        return false;                                                 \
      if (!is_present)                                                \
        p->clear_##name();                                            \
      else if (!ReadParam(m, iter, p->mutable_##name()))              \
        return false;                                                 \
    }

#define IPC_PROTOBUF_MESSAGE_TRAITS_REPEATED_COMPLEX_MEMBER(name) \
    if (!ReadParam(m, iter, p->mutable_##name()))                 \
      return false;

#define IPC_PROTOBUF_MESSAGE_TRAITS_END() \
    return true;                          \
  }

#endif  // CHROME_COMMON_SAFE_BROWSING_PROTOBUF_MESSAGE_READ_MACROS_H_
