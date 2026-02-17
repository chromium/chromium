# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import argparse
import sys
import os

REPOSITORY_ROOT = os.path.abspath(
    os.path.join(os.path.dirname(__file__), os.pardir, os.pardir, os.pardir,
                 os.pardir))

sys.path.insert(0, REPOSITORY_ROOT)
import components.cronet.tools.utils as cronet_utils  # pylint: disable=wrong-import-position


def filter_undefined_only(line):
  return "*UND*" in line


def filter_anything_but_undefined(line):
  return "*UND*" not in line


def parse_args():
  parser = argparse.ArgumentParser(
      description=
      "Generate a linker script that strips symbols from a shared library that are not used from another shared library."
  )
  parser.add_argument("--objdump",
                      required=True,
                      help="Path to llvm-objdump tool")
  parser.add_argument(
      "--lib-undefined",
      required=True,
      help="Library containing undefined symbols (e.g., cronet)")
  parser.add_argument("--lib-defined",
                      required=True,
                      help="Library providing the symbols (e.g., libcrypto)")
  parser.add_argument("--out",
                      required=True,
                      help="Path to the output map file")
  return parser.parse_args()


def get_symbols(objdump_path, lib_path):
  return cronet_utils.run_and_get_stdout([objdump_path, "-T", lib_path])


def parse_and_filter_objdump_symbols(objdump_text, filter_fn):
  symbols = set()
  seen_dynamic_table_header = False
  for line in objdump_text.splitlines():
    line = line.strip()
    if not line:
      continue

    if "DYNAMIC SYMBOL TABLE:" in line:
      seen_dynamic_table_header = True
      continue

    if not seen_dynamic_table_header:
      continue

    if filter_fn(line):
      symbols.add(line.split()[-1])

  return symbols


def main():
  args = parse_args()
  undefined_symbols = parse_and_filter_objdump_symbols(
      get_symbols(args.objdump, args.lib_undefined), filter_undefined_only)
  defined_symbols = parse_and_filter_objdump_symbols(
      get_symbols(args.objdump, args.lib_defined),
      filter_anything_but_undefined)
  intersection = sorted(undefined_symbols.intersection(defined_symbols))
  cronet_utils.write_file(
      args.out, """
INTERSECTED_SYMBOLS {{
    global:
{symbols};
    local: *;
}};
""".format(symbols=";\n".join(intersection)))


if __name__ == "__main__":
  main()
