# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import os.path
import subprocess
import sys
from typing import Any, Mapping

import wayland_protocol_c_arg_handling
import wayland_protocol_construction
import wayland_protocol_data_classes
import wayland_protocol_externals
import wayland_protocol_identifiers


def expand_template(template: str, context: Mapping[str, Any]) -> str:
    """Expands the template using context, and returns the result"""

    # Loaded from third_party/jinja2 after a sys.path modification
    import jinja2

    env = jinja2.Environment(
        loader=jinja2.FileSystemLoader(os.path.dirname(template)),
        keep_trailing_newline=True,  # newline-terminate generated files
        lstrip_blocks=True,
        trim_blocks=True)  # so don't need {%- -%} everywhere

    # Additional global functions
    env.globals['get_c_arg_for_server_request_arg'] = (
        wayland_protocol_c_arg_handling.get_c_arg_for_server_request_arg)
    env.globals['get_c_arg_for_server_event_arg'] = (
        wayland_protocol_c_arg_handling.get_c_arg_for_server_event_arg)
    env.globals['get_c_return_type_for_client_request'] = (
        wayland_protocol_c_arg_handling.get_c_return_type_for_client_request)
    env.globals['get_c_arg_for_client_request_arg'] = (
        wayland_protocol_c_arg_handling.get_c_arg_for_client_request_arg)
    env.globals['get_c_arg_for_client_event_arg'] = (
        wayland_protocol_c_arg_handling.get_c_arg_for_client_event_arg)
    env.globals['get_construction_steps'] = (
        wayland_protocol_construction.get_construction_steps)
    env.globals['get_destructor'] = (
        wayland_protocol_construction.get_destructor)
    env.globals['get_external_interfaces_for_protocol'] = (
        wayland_protocol_externals.get_external_interfaces_for_protocol)
    env.globals['get_minimum_version_to_construct'] = (
        wayland_protocol_construction.get_minimum_version_to_construct)
    env.globals['get_versions_to_test_for_event_delivery'] = (
        wayland_protocol_construction.get_versions_to_test_for_event_delivery)
    env.globals['is_global_interface'] = (
        wayland_protocol_construction.is_global_interface)

    # Additional filters for transforming text
    env.filters['kebab'] = wayland_protocol_identifiers.kebab_case
    env.filters['pascal'] = wayland_protocol_identifiers.pascal_case

    return env.get_template(os.path.basename(template)).render(context)


def clang_format_source_text(source_text: str, clang_format_path: str,
                             effective_filename: str) -> str:
    """Runs clang-format on source_text and returns the result."""
    # clang-format the output, for better readability and for
    # -Wmisleading-indentation.
    return subprocess.run(
        [clang_format_path, '--assume-filename', effective_filename],
        input=source_text,
        capture_output=True,
        check=True,
        text=True).stdout


def write_if_changed(source_text: str, output: str) -> str:
    """Writes source_text to output, but only if different."""
    if os.path.exists(output):
        with open(output, 'rt') as infile:
            if infile.read() == source_text:
                return

    with open(output, 'wt') as outfile:
        outfile.write(source_text)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('third_party_path')
    parser.add_argument('clang_format_path')
    parser.add_argument('template')
    parser.add_argument('output')
    parser.add_argument('protocols', nargs='+')
    args = parser.parse_args()

    # Allows us to import jinga2
    sys.path.append(args.third_party_path)

    protocols = wayland_protocol_data_classes.Protocols.parse_xml_files(
        args.protocols)

    if len(protocols.protocols) == 1:
        expanded = expand_template(args.template,
                                   {'protocol': protocols.protocols[0]})
    else:
        expanded = expand_template(args.template,
                                   {'protocols': protocols.protocols})

    formatted = clang_format_source_text(expanded, args.clang_format_path,
                                         args.output)
    write_if_changed(formatted, args.output)


if __name__ == '__main__':
    main()
