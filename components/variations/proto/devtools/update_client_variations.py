# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Updates the generated ClientVariations proto parser and formatter.

This script builds the ClientVariations proto TypeScript bindings,
transpiles the parser to JavaScript, and bundles it with its dependencies
using rollup.
"""

import argparse
import json
import os
import pathlib
import subprocess
import sys
import tempfile
from typing import Optional

HERE_DIR = os.path.dirname(__file__)
ROOT = os.path.normpath(os.path.join(HERE_DIR, '..', '..', '..', '..'))

sys.path.append(os.path.join(ROOT, 'third_party', 'node'))
import node
import node_modules

OUTPUT_TEMPLATE = """\
/* eslint-disable */
// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Note: This is a generated file. Do not edit by hand. Instead, run
// components/variations/proto/devtools/update_client_variations.py to update.

// clang-format off
%s
// clang-format on
"""


def transpile_typescript(node_path: str, tsc_path: str, parser_ts: str,
                         gen_proto_ts: str, out_dir: str) -> str:
  """Transpiles the parser and its proto dependency to JavaScript."""
  # We need to map @bufbuild/protobuf and the local proto bindings.
  # tsc will compile both the parser and its dependency (the generated proto)
  # because they are linked via the 'paths' mapping.
  tsconfig = {
      "compilerOptions": {
          "module": "ESNext",
          "target": "ESNext",
          "moduleResolution": "bundler",
          "allowJs": True,
          "rootDir": ROOT,
          "outDir": out_dir,
          "skipLibCheck": True,
          "paths": {
              # Map the import in the parser to the actual TS file location
              # so tsc can find and compile it.
              "client_variations_proto": [gen_proto_ts],
              "@bufbuild/protobuf/*": [
                  os.path.join(
                      ROOT,
                      "third_party/node/node_modules/@bufbuild/protobuf/dist/esm/*"
                  )
              ],
              "/@bufbuild/protobuf/*": [
                  os.path.join(
                      ROOT,
                      "third_party/node/node_modules/@bufbuild/protobuf/dist/esm/*"
                  )
              ]
          }
      },
      "files": [parser_ts]
  }

  tsconfig_path = os.path.join(out_dir, 'tsconfig.json')
  with open(tsconfig_path, 'w') as f:
    json.dump(tsconfig, f)

  cmd = [node_path, tsc_path, '-p', tsconfig_path]
  subprocess.check_call(cmd)

  # The output JS for the parser will be in out_dir mirroring the source tree.
  rel_parser_js = os.path.relpath(parser_ts, ROOT).replace('.ts', '.js')
  return os.path.join(out_dir, rel_parser_js)


def bundle_with_rollup(node_path: str, rollup_path: str, plugin_path: str,
                       parser_js: str, gen_proto_ts: str, bundle_js: str,
                       tmp_dir: str) -> None:
  """Bundles the transpiled parser and its dependencies using Rollup."""
  # On Windows, absolute paths must be valid file:// URLs for ESM loading.
  plugin_url = pathlib.Path(plugin_path).as_uri()

  # Custom resolver for rollup to avoid needing node-resolve plugin.
  parser_js_posix = parser_js.replace('\\', '/')
  bundle_js_posix = bundle_js.replace('\\', '/')
  node_modules_posix = os.path.join(
      ROOT, "third_party/node/node_modules/@bufbuild/protobuf/dist/esm"
  ).replace('\\', '/')

  rel_proto_js = os.path.relpath(gen_proto_ts, ROOT).replace('.ts', '.js')
  proto_js_posix = os.path.join(tmp_dir, rel_proto_js).replace('\\', '/')

  rollup_config = f"""
import {{ variationsResolver }} from '{plugin_url}';

export default {{
  input: '{parser_js_posix}',
  output: {{
    file: '{bundle_js_posix}',
    format: 'es',
  }},
  plugins: [
    variationsResolver('{node_modules_posix}', '{proto_js_posix}')
  ]
}};
"""
  rollup_config_path = os.path.join(tmp_dir, 'rollup.config.mjs')
  with open(rollup_config_path, 'w') as f:
    f.write(rollup_config)

  cmd = [node_path, rollup_path, '--config', rollup_config_path]
  subprocess.check_call(cmd)


def main() -> None:
  parser = argparse.ArgumentParser()
  parser.add_argument('--parser-ts', required=True, help='path to the parser TS file')
  parser.add_argument('--proto-ts', required=True, help='path to the generated proto TS file')
  parser.add_argument('--out-js', required=True, help='path to the output JS file')
  args = parser.parse_args()

  parser_ts = os.path.abspath(args.parser_ts)
  gen_proto_ts = os.path.abspath(args.proto_ts)
  output_file = os.path.abspath(args.out_js)
  plugin_path = os.path.join(HERE_DIR, 'rollup_plugin.mjs')

  # Paths to tools
  node_path = node.GetBinaryPath()
  tsc_path = node_modules.PathToTypescript()
  rollup_path = node_modules.PathToRollup()

  with tempfile.TemporaryDirectory() as tmp_dir:
    parser_js = transpile_typescript(node_path, tsc_path, parser_ts,
                                     gen_proto_ts, tmp_dir)

    bundle_js = os.path.join(tmp_dir, 'bundle.js')
    bundle_with_rollup(node_path, rollup_path, plugin_path, parser_js,
                       gen_proto_ts, bundle_js, tmp_dir)

    with open(bundle_js, 'r') as f:
      bundled_content = f.read().strip()

    os.makedirs(os.path.dirname(output_file), exist_ok=True)
    with open(output_file, 'w') as f:
      f.write(OUTPUT_TEMPLATE % bundled_content)


if __name__ == '__main__':
  main()
