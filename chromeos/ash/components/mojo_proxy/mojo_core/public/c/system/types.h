// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains types and constants/macros common to different Mojo system
// APIs.

#ifndef CHROMEOS_ASH_COMPONENTS_MOJO_PROXY_MOJO_CORE_PUBLIC_C_SYSTEM_TYPES_H_
#define CHROMEOS_ASH_COMPONENTS_MOJO_PROXY_MOJO_CORE_PUBLIC_C_SYSTEM_TYPES_H_

#include <stdint.h>

#include "base/memory/raw_ptr_exclusion.h"
#include "chromeos/ash/components/mojo_proxy/mojo_core/public/c/system/macros.h"

namespace mojo_legacy {
// |MojoTimeTicks|: A time delta, in microseconds, the meaning of which is
// source-dependent.

typedef int64_t MojoTimeTicks;

// |MojoHandle|: Handles to Mojo objects.
//   |MOJO_LEGACY_HANDLE_INVALID| - A value that is never a valid handle.

typedef uintptr_t MojoHandle;

#ifdef __cplusplus
inline constexpr MojoHandle MOJO_LEGACY_HANDLE_INVALID = 0;
#else
#define MOJO_LEGACY_HANDLE_INVALID ((MojoHandle)0)
#endif

// |MojoResult|: Result codes for Mojo operations. The only success code is zero
// (|MOJO_LEGACY_RESULT_OK|); all non-zero values should be considered as
// error/failure codes (even if the value is not recognized).
//   |MOJO_LEGACY_RESULT_OK| - Not an error; returned on success.
//   |MOJO_LEGACY_RESULT_CANCELLED| - Operation was cancelled, typically by
//       the caller.
//   |MOJO_LEGACY_RESULT_UNKNOWN| - Unknown error (e.g., if not enough
//       information is available for a more specific error).
//   |MOJO_LEGACY_RESULT_INVALID_ARGUMENT| - Caller specified an invalid
//       argument. This differs from |MOJO_LEGACY_RESULT_FAILED_PRECONDITION|
//       in that the former indicates arguments that are invalid regardless
//       of the state of the system.
//   |MOJO_LEGACY_RESULT_DEADLINE_EXCEEDED| - Deadline expired before the
//       operation could complete.
//   |MOJO_LEGACY_RESULT_NOT_FOUND| - Some requested entity was not found
//       (i.e., does not exist).
//   |MOJO_LEGACY_RESULT_ALREADY_EXISTS| - Some entity or condition that we
//       attempted to create already exists.
//   |MOJO_LEGACY_RESULT_PERMISSION_DENIED| - The caller does not have
//       permission for the operation (use
//       |MOJO_LEGACY_RESULT_RESOURCE_EXHAUSTED| for rejections caused by
//       exhausting some resource instead).
//   |MOJO_LEGACY_RESULT_RESOURCE_EXHAUSTED| - Some resource required for the
//       call (possibly some quota) has been exhausted.
//   |MOJO_LEGACY_RESULT_FAILED_PRECONDITION| - The system is not in a state
//       required for the operation (use this if the caller must do something
//       to rectify the state before retrying).
//   |MOJO_LEGACY_RESULT_ABORTED| - The operation was aborted by the system,
//       possibly due to a concurrency issue (use this if the caller may
//       retry at a higher level).
//   |MOJO_LEGACY_RESULT_OUT_OF_RANGE| - The operation was attempted past the
//       valid range. Unlike |MOJO_LEGACY_RESULT_INVALID_ARGUMENT|, this
//       indicates that the operation may be/become valid depending on the
//       system state. (This error is similar to
//       |MOJO_LEGACY_RESULT_FAILED_PRECONDITION|, but is more specific.)
//   |MOJO_LEGACY_RESULT_UNIMPLEMENTED| - The operation is not implemented,
//       supported, or enabled.
//   |MOJO_LEGACY_RESULT_INTERNAL| - Internal error: this should never happen
//       and indicates that some invariant expected by the system has been
//       broken.
//   |MOJO_LEGACY_RESULT_UNAVAILABLE| - The operation is (temporarily)
//       currently unavailable. The caller may simply retry the operation
//       (possibly with a backoff).
//   |MOJO_LEGACY_RESULT_DATA_LOSS| - Unrecoverable data loss or corruption.
//   |MOJO_LEGACY_RESULT_BUSY| - One of the resources involved is currently
//       being used (possibly on another thread) in a way that prevents the
//       current operation from proceeding, e.g., if the other operation may
//       result in the resource being invalidated.
//   |MOJO_LEGACY_RESULT_SHOULD_WAIT| - The request cannot currently be
//       completed (e.g., if the data requested is not yet available). The
//       caller should wait for it to be feasible using a trap.
//
// The codes from |MOJO_LEGACY_RESULT_OK| to |MOJO_LEGACY_RESULT_DATA_LOSS| come
// from Google3's canonical error codes.

typedef uint32_t MojoResult;

#ifdef __cplusplus
inline constexpr MojoResult MOJO_LEGACY_RESULT_OK = 0;
inline constexpr MojoResult MOJO_LEGACY_RESULT_CANCELLED = 1;
inline constexpr MojoResult MOJO_LEGACY_RESULT_UNKNOWN = 2;
inline constexpr MojoResult MOJO_LEGACY_RESULT_INVALID_ARGUMENT = 3;
inline constexpr MojoResult MOJO_LEGACY_RESULT_DEADLINE_EXCEEDED = 4;
inline constexpr MojoResult MOJO_LEGACY_RESULT_NOT_FOUND = 5;
inline constexpr MojoResult MOJO_LEGACY_RESULT_ALREADY_EXISTS = 6;
inline constexpr MojoResult MOJO_LEGACY_RESULT_PERMISSION_DENIED = 7;
inline constexpr MojoResult MOJO_LEGACY_RESULT_RESOURCE_EXHAUSTED = 8;
inline constexpr MojoResult MOJO_LEGACY_RESULT_FAILED_PRECONDITION = 9;
inline constexpr MojoResult MOJO_LEGACY_RESULT_ABORTED = 10;
inline constexpr MojoResult MOJO_LEGACY_RESULT_OUT_OF_RANGE = 11;
inline constexpr MojoResult MOJO_LEGACY_RESULT_UNIMPLEMENTED = 12;
inline constexpr MojoResult MOJO_LEGACY_RESULT_INTERNAL = 13;
inline constexpr MojoResult MOJO_LEGACY_RESULT_UNAVAILABLE = 14;
inline constexpr MojoResult MOJO_LEGACY_RESULT_DATA_LOSS = 15;
inline constexpr MojoResult MOJO_LEGACY_RESULT_BUSY = 16;
inline constexpr MojoResult MOJO_LEGACY_RESULT_SHOULD_WAIT = 17;
#else
#define MOJO_LEGACY_RESULT_OK ((MojoResult)0)
#define MOJO_LEGACY_RESULT_CANCELLED ((MojoResult)1)
#define MOJO_LEGACY_RESULT_UNKNOWN ((MojoResult)2)
#define MOJO_LEGACY_RESULT_INVALID_ARGUMENT ((MojoResult)3)
#define MOJO_LEGACY_RESULT_DEADLINE_EXCEEDED ((MojoResult)4)
#define MOJO_LEGACY_RESULT_NOT_FOUND ((MojoResult)5)
#define MOJO_LEGACY_RESULT_ALREADY_EXISTS ((MojoResult)6)
#define MOJO_LEGACY_RESULT_PERMISSION_DENIED ((MojoResult)7)
#define MOJO_LEGACY_RESULT_RESOURCE_EXHAUSTED ((MojoResult)8)
#define MOJO_LEGACY_RESULT_FAILED_PRECONDITION ((MojoResult)9)
#define MOJO_LEGACY_RESULT_ABORTED ((MojoResult)10)
#define MOJO_LEGACY_RESULT_OUT_OF_RANGE ((MojoResult)11)
#define MOJO_LEGACY_RESULT_UNIMPLEMENTED ((MojoResult)12)
#define MOJO_LEGACY_RESULT_INTERNAL ((MojoResult)13)
#define MOJO_LEGACY_RESULT_UNAVAILABLE ((MojoResult)14)
#define MOJO_LEGACY_RESULT_DATA_LOSS ((MojoResult)15)
#define MOJO_LEGACY_RESULT_BUSY ((MojoResult)16)
#define MOJO_LEGACY_RESULT_SHOULD_WAIT ((MojoResult)17)
#endif

// Flags passed to |MojoInitialize()| via |MojoInitializeOptions|.
typedef uint32_t MojoInitializeFlags;

// No flags.
#define MOJO_LEGACY_INITIALIZE_FLAG_NONE ((MojoInitializeFlags)0)

// The calling process will be initialized as the broker process for its IPC
// network. Any connected graph of Mojo consumers must have exactly one broker
// process. That process is always the first member of the network and it should
// set this flag during initialization. Attempts to invite a broker process into
// an existing network will always fail.
//
// This flag is ignored when |MOJO_LEGACY_INITIALIZE_FLAG_LOAD_ONLY| is set.
#define MOJO_LEGACY_INITIALIZE_FLAG_AS_BROKER ((MojoInitializeFlags)1)

// Even if not initialized as the broker process, the calling process will be
// configured for direct shared memory allocation. This can be used for
// non-broker processes which are still sufficiently privileged to allocate
// their own shared memory.
//
// This flag is ignored when |MOJO_LEGACY_INITIALIZE_FLAG_LOAD_ONLY| is set.
#define MOJO_LEGACY_INITIALIZE_FLAG_FORCE_DIRECT_SHARED_MEMORY_ALLOCATION \
  ((MojoInitializeFlags)2)

// This call to |MojoInitialize()| should NOT fully initialize Mojo's internal
// IPC support engine. Initialization is essentially a two-phase operation:
// first the library is loaded and its global state is initialized, and then
// full IPC support is initialized. The latter phase may spawn a background
// thread, thus making it hostile to certain scenarios (e.g. prior to a fork()
// on Linux et al); meanwhile the former phase may still need to be completed
// early, e.g. prior to some sandbox configuration which may precede a fork().
//
// Applications wishing to separate initialization into two phases can set
// this flag during an initial call to |MojoInitialize()|. To subsequently
// enable use of all Mojo APIs, |MojoInitialize()| must be called another time,
// without this flag set.
//
// Note that various MojoInitializeOptions may be ignored on the second call
// to |MojoInitialize()|, while others may actually override options passed to
// the first call. Documentation on individual option fields and flags clarifies
// this behavior.
#define MOJO_LEGACY_INITIALIZE_FLAG_LOAD_ONLY ((MojoInitializeFlags)4)

// Options passed to |MojoInitialize()|.
struct MOJO_LEGACY_ALIGNAS(8) MojoInitializeOptions {
  // The size of this structure, used for versioning.
  uint32_t struct_size;

  // See |MojoInitializeFlags|.
  MojoInitializeFlags flags;

  // Address and length of the UTF8-encoded path of a mojo_core shared library
  // to load. If the |mojo_core_path| is null then |mojo_core_path_length| is
  // ignored and Mojo will fall back first onto the
  // |MOJO_LEGACY_CORE_LIBRARY_PATH| environment variable, and then onto the
  // current working directory.
  //
  // NOTE: These fields are only observed during the first successful call to
  // |MojoInitialize()| in a process.
  MOJO_LEGACY_POINTER_FIELD(const char*, mojo_core_path);
  uint32_t mojo_core_path_length;

  // For POSIX and Fuchsia systems only, this is the |argc| and |argv| from
  // the calling process's main() entry point. These fields are ignored on
  // Windows, but we define them anyway for the sake of ABI consistency.
  //
  // NOTE: These fields are only observed during the first successful call to
  // |MojoInitialize()| within a process.
  int32_t argc;
  // RAW_PTR_EXCLUSION: this struct is part of the (frozen) Mojo C system
  // API, whose layout must remain C-compatible and stable.
  RAW_PTR_EXCLUSION MOJO_LEGACY_POINTER_FIELD(const char* const*, argv);
};
MOJO_LEGACY_STATIC_ASSERT(sizeof(struct MojoInitializeOptions) == 32,
                          "MojoInitializeOptions has wrong size");

// Flags passed to |MojoShutdown()| via |MojoShutdownOptions|.
typedef uint32_t MojoShutdownFlags;

// No flags.
#define MOJO_LEGACY_SHUTDOWN_FLAG_NONE ((MojoShutdownFlags)0)

// Options passed to |MojoShutdown()|.
struct MOJO_LEGACY_ALIGNAS(8) MojoShutdownOptions {
  // The size of this structure, used for versioning.
  uint32_t struct_size;

  // See |MojoShutdownFlags|.
  MojoShutdownFlags flags;
};
MOJO_LEGACY_STATIC_ASSERT(sizeof(struct MojoShutdownOptions) == 8,
                          "MojoShutdownOptions has wrong size");

// |MojoHandleSignals|: Used to specify signals that can be watched for on a
// handle (and which can be triggered), e.g., the ability to read or write to
// the handle.
//   |MOJO_LEGACY_HANDLE_SIGNAL_NONE| - No flags. A registered watch will
//       always fail to arm with |MOJO_LEGACY_RESULT_FAILED_PRECONDITION|
//       when watching for this.
//   |MOJO_LEGACY_HANDLE_SIGNAL_READABLE| - Can read (e.g., a message) from
//       the handle.
//   |MOJO_LEGACY_HANDLE_SIGNAL_WRITABLE| - Can write (e.g., a message) to
//       the handle.
//   |MOJO_LEGACY_HANDLE_SIGNAL_PEER_CLOSED| - The peer handle is closed.
//   |MOJO_LEGACY_HANDLE_SIGNAL_NEW_DATA_READABLE| - Can read data from a
//       data pipe consumer handle (implying
//       MOJO_LEGACY_HANDLE_SIGNAL_READABLE is also set), AND there is some
//       nonzero quantity of new data available on the pipe since the last
//       |MojoReadData()| or |MojoBeginReadData()| call on the handle.
//   |MOJO_LEGACY_HANDLE_SIGNAL_PEER_REMOTE| - The peer handle exists in a
//       remote execution context (e.g. in another process.) Note that this
//       signal is maintained with best effort but may at any time be
//       slightly out of sync with the actual location of the peer handle.
//   |MOJO_LEGACY_HANDLE_SIGNAL_QUOTA_EXCEEDED| - One or more quotas set on
//       the handle is currently exceeded.

typedef uint32_t MojoHandleSignals;

#ifdef __cplusplus
inline constexpr MojoHandleSignals MOJO_LEGACY_HANDLE_SIGNAL_NONE = 0;
inline constexpr MojoHandleSignals MOJO_LEGACY_HANDLE_SIGNAL_READABLE = 1 << 0;
inline constexpr MojoHandleSignals MOJO_LEGACY_HANDLE_SIGNAL_WRITABLE = 1 << 1;
inline constexpr MojoHandleSignals MOJO_LEGACY_HANDLE_SIGNAL_PEER_CLOSED = 1
                                                                           << 2;
inline constexpr MojoHandleSignals MOJO_LEGACY_HANDLE_SIGNAL_NEW_DATA_READABLE =
    1 << 3;
inline constexpr MojoHandleSignals MOJO_LEGACY_HANDLE_SIGNAL_PEER_REMOTE = 1
                                                                           << 4;
inline constexpr MojoHandleSignals MOJO_LEGACY_HANDLE_SIGNAL_QUOTA_EXCEEDED =
    1 << 5;
#else
#define MOJO_LEGACY_HANDLE_SIGNAL_NONE ((MojoHandleSignals)0)
#define MOJO_LEGACY_HANDLE_SIGNAL_READABLE ((MojoHandleSignals)1 << 0)
#define MOJO_LEGACY_HANDLE_SIGNAL_WRITABLE ((MojoHandleSignals)1 << 1)
#define MOJO_LEGACY_HANDLE_SIGNAL_PEER_CLOSED ((MojoHandleSignals)1 << 2)
#define MOJO_LEGACY_HANDLE_SIGNAL_NEW_DATA_READABLE ((MojoHandleSignals)1 << 3);
#define MOJO_LEGACY_HANDLE_SIGNAL_PEER_REMOTE ((MojoHandleSignals)1 << 4);
#define MOJO_LEGACY_HANDLE_SIGNAL_QUOTA_EXCEEDED ((MojoHandleSignals)1 << 5);
#endif

// |MojoHandleSignalsState|: Returned by watch notification callbacks and
// |MojoQueryHandleSignalsState| functions to indicate the signaling state of
// handles. Members are as follows:
//   - |satisfied signals|: Bitmask of signals that were satisfied at some time
//         before the call returned.
//   - |satisfiable signals|: These are the signals that are possible to
//         satisfy. For example, if the return value was
//         |MOJO_LEGACY_RESULT_FAILED_PRECONDITION|, you can use this field to
//         determine which, if any, of the signals can still be satisfied.
// Note: This struct is not extensible (and only has 32-bit quantities), so it's
// 32-bit-aligned.
MOJO_LEGACY_STATIC_ASSERT(MOJO_LEGACY_ALIGNOF(int32_t) == 4,
                          "int32_t has weird alignment");
struct MOJO_LEGACY_ALIGNAS(4) MojoHandleSignalsState {
  MojoHandleSignals satisfied_signals;
  MojoHandleSignals satisfiable_signals;
};
MOJO_LEGACY_STATIC_ASSERT(sizeof(struct MojoHandleSignalsState) == 8,
                          "MojoHandleSignalsState has wrong size");

// TODO(crbug.com/40565809): Remove these aliases.
#define MOJO_LEGACY_WATCH_CONDITION_SATISFIED \
  MOJO_LEGACY_TRIGGER_CONDITION_SIGNALS_SATISFIED
#define MOJO_LEGACY_WATCH_CONDITION_NOT_SATISFIED \
  MOJO_LEGACY_TRIGGER_CONDITION_SIGNALS_UNSATISFIED

}  // namespace mojo_legacy

#endif  // CHROMEOS_ASH_COMPONENTS_MOJO_PROXY_MOJO_CORE_PUBLIC_C_SYSTEM_TYPES_H_
