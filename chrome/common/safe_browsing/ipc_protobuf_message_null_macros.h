// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// No include guard, may be included multiple times.

// NULL out all the macros that need NULLing, so that multiple includes of
// *_generator.h files will not generate noise.
#undef IPC_PROTOBUF_MESSAGE_TRAITS_BEGIN
#undef IPC_PROTOBUF_MESSAGE_TRAITS_OPTIONAL_FUNDAMENTAL_MEMBER
#undef IPC_PROTOBUF_MESSAGE_TRAITS_OPTIONAL_COMPLEX_MEMBER
#undef IPC_PROTOBUF_MESSAGE_TRAITS_REPEATED_COMPLEX_MEMBER
#undef IPC_PROTOBUF_MESSAGE_TRAITS_END

#define IPC_PROTOBUF_MESSAGE_TRAITS_BEGIN(message_name)
#define IPC_PROTOBUF_MESSAGE_TRAITS_OPTIONAL_FUNDAMENTAL_MEMBER(name)
#define IPC_PROTOBUF_MESSAGE_TRAITS_OPTIONAL_COMPLEX_MEMBER(name)
#define IPC_PROTOBUF_MESSAGE_TRAITS_REPEATED_COMPLEX_MEMBER(name)
#define IPC_PROTOBUF_MESSAGE_TRAITS_END()
