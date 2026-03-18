#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Generates data struct from GPU blocklist and driver bug workarounds json."""

import argparse
import json
import os
import platform
import subprocess
import sys

_LICENSE = """// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

"""

_DO_NOT_EDIT_WARNING = """// This file is auto-generated from
//    chromeos/ash/experiences/frozen_update/process_json.py
// It's formatted by clang-format using chromium coding style:
//    clang-format -i -style=chromium filename
// DO NOT EDIT!

"""


def process_json_file(script_dir, output_dir):
    json_path = os.path.join(script_dir, 'frozen_update_gpu_list.json')
    type_header_filename = 'frozen_gpu.h'
    type_path = os.path.join(script_dir, type_header_filename)
    output_header_filename = 'frozen_update_gpu_list_autogen.h'
    output_header_path = os.path.join(output_dir, output_header_filename)
    output_data_filename = 'frozen_update_gpu_list_autogen.cc'
    output_data_path = os.path.join(output_dir, output_data_filename)

    # Read data from json file
    with open(json_path, 'r', encoding='utf-8') as json_file:
        json_data = json.load(json_file)

    entry_count = len(json_data['entries'])
    entries_type = f"std::array<const FrozenGpu, {entry_count}>"

    # Build the entries string separately using a generator expression
    entries_str = '\n'.join(
        f"      {{ .vendor = {entry['vendor']}, .device = {entry['device']} }},"
        for entry in json_data['entries']
    )

    with open(output_data_path, 'w', encoding='utf-8') as data_file:
        cc_template = f"""{_LICENSE}
{_DO_NOT_EDIT_WARNING}
#include "chromeos/ash/experiences/frozen_update/{output_header_filename}"

#include <iterator>

#include "chromeos/ash/experiences/frozen_update/{type_header_filename}"

namespace ash {{

const base::span<const FrozenGpu> GetFrozenGpuEntries() {{
  static constexpr auto kFrozenGpuEntries = {entries_type}{{{{
{entries_str}
  }}}};
  return kFrozenGpuEntries;
}}

}}  // namespace ash
"""
        data_file.write(cc_template)

    # Write header file
    with open(output_header_path, 'w', encoding='utf-8') as header_file:
        path_dir = "chromeos/ash/experiences/frozen_update"
        file_guard_parts = [
            path_dir.upper().replace('/', '_'),
            output_header_filename.upper().replace('.', '_'),
            '' # For trailing underscore.
        ]
        file_guard = '_'.join(file_guard_parts)

        header_template = f"""{_LICENSE}
{_DO_NOT_EDIT_WARNING}
#ifndef {file_guard}
#define {file_guard}

#include <array>
#include "base/containers/span.h"

#include "chromeos/ash/experiences/frozen_update/{type_header_filename}"

namespace ash {{

const base::span<const FrozenGpu> GetFrozenGpuEntries();

}}  // namespace ash

#endif  // {file_guard}
"""
        header_file.write(header_template)


def main(argv):
    parser = argparse.ArgumentParser()
    parser.add_argument("--output-dir",
                        required=True,
                        help="Output directory for FrozenUpdateGpuList data "
                             "file. If unspecified, the file is not generated.")

    # argparse returns a Namespace object, not a tuple
    args = parser.parse_args(argv)
    if not os.path.exists(args.output_dir):
        parser.error(f"The specified output directory does not exist: "
                     f"{args.output_dir}")

    script_dir = os.path.dirname(os.path.realpath(__file__))
    process_json_file(script_dir, args.output_dir)


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
