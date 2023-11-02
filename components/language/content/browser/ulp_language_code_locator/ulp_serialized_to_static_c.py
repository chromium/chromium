# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Generate c++ structure containing serialized ULP language quad tree"""

import argparse
import os.path
import sys

sys.path.insert(1,
    os.path.join(os.path.dirname(__file__),
    os.path.pardir,
    os.path.pardir,
    os.path.pardir,
    os.path.pardir,
    os.path.pardir,
    "third_party"))
import jinja2 # pylint: disable=F0401


def ReadSerializedData(input_path):
  """Read serialized ULP language quad tree"""

  with open(input_path, 'rb') as input_file:
    data = input_file.read()

  linebreak = data.index(b'\n')
  # First line is comma-separated list of languages.
  language_codes = data[:linebreak].strip().decode(encoding="utf-8").split(',')
  # Rest of the file is the serialized tree.
  tree_bytes = data[linebreak+1:]

  # Strings are read in Python 2 so we need to use ord() to convert to bytes.
  to_bytes = ord if sys.version_info.major == 2 else lambda x: x

  # We group the bytes in the string into 32 bits integers.
  tree_serialized = [
    sum((to_bytes(tree_bytes[i+b]) << (8*b)) if i+b < len(tree_bytes) else 0
    for b in range(4))
    for i in range(0, len(tree_bytes), 4)
  ]
  return language_codes, tree_serialized


def GenerateCpp(output_path,
                template_path,
                models):
    """Render the template"""
    with open(template_path, "r") as f:
      template = jinja2.Template(f.read())
      generated_code = template.render(models=models)

    # Write the generated code.
    with open(output_path, "w") as f:
      f.write(generated_code)


def Main():
    parser = argparse.ArgumentParser()
    parser.add_argument(
      "--output", required=True,
      help="path to the generated c++ file")
    parser.add_argument(
      "--template", required=True,
      help="path to the template used to generate c++ file")
    parser.add_argument(
      "--data", required=True, nargs='+',
      help="path to the input serialized ULP data file")

    args = parser.parse_args()

    output_path = args.output
    template_path = args.template

    models = [ReadSerializedData(data_path) for data_path in args.data]

    GenerateCpp(output_path,
                template_path,
                models)

if __name__ == "__main__":
    Main()
