#!/usr/bin/env python3
#
# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""MojoLPM Testcase Generator

This script must be used with MojoLPMGenerator and a RendererFuzzer. It
generates code to handle interface creation requested by MojoLPM by using the
`ipc_interfaces_dumper` binary. It then formats a JSON file that
MojoLPMGenerator will understand.

JSON Format:
{
  # Those are the context (frame, document...) bound interfaces.
  "context_interfaces": [
    [
      "//path/to/mojom.mojom",
      "qualified.interface.name",
      "{Associated,}Remote",
    ],
    ...
  ],
  # Those are the process bound interfaces.
  "process_interfaces": [
    {
      "//path/to/mojom.mojom",
      "qualified.interface.name",
      "{Associated,}Remote",
    },
    ...
  ],
  # Those are the format MojoLPMGenerator undertands.
  # This groups all interfaces together.
  "interfaces": [
    {
      "gen/path/to/generator/file.mojom-module",
      "qualified.interface.name",
      "{Associated,}Remote",
    },
    ...
  ]
}

This script uses the jinja2 and the `testcase.h.tmpl` template to generate C++
code. A class named `RendererTestcase` will be created.
"""

from __future__ import annotations

import argparse
import json
import os
import pathlib
import re
import subprocess
import sys
import tempfile
import typing


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


SOURCE_DIR = _GetDirAbove('chrome')

sys.path.insert(1, os.path.join(SOURCE_DIR, 'third_party'))
sys.path.append(os.path.join(SOURCE_DIR, 'build'))
sys.path.append(os.path.join(SOURCE_DIR, 'mojo/public/tools/mojom/'))

from mojom.parse import parser as mojom_parser, ast
import action_helpers
import jinja2

XVFB_PATH = os.path.join(SOURCE_DIR, 'testing/xvfb.py')


def strip_end(text: str, suffix: str) -> str:
  """Similar to python 3.9 `removesuffix` function.

  Args:
      text: input text.
      suffix: the suffix to remove if present.

  Returns:
      the input string with suffix removed if present.
  """
  if suffix and text.endswith(suffix):
    return text[:-len(suffix)]
  return text


def get_all_interfaces(metadata_file: str) -> typing.List[str]:
  """Returns the list of every mojo interfaces in src_dir. Recurses through
  subdirectories.

  Args:
      src_dir: the chromium source directory.

  Returns:
      the list of paths to mojom interfaces.
  """
  res = []
  with open(metadata_file, 'r', encoding='utf-8') as file:
    lines = [line.rstrip() for line in file]
    for line in lines:
      with open(line, 'r') as metadata:
        data = json.load(metadata)
        for mojom_file in data['sources']:
          if mojom_file.endswith('.mojom'):
            path = os.path.join(os.path.dirname(line), mojom_file)
            path = os.path.abspath(path)
            res.append(path)
  return res


def is_defined_in_module(qualified_name: str, interface: ast.Mojom) -> bool:
  namespace = ".".join(qualified_name.split('.')[:-1])
  name = qualified_name.split('.')[-1]
  if not interface.module:
    return False
  m_namespace = str(interface.module.mojom_namespace)
  if m_namespace != namespace:
    return False

  if not interface.definition_list:
    return False
  for definition in interface.definition_list:
    if (isinstance(definition, ast.Interface) and
        str(definition.mojom_name) == name):
      return True
  return False



def find_matching_interface(qualified_name: str,
                            modules: typing.List[ast.Mojom]) -> str:
  """Finds the correct mojom file for the given interface. The interface name
  must be qualified.

  Args:
      qualified_name: the qualified interface name (e.g.
      'blink.mojom.BlobRegistry').
      modules: the list of parsed mojom modules.

  Returns:
      the path to the mojom file corresponding to the input interface.
  """
  for module in modules:
    if is_defined_in_module(qualified_name, module):
      return module.module.filename
  return None


def ensure_interface_deps_complete(interfaces: typing.List[str],
                                   modules: typing.List[ast.Mojom],
                                   build_dir: str):
  """Ensures that all the interfaces can be fetched from the parsed mojom
  modules.

  Args:
      interfaces: the list of interfaces (qualified names).
      modules: the list of mojom modules to search into.

  Raises:
      Exception: if at least one interface could not be found.
  """
  missing_interfaces = []
  for interface in interfaces:
    res = find_matching_interface(interface, modules)
    if not res:
      missing_interfaces.append(interface)
  if len(missing_interfaces) != 0:
    raise Exception('Missing browser exposed targets for the following '
                    'interfaces:\n'
                    f'{missing_interfaces}\n'
                    'Please add the corresponding targets to '
                    '`//chrome/browser_exposed_mojom_targets.gni`.')


def handle_interfaces(interfaces,
                      mojom_files: typing.List[ast.Mojom],
                      source_path: str,
                      output):
  """Finds the mojom files for the given interfaces and append the formatted
  result to the output list.

  Args:
      interfaces: the interfaces to handle.
      mojom_files: the list of parsed mojom files to look into.
      source_path: the path to chromium's root source directory.
      output: the output list.
  """
  for interface in interfaces:
    qualified_name = interface['qualified_name']
    interface_type = interface['type']
    path = pathlib.Path(find_matching_interface(qualified_name, mojom_files))
    path = path.relative_to(source_path)
    output.append([
      f"//{path}", qualified_name, interface_type
    ])


def filter_data(data):
  """Filters the JSON data. As for now, we filter out:
    - AssociatedRemote
    - Duplicate interfaces for context and process interfaces.

  Args:
      data: the JSON data.
  """
  data_filter = lambda x : x['type'] != 'AssociatedRemote'
  data['context_interfaces'] = list(filter(data_filter,
                                           data['context_interfaces']))
  data['process_interfaces'] = list(filter(data_filter,
                                           data['process_interfaces']))
  ctx_interfaces = [s['qualified_name'] for s in data['context_interfaces']]
  data_filter = lambda x: x['qualified_name'] not in ctx_interfaces
  data['process_interfaces'] = list(filter(data_filter,
                                           data['process_interfaces']))


def run_ipc_dumper(dumper_path: str, out_file: str):
  """This runs the ipc_dumper executable at `dumper_path` and redirects its
  output to `out_file` so that this tool can use it to generate the list of
  interfaces.

  Args:
      dumper_path: path to the `ipc_interfaces_dumper` executable.
      out_file: the file to which we'll dump the interfaces.
  """
  env = os.environ.copy()
  env["IPC_DUMP_PATH"] = out_file
  # Since we're running these at compile time, we need to make sure this will
  # run regardless of the building flags being used.
  # When enabling ASAN, we have a `detect_ord_violation` issue when running
  # this tool.
  env["ASAN_OPTIONS"] = 'detect_odr_violation=0'
  # The binary hosts a single browser test, so use --single-process-tests to
  # reduce overhead and prevent the test launcher from killing the dumper if
  # it takes more than 45 seconds (not unheard of in some configurations).
  args = [
      XVFB_PATH,
      os.path.abspath(dumper_path),
      '--single-process-tests',
  ]
  # TODO(349980051): crbug.com/349980051: when ubsan is enabled by default in
  # ASAN enabled builds, we had timeout issues running this binary.
  try:
    subprocess.run(args, capture_output=True, env=env, check=True)
  except subprocess.CalledProcessError as e:
    raise Exception(f'Command {args} failed (ret {e.returncode}) with:'
                    f'{e.output.decode(sys.getfilesystemencoding())}'
                    f'{e.stderr.decode(sys.getfilesystemencoding())}')


def generate_interfaces(ipc_interfaces_dumper: str,
                        interfaces_f: str,
                        gen_dir: str,
                        metadata_file: str,
                        depfile: str):
  """Generates the appropriate interfaces file given the output of the
  `ipc_interfaces_dumper`.

  Args:
      ipc_interfaces_dumper: the path to the `ipc_interfaces_dumper` binary.
      interfaces_f: the output path to the JSON interfaces file.
      gen_dir: the path to the root gen directory.
      metadata_file: the path to the mojo metadata file.
      depfile: the depfile to write to.
  """
  interfaces = get_all_interfaces(metadata_file)
  parsed_interfaces = []
  for interface in interfaces:
    with open(interface, 'r', encoding="utf-8") as f:
      parsed_interfaces.append(mojom_parser.Parse(f.read(), interface))
  output = {"context_interfaces": [], "process_interfaces": []}
  with tempfile.NamedTemporaryFile() as input_file:
    run_ipc_dumper(ipc_interfaces_dumper, input_file.name)
    with open(input_file.name, 'r') as in_f:
      data = json.load(in_f)
      filter_data(data)
      all_interfaces = data['context_interfaces'] + data['process_interfaces']
      qualified_names = [e['qualified_name'] for e in all_interfaces]
      ensure_interface_deps_complete(qualified_names,
                                     parsed_interfaces,
                                     os.path.join(gen_dir, os.pardir))
      handle_interfaces(data['context_interfaces'],
                        parsed_interfaces,
                        SOURCE_DIR,
                        output['context_interfaces'])
      handle_interfaces(data['process_interfaces'],
                        parsed_interfaces,
                        SOURCE_DIR,
                        output['process_interfaces'])

  # MojoLPMGenerator expects a particular format for generating MojoLPM
  # boilerplate. This part will generate the expected format and rebase the
  # mojom module paths in order for MojoLPMGenerator to be able to find them.
  output['interfaces'] = []
  for interface in output['context_interfaces'] + output['process_interfaces']:
    path = interface[0]
    path = os.path.join(gen_dir, path.lstrip('/')) + '-module'
    output['interfaces'].append([
      path, interface[1], interface[2]
    ])
  with action_helpers.atomic_output(interfaces_f, mode="w") as f:
    json.dump(output, f)

  # Now, we want to write the depfile so that ninja knows that we're depending
  # on the mojom files. If one gets modified, we want to re-run this action.
  all_interfaces = output['context_interfaces'] + output['process_interfaces']
  paths = [i[0].lstrip('//') for i in all_interfaces]
  paths = [pathlib.Path(os.path.join(SOURCE_DIR, p)) for p in paths]
  action_helpers.write_depfile(depfile,
                               interfaces_f,
                               [os.path.relpath(p) for p in paths])


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


def generate_testcase(interfaces_f: str,
                      fuzzer_dir: str,
                      fuzzer_name: str,
                      testcase_f: str):
  """Generates the testcase file given the interface list and the
  MojoLPMGenerator fuzzer name.

  Args:
      interfaces_f: the path to the JSON interface file.
      fuzzer_dir: the path to the fuzzer directory.
      fuzzer_name: the name of the MojoLPM fuzzer.
      testcase_f: the output path to the testcase .h file.
  """
  template_dir = os.path.dirname(os.path.abspath(__file__))
  environment = jinja2.Environment(loader=jinja2.FileSystemLoader(
      template_dir))
  template = environment.get_template('testcase.h.tmpl')
  fuzzer_path = os.path.join(fuzzer_dir, fuzzer_name)
  fuzzer_name = snake_to_camel_case(fuzzer_name)
  mojolpm_classname = f"mojolpmgenerator::{fuzzer_name}Testcase"
  with open(interfaces_f, 'r') as f:
    data = json.load(f)
    context = [c[1] for c in data['context_interfaces']]
    process = [p[1] for p in data['process_interfaces']]
    context = {
      "filename": testcase_f,
      "mojolpm_generator_filepath": f"{fuzzer_path}.h",
      "mojolpm_generator_classname": mojolpm_classname,
      "process_interfaces": [split_interface_name(p) for p in process],
      "context_interfaces": [split_interface_name(c) for c in context],
    }
    with action_helpers.atomic_output(testcase_f, mode="w") as f:
      f.write(template.render(context))


def main():
  parser = argparse.ArgumentParser(
      description='Generate an IPC fuzzer based on MojoLPM Generator.')
  parser.add_argument(
      '-p',
      '--path',
      required=True,
      help="The path to ipc_interfaces_dumper binary.")
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
      '-t',
      '--testcase-output-path',
      required=True,
      help="The path where the testcase file will be written to.")
  parser.add_argument(
      '-i',
      '--interface-output-path',
      required=True,
      help="The path where the interface file will be written to.")
  parser.add_argument(
      '-r',
      '--root-gen-dir',
      required=True,
      help="The path to the root gen dir.")
  parser.add_argument(
      '-m',
      '--metadata-file',
      required=True,
      help="Path to the metadata file.")
  parser.add_argument(
      '-f',
      '--depfile',
      required=True,
      help="The path to the depfile.")

  args = parser.parse_args()
  generate_interfaces(args.path,
                      args.interface_output_path,
                      args.root_gen_dir,
                      args.metadata_file,
                      args.depfile)
  generate_testcase(args.interface_output_path,
                    args.fuzzer_dir,
                    args.name,
                    args.testcase_output_path)


if __name__ == "__main__":
  main()
