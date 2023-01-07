# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Updates the generated ClientVariations proto parser and formatter.

If this script happens not to run correctly in your environment, it should be
easy to perform the steps manually. This script simply builds a generated file,
and then copies it into the Chromium checkout, making some simple modifications.
"""

import argparse
import os

OUTPUT_TEMPLATE = """\
/* eslint-disable */
// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Note: This is a generated file. Do not edit by hand. Instead, run
// components/variations/proto/devtools/update_client_variations.py to update.

const gen = {};

// clang-format off
%s
// clang-format on

export function parseClientVariations(data) {
  return gen.parseClientVariations(data);
}
export function formatClientVariations(data) {
  return gen.formatClientVariations(data);
}
"""


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument(
      '-t', '--target', default='Default',
      help='the target build subdirectory under src/out/')
  args = parser.parse_args()

  cwd = os.path.dirname(__file__)
  root = os.path.join(cwd, '..', '..', '..', '..')
  build_dir = os.path.abspath(os.path.join(root, 'out', args.target))
  cmd = 'autoninja -C %s components/variations/proto/devtools' % build_dir
  os.system(cmd)

  script_file = os.path.join(
      build_dir, 'gen', 'components', 'variations', 'proto', 'devtools',
      'client_variations_gen.js')
  with open(script_file, 'r') as f:
    script = f.read().strip()
  script = script.replace('call(this)', 'call(gen)')

  output_file = os.path.abspath(
      os.path.join(cwd, 'client_variations.js'))
  with open(output_file, 'w') as f:
    f.write(OUTPUT_TEMPLATE % script)


if __name__ == '__main__':
  main()
