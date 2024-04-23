#!/usr/bin/env python3
#
# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""MojoLPM Testcase Generator

This script must be used with MojoLPMGenerator and a RendererFuzzer. It
generates code to handle interface creation requested by MojoLPM by using the
interface brokers provided by the internal RendererFuzzer mechanism.

This script uses the jinja2 and the `testcase.h.tmpl` template to generate C++
code. A class named `RendererTestcase` will be created.
"""

from __future__ import annotations

import abc
import argparse
import dataclasses
import os
import pathlib
import re
import sys

import typing
import enum

# Copied from //mojo/public/tools/mojom/mojom/fileutil.py.
def AddLocalRepoThirdPartyDirToModulePath():
  """Helper function to find the top-level directory of this script's repository
  assuming the script falls somewhere within a 'chrome' directory, and insert
  the top-level 'third_party' directory early in the module search path. Used to
  ensure that third-party dependencies provided within the repository itself
  (e.g. Chromium sources include snapshots of jinja2 and ply) are preferred over
  locally installed system library packages."""
  def _GetDirAbove(dirname: str):
    """Returns the directory "above" this file containing |dirname| (which must
    also be "above" this file)."""
    path = os.path.abspath(__file__)
    while True:
      path, tail = os.path.split(path)
      if not tail:
        return None
      if tail == dirname:
        return path

  toplevel_dir = _GetDirAbove('chrome')
  if toplevel_dir:
    sys.path.insert(1, os.path.join(toplevel_dir, 'third_party'))

# This is needed in order to be able to import jinja2.
AddLocalRepoThirdPartyDirToModulePath()

import jinja2

def split_interface_name(interface: str):
  """Helper that splits a qualified mojo interface name into a dictionary
  containing the key 'name' that contains the name of the interface, and the
  key 'namespace' that contains its mojo namespace.

  Args:
      interface (str): the qualified interface name

  Returns:
      a dict containing the actual interface name and its namespace.
  """
  components = interface.split('.')
  ns_id = snake_to_camel_case('_'.join(components[:-1]))
  # The camel casing used by protobuf is slightly different than the one used
  # by Mojo interfaces. See `str.title()` vs `str.capitalize()`.
  # For example, 'MyInterface2xVeryCool' should become `MyInterface2XVeryCool'
  # (notice the capital 'X').
  interface_id = snake_to_camel_case(camel_to_snake_case(components[-1]))
  return {
    "identifier": f"{ns_id}{interface_id}",
    "name": components[-1],
    "namespace": "::".join(components[:-1]),
  }

def snake_to_camel_case(snake_str: str) -> str:
  """Snake case to camel case conversion.

  Args:
      snake_str: the snake case identifier to convert.

  Returns:
     `snake_str` converted to a camel case identifier.
  """
  return "".join(x.title() for x in snake_str.lower().split("_"))


def camel_to_snake_case(name: str) -> str:
  """Camel case to snake case conversion.

  Args:
      name: the camel case identifier

  Returns:
      `name` converted to a snake case identifier.
  """
  # This regex matches an uppercase character that is not the first character.
  # It then inserts an underscore character at the matched positions. Since
  # this regex uses negative lookback and positive lookahead, it doesn't
  # consume characters, and thus the `sub` method won't replace any existing
  # character but only add the '_'.
  return re.sub(r"(?<!^)(?=[A-Z])", "_", name).lower()

def main():
  parser = argparse.ArgumentParser(
      description='Generate an IPC fuzzer based on MojoLPM Generator.')
  parser.add_argument(
      '-c',
      '--context',
      default=[],
      nargs='+',
      required=True,
      help="Context bound interfaces to fuzz.")
  parser.add_argument(
      '-p',
      '--process',
      default=[],
      nargs='+',
      required=True,
      help="Process bound interfaces to fuzz.")
  parser.add_argument(
      '-d',
      '--fuzzer_dir',
      required=True,
      help="The directory in which the MojoLPMGenerator fuzzer is generated.")
  parser.add_argument(
      '-n',
      '--name',
      required=True,
      help="""The name of the MojoLPMGenerator fuzzing target.
      This will used to deduce the name of the generated MojoLPM testcase.""")
  parser.add_argument(
      '-o',
      '--output',
      required=True,
      help="Output file name.")

  args = parser.parse_args()
  template_dir = os.path.dirname(os.path.abspath(__file__))
  environment = jinja2.Environment(loader=jinja2.FileSystemLoader(
      template_dir))
  template = environment.get_template('testcase.h.tmpl')
  fuzzer_path = os.path.join(args.fuzzer_dir, args.name)
  fuzzer_name = snake_to_camel_case(args.name)
  mojolpm_classname = f"mojolpmgenerator::{fuzzer_name}Testcase"
  context = {
    "filename": args.output,
    "mojolpm_generator_filepath": f"{fuzzer_path}.h",
    "mojolpm_generator_classname": mojolpm_classname,
    "process_interfaces": [split_interface_name(p) for p in args.process],
    "context_interfaces": [split_interface_name(c) for c in args.context],
  }
  with pathlib.Path(args.output).open(mode="w") as f:
    f.write(template.render(context))

if __name__ == "__main__":
  main()
