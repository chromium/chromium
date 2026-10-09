#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Migrates the legacy (pre-ipcz) Mojo Core implementation and its tests from
//mojo into //chromeos/ash/components/mojo_proxy/mojo_core.

The migration proceeds in two mechanical stages, each landed separately so that
they can be verified independently.

copy:
  Copies the files listed in FILES below from //mojo/<path> to
  //chromeos/ash/components/mojo_proxy/mojo_core/<path>.

transform:
  Rewrites the copied files in place:
    * include paths: "mojo/..." => "chromeos/ash/components/mojo_proxy/mojo_core/..."
    * include guards: recomputed from the new file paths.
    * namespaces: `mojo::` => `mojo_legacy::`
    * macros: `MOJO_...` => `MOJO_LEGACY_...`
    * C API headers and translation units: file-scope declarations are wrapped in
      `namespace mojo_legacy`, and `extern "C"` blocks are removed, so that the
      clone's C API symbols and types cannot collide with the real //mojo ones
      when both are linked into the same binary.

The file list covers the legacy subset of //mojo/core (with ports/ and
embedder/), the parts of //mojo/public that legacy Mojo Core needs (the C
system API, the C++ platform support library and a subset of the C++ system
wrappers), and the legacy Mojo Core tests with their test-support library, so
that the frozen clone keeps the unit-test coverage that //mojo will lose when
it drops legacy Mojo Core. ipcz-based code (core_ipcz, ipcz_driver), other
platforms' files (_win, _mac, _fuchsia), fuzzers and ipcz-only tests
(core_ipcz_test, ipcz_driver tests) are deliberately excluded.

//mojo/core/embedder/features is also excluded: the clone is frozen, so the
behavior currently selected by the default feature state should be hardcoded
instead.
"""

import argparse
import os
import re
import shutil
import sys

_SRC_ROOT = os.path.normpath(
  os.path.join(os.path.dirname(__file__), "..", "..", "..", "..", "..")
)
_SOURCE_DIR = os.path.join(_SRC_ROOT, "mojo")

_NEW_INCLUDE_PREFIX = "chromeos/ash/components/mojo_proxy/mojo_core/"
_TARGET_DIR = os.path.join(_SRC_ROOT, os.path.normpath(_NEW_INCLUDE_PREFIX))

# The frozen legacy Mojo Core implementation, paths relative to //mojo.
_IMPL_FILES = [
  # The legacy Mojo Core implementation (Linux/ChromeOS subset).
  "core/atomic_flag.cc",
  "core/atomic_flag.h",
  "core/broker.h",
  "core/broker_host.cc",
  "core/broker_host.h",
  "core/broker_messages.h",
  "core/broker_posix.cc",
  "core/channel.cc",
  "core/channel.h",
  "core/channel_linux.cc",
  "core/channel_linux.h",
  "core/channel_posix.cc",
  "core/channel_posix.h",
  "core/configuration.cc",
  "core/configuration.h",
  "core/connection_params.cc",
  "core/connection_params.h",
  "core/core.cc",
  "core/core.h",
  "core/data_pipe_consumer_dispatcher.cc",
  "core/data_pipe_consumer_dispatcher.h",
  "core/data_pipe_control_message.cc",
  "core/data_pipe_control_message.h",
  "core/data_pipe_producer_dispatcher.cc",
  "core/data_pipe_producer_dispatcher.h",
  "core/dispatcher.cc",
  "core/dispatcher.h",
  "core/entrypoints.cc",
  "core/entrypoints.h",
  "core/handle_signals_state.h",
  "core/handle_table.cc",
  "core/handle_table.h",
  "core/invitation_dispatcher.cc",
  "core/invitation_dispatcher.h",
  "core/message_pipe_dispatcher.cc",
  "core/message_pipe_dispatcher.h",
  "core/node_channel.cc",
  "core/node_channel.h",
  "core/node_controller.cc",
  "core/node_controller.h",
  "core/options_validation.h",
  "core/platform_handle_dispatcher.cc",
  "core/platform_handle_dispatcher.h",
  "core/platform_handle_in_transit.cc",
  "core/platform_handle_in_transit.h",
  "core/platform_handle_utils.cc",
  "core/platform_handle_utils.h",
  "core/platform_shared_memory_mapping.cc",
  "core/platform_shared_memory_mapping.h",
  "core/request_context.cc",
  "core/request_context.h",
  "core/shared_buffer_dispatcher.cc",
  "core/shared_buffer_dispatcher.h",
  "core/system_impl_export.h",
  "core/user_message_impl.cc",
  "core/user_message_impl.h",
  "core/watch.cc",
  "core/watch.h",
  "core/watcher_dispatcher.cc",
  "core/watcher_dispatcher.h",
  "core/watcher_set.cc",
  "core/watcher_set.h",
  # The ports library underlying legacy message pipes.
  "core/ports/event.cc",
  "core/ports/event.h",
  "core/ports/message_filter.h",
  "core/ports/message_queue.cc",
  "core/ports/message_queue.h",
  "core/ports/name.cc",
  "core/ports/name.h",
  "core/ports/node.cc",
  "core/ports/node.h",
  "core/ports/node_delegate.h",
  "core/ports/port.cc",
  "core/ports/port.h",
  "core/ports/port_locker.cc",
  "core/ports/port_locker.h",
  "core/ports/port_ref.cc",
  "core/ports/port_ref.h",
  "core/ports/user_data.h",
  "core/ports/user_message.cc",
  "core/ports/user_message.h",
  # The embedder API (Init/ShutDown and IO thread support).
  "core/embedder/configuration.h",
  "core/embedder/embedder.cc",
  "core/embedder/embedder.h",
  "core/embedder/process_error_callback.h",
  "core/embedder/scoped_ipc_support.cc",
  "core/embedder/scoped_ipc_support.h",
  # The C system API (types and thunks).
  "public/c/system/buffer.h",
  "public/c/system/core.h",
  "public/c/system/data_pipe.h",
  "public/c/system/functions.h",
  "public/c/system/invitation.h",
  "public/c/system/macros.h",
  "public/c/system/message_pipe.h",
  "public/c/system/platform_handle.h",
  "public/c/system/quota.h",
  "public/c/system/system_export.h",
  "public/c/system/thunks.cc",
  "public/c/system/thunks.h",
  "public/c/system/trap.h",
  "public/c/system/types.h",
  # The C++ platform support library (Linux/ChromeOS subset).
  "public/cpp/platform/named_platform_channel.cc",
  "public/cpp/platform/named_platform_channel.h",
  "public/cpp/platform/named_platform_channel_posix.cc",
  "public/cpp/platform/platform_channel.cc",
  "public/cpp/platform/platform_channel.h",
  "public/cpp/platform/platform_channel_endpoint.cc",
  "public/cpp/platform/platform_channel_endpoint.h",
  "public/cpp/platform/platform_channel_server.cc",
  "public/cpp/platform/platform_channel_server.h",
  "public/cpp/platform/platform_channel_server_endpoint.cc",
  "public/cpp/platform/platform_channel_server_endpoint.h",
  "public/cpp/platform/platform_channel_server_posix.cc",
  "public/cpp/platform/platform_handle.cc",
  "public/cpp/platform/platform_handle.h",
  "public/cpp/platform/platform_handle_internal.h",
  "public/cpp/platform/socket_utils_posix.cc",
  "public/cpp/platform/socket_utils_posix.h",
  # The C++ system wrappers used by legacy Mojo Core embedders and tests.
  "public/cpp/system/buffer.cc",
  "public/cpp/system/buffer.h",
  "public/cpp/system/data_pipe.h",
  "public/cpp/system/handle.h",
  "public/cpp/system/handle_signals_state.h",
  "public/cpp/system/invitation.cc",
  "public/cpp/system/invitation.h",
  "public/cpp/system/isolated_connection.cc",
  "public/cpp/system/isolated_connection.h",
  "public/cpp/system/message.cc",
  "public/cpp/system/message.h",
  "public/cpp/system/message_pipe.cc",
  "public/cpp/system/message_pipe.h",
  "public/cpp/system/platform_handle.cc",
  "public/cpp/system/platform_handle.h",
  "public/cpp/system/simple_watcher.cc",
  "public/cpp/system/simple_watcher.h",
  "public/cpp/system/system_export.h",
  "public/cpp/system/trap.cc",
  "public/cpp/system/trap.h",
  "public/cpp/system/wait.cc",
  "public/cpp/system/wait.h",
]

# The legacy Mojo Core tests and their test-support library. Kept so the frozen
# clone retains the unit-test coverage that //mojo loses when it drops legacy
# Mojo Core.
_TEST_FILES = [
  # The C/C++ test-support shim (perf logging etc.).
  "public/c/test_support/test_support.h",
  "public/tests/test_support_private.cc",
  "public/tests/test_support_private.h",
  # The Mojo Core test-support library (gtest base, multiprocess helpers,
  # per-test core initialization).
  "core/test/mock_node_channel_delegate.cc",
  "core/test/mock_node_channel_delegate.h",
  "core/test/mojo_test_base.cc",
  "core/test/mojo_test_base.h",
  "core/test/mojo_test_suite_base.cc",
  "core/test/mojo_test_suite_base.h",
  "core/test/multiprocess_test_helper.cc",
  "core/test/multiprocess_test_helper.h",
  "core/test/run_all_unittests.cc",
  "core/test/scoped_mojo_support.cc",
  "core/test/scoped_mojo_support.h",
  "core/test/test_support_impl.cc",
  "core/test/test_support_impl.h",
  "core/test/test_switches.cc",
  "core/test/test_switches.h",
  "core/test/test_utils.cc",
  "core/test/test_utils.h",
  # The ports library tests.
  "core/ports/name_unittest.cc",
  "core/ports/ports_unittest.cc",
  # Legacy-internal unit tests.
  "core/core_test_base.cc",
  "core/core_test_base.h",
  "core/core_unittest.cc",
  "core/node_channel_unittest.cc",
  "core/node_controller_unittest.cc",
  "core/options_validation_unittest.cc",
  "core/platform_handle_dispatcher_unittest.cc",
  "core/quota_unittest.cc",
  "core/shared_buffer_dispatcher_unittest.cc",
  # Public-API tests, exercised here against the legacy implementation.
  "core/channel_unittest.cc",
  "core/data_pipe_unittest.cc",
  "core/embedder_unittest.cc",
  "core/invitation_unittest.cc",
  "core/message_pipe_unittest.cc",
  "core/message_unittest.cc",
  "core/multiprocess_message_pipe_unittest.cc",
  "core/platform_wrapper_unittest.cc",
  "core/shared_buffer_unittest.cc",
  "core/signals_unittest.cc",
  "core/trap_unittest.cc",
]

FILES = _IMPL_FILES + _TEST_FILES


def copy():
  for path in FILES:
    source = os.path.join(_SOURCE_DIR, path)
    target = os.path.join(_TARGET_DIR, path)
    os.makedirs(os.path.dirname(target), exist_ok=True)
    shutil.copyfile(source, target)
  print(f"Copied {len(FILES)} files to {_TARGET_DIR}")


# The C system API headers declare types and functions at file scope, with C
# linkage for the functions. To guarantee that the clone cannot collide with
# the real //mojo C API when both are linked into the same binary (the proxy
# links this clone for its legacy side and //mojo for its ipcz side), their
# entire contents are wrapped in `namespace mojo_legacy` and the `extern "C"`
# blocks are removed. "macros.h", "system_export.h" and "core.h" contain only
# macros and includes, so they don't need wrapping.
_WRAP_WHOLE_HEADERS = [
  "public/c/system/buffer.h",
  "public/c/system/data_pipe.h",
  "public/c/system/functions.h",
  "public/c/system/invitation.h",
  "public/c/system/message_pipe.h",
  "public/c/system/platform_handle.h",
  "public/c/system/quota.h",
  "public/c/system/thunks.h",
  "public/c/system/trap.h",
  "public/c/system/types.h",
]

# "thunks.cc" defines the C API functions (and helpers in an anonymous
# namespace); the whole translation unit is wrapped in `namespace mojo_legacy`.
_WRAP_WHOLE_TUS = [
  "public/c/system/thunks.cc",
]

# "entrypoints.cc" defines its C API shims inside an anonymous namespace, then
# defines `mojo::core::GetSystemThunks()` etc. in a separate namespace block.
# Only the anonymous namespace is wrapped (the other block is renamed by the
# regular namespace rewrite).
_WRAP_ANON_NAMESPACE_TUS = [
  "core/entrypoints.cc",
]

_EXTERN_C_GUARDED_OPEN = '#ifdef __cplusplus\nextern "C" {\n#endif\n'
_EXTERN_C_GUARDED_CLOSE = '#ifdef __cplusplus\n}  // extern "C"\n#endif\n'
_EXTERN_C_OPEN = 'extern "C" {\n'
_EXTERN_C_CLOSE = '}  // extern "C"\n'

_NAMESPACE_OPEN = "namespace mojo_legacy {\n"
_NAMESPACE_CLOSE = "}  // namespace mojo_legacy\n"


def _include_guard(path):
  return re.sub(r"[^A-Z0-9]", "_", path.upper()) + "_"


def _remove_extern_c(path, text):
  if _EXTERN_C_GUARDED_OPEN in text:
    assert text.count(_EXTERN_C_GUARDED_OPEN) == 1, path
    assert text.count(_EXTERN_C_GUARDED_CLOSE) == 1, path
    text = text.replace(_EXTERN_C_GUARDED_OPEN, "")
    text = text.replace(_EXTERN_C_GUARDED_CLOSE, "")
  assert text.count(_EXTERN_C_OPEN) <= 1, path
  text = text.replace(_EXTERN_C_OPEN, "")
  text = text.replace(_EXTERN_C_CLOSE, "")
  assert 'extern "C"' not in text, path
  return text


def _wrap_after_includes(path, text, up_to=None):
  """Wraps everything after the last #include (and before `up_to`, if given)
  in namespace mojo_legacy."""
  includes = list(re.finditer(r"^#include .*\n", text, re.MULTILINE))
  assert includes, path
  start = includes[-1].end()
  if up_to is not None:
    index = text.rindex(up_to)
    assert index > start, path
    return (
      text[:start]
      + "\n"
      + _NAMESPACE_OPEN
      + text[start:index].strip()
      + "\n\n"
      + _NAMESPACE_CLOSE
      + "\n"
      + text[index:]
    )
  return (
    text[:start]
    + "\n"
    + _NAMESPACE_OPEN
    + text[start:].strip()
    + "\n\n"
    + _NAMESPACE_CLOSE
  )


def _transform_file(path):
  target = os.path.join(_TARGET_DIR, path)
  with open(target) as f:
    text = f.read()

  # Rewrite include paths.
  text = text.replace('#include "mojo/', f'#include "{_NEW_INCLUDE_PREFIX}')

  # Recompute include guards from the new paths.
  if path.endswith(".h"):
    old_guard = _include_guard("mojo/" + path)
    new_guard = _include_guard(_NEW_INCLUDE_PREFIX + path)
    assert text.count(old_guard) == 3, (path, old_guard)
    text = text.replace(old_guard, new_guard)

  # Namespace the C API.
  if path in _WRAP_WHOLE_HEADERS:
    text = _remove_extern_c(path, text)
    final_endif = f"#endif  // {_include_guard(_NEW_INCLUDE_PREFIX + path)}\n"
    assert text.endswith(final_endif), path
    text = _wrap_after_includes(path, text, up_to=final_endif)
  elif path in _WRAP_WHOLE_TUS:
    text = _remove_extern_c(path, text)
    text = _wrap_after_includes(path, text)
  elif path in _WRAP_ANON_NAMESPACE_TUS:
    text = _remove_extern_c(path, text)
    assert text.count("\nnamespace {\n") == 1, path
    assert text.count("\n}  // namespace\n") == 1, path
    text = text.replace(
      "\nnamespace {\n", "\n" + _NAMESPACE_OPEN + "\nnamespace {\n"
    )
    text = text.replace(
      "\n}  // namespace\n", "\n}  // namespace\n\n" + _NAMESPACE_CLOSE
    )

  # Rename the `mojo` namespace to `mojo_legacy`.
  text = re.sub(r"\bmojo::", "mojo_legacy::", text)
  text = re.sub(r"\bnamespace mojo\b", "namespace mojo_legacy", text)

  # Rename `MOJO_` macros to `MOJO_LEGACY_`. The recomputed include guards
  # above no longer start with `MOJO_`, so they are unaffected.
  text = re.sub(r"\bMOJO_", "MOJO_LEGACY_", text)

  with open(target, "w") as f:
    f.write(text)


def transform():
  for path in FILES:
    _transform_file(path)

  # Audit: no //mojo includes, `mojo::` references or `extern "C"`.
  for path in FILES:
    with open(os.path.join(_TARGET_DIR, path)) as f:
      text = f.read()
    assert '#include "mojo/' not in text, path
    assert not re.search(r"\bmojo::", text), path
    assert not re.search(r"\bnamespace mojo\b", text), path
    assert not re.search(r"\bMOJO_(?!LEGACY_)", text), path
    if path in (
      _WRAP_WHOLE_HEADERS + _WRAP_WHOLE_TUS + _WRAP_ANON_NAMESPACE_TUS
    ):
      assert 'extern "C"' not in text, path
  print(f"Transformed {len(FILES)} files in {_TARGET_DIR}")


def main():
  parser = argparse.ArgumentParser(description=__doc__)
  parser.add_argument("stage", choices=["copy", "transform"])
  args = parser.parse_args()
  if args.stage == "copy":
    copy()
  elif args.stage == "transform":
    transform()
  else:
    assert False
  return 0


if __name__ == "__main__":
  sys.exit(main())
