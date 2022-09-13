#!/usr/bin/env python3
# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Lint as: python3
"""The tools provides lot of protoc related helper functions."""

import glob
import os
import subprocess

_protoc_path = None


def run_command(args, input):
    """Uses subprocess to execute the command line args."""
    proc = subprocess.run(
        args, input=input, stdout=subprocess.PIPE, stderr=subprocess.PIPE)

    if proc.returncode != 0:
        raise RuntimeError(proc.stderr.decode('utf-8'))

    return proc.stdout


def get_protoc_common_args(root_dir, proto_path):
    """Returns a list of protoc common args as a list."""
    result = ['-I' + os.path.join(root_dir)]
    full_path = os.path.join(root_dir, proto_path)
    if os.path.isdir(full_path):
        for root, _, files in os.walk(full_path):
            result += [
                os.path.join(root, f) for f in files if f.endswith('.proto')
            ]
    else:
        result += [full_path]
    return result


def encode_proto(text, message_name, root_dir, proto_path):
    """Calls a command line to encode the text string and returns binary
    bytes."""
    input_buffer = text
    if isinstance(input_buffer, str):
        input_buffer = text.encode()
    return run_command([protoc_path(root_dir), '--encode=' + message_name
                        ] + get_protoc_common_args(root_dir, proto_path),
                       input_buffer)


def decode_proto(data, message_name, root_dir, proto_path):
    """Calls a command line to decode the binary bytes array into text
    string."""
    return run_command([protoc_path(root_dir), '--decode=' + message_name
                        ] + get_protoc_common_args(root_dir, proto_path),
                       data).decode('utf-8')


def protoc_path(root_dir):
    """Returns the path to the proto compiler, protoc."""
    global _protoc_path
    if not _protoc_path:
        protoc_list = list(
            glob.glob(os.path.join(root_dir, "out") + "/*/protoc")) + list(
                glob.glob(os.path.join(root_dir, "out") + "/*/*/protoc"))
        if not len(protoc_list):
            print("Can't find a suitable build output directory",
                  "(it should have protoc)")
            sys.exit(1)
        _protoc_path = protoc_list[0]
    return _protoc_path
