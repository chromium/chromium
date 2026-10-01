# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import os
import multiprocessing.dummy
from typing import Callable, List, Any

REPOSITORY_ROOT = os.path.abspath(
    os.path.join(os.path.dirname(__file__), os.pardir, os.pardir, os.pardir)
)
OUT_DIR = os.path.join(REPOSITORY_ROOT, 'out')
ARCHS = ['x86', 'x64', 'arm', 'arm64', 'riscv64']


def is_rust_build_script(script: str) -> bool:
    return script == "//build/rust/gni_impl/run_build_script.py"


def is_supported_source_file(name):
    """Returns True if |name| can appear in a 'srcs' list."""
    return os.path.splitext(name)[1] in [
        '.c',
        '.cc',
        '.cpp',
        '.java',
        '.proto',
        '.S',
        '.aidl',
        '.rs',
    ]


CPP_VERSION = 'c++17'
tethering_apex = "com.android.tethering"


def get_toolchain_name(toolchain_label: str) -> str:
    return toolchain_label[toolchain_label.find(":") + 1 :]


def get_toolchain_label_from_label(target_label: str) -> str:
    return target_label[target_label.find('(') + 1 : -1]


def label_without_toolchain(label: str) -> str:
    """Strips the toolchain from a GN label.

    Return a GN label (e.g //buildtools:protobuf(//gn/standalone/toolchain:
    gcc_like_host) without the parenthesised toolchain part.
    """
    return label.split('(')[0]


def run_concurrently(
    func: Callable[..., Any], args_list: List[tuple]
) -> List[Any]:
    """Runs a function concurrently using a thread pool.

    Args:
        func: The function to run.
        args_list: A list of tuples, where each tuple contains the arguments
                   for one invocation of the function.

    Returns:
        A list of results in the same order as args_list.
    """
    if not args_list:
        return []
    with multiprocessing.dummy.Pool(len(args_list)) as pool:
        results = [pool.apply_async(func, args) for args in args_list]
        return [r.get() for r in results]
