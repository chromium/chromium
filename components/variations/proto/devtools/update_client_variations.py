# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Updates the generated ClientVariations proto parser and formatter.

This script builds the ClientVariations proto TypeScript bindings,
transpiles the parser to JavaScript, bundles it with its dependencies
using rollup, and minifies the output using terser.
"""

import argparse
import json
import os
import pathlib
import shutil
import subprocess
import sys
import tempfile

HERE_DIR = os.path.dirname(__file__)
ROOT = os.path.normpath(os.path.join(HERE_DIR, '..', '..', '..', '..'))

sys.path.append(os.path.join(ROOT, 'third_party', 'node'))
import node
import node_modules

OUTPUT_TEMPLATE = """\
// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Note: This is a generated file. Do not edit by hand. Instead, run
// components/variations/proto/devtools/update_client_variations.py to update.

// clang-format off
%s
// clang-format on
"""


def transpile_typescript(parser_ts: str, gen_proto_ts: str,
                         out_dir: str) -> str:
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

  node.RunNode([node_modules.PathToTypescript(), '-p', tsconfig_path])

  # The output JS for the parser will be in out_dir mirroring the source tree.
  rel_parser_js = os.path.relpath(parser_ts, ROOT).replace('.ts', '.js')
  return os.path.join(out_dir, rel_parser_js)


def bundle_with_rollup(parser_js: str, gen_proto_ts: str, bundle_js: str,
                       tmp_dir: str) -> None:
  """Bundles the transpiled parser and its dependencies using Rollup."""
  # On Windows, absolute paths must be valid file:// URLs for ESM loading.
  plugin_path = os.path.join(HERE_DIR, 'rollup_plugin.mjs')
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

  node.RunNode([node_modules.PathToRollup(), '--config', rollup_config_path])


def minify_with_terser(bundle_js: str, minified_js: str) -> None:
  """Minifies the Rollup bundle using Terser."""
  node.RunNode([
      node_modules.PathToTerser(),
      bundle_js,
      '--module',
      '--compress',
      '--mangle',
      '--format',
      'max_line_len=500',
      '-o',
      minified_js,
  ])


def generate_bundle(parser_ts: str, gen_proto_ts: str, output_file: str) -> None:
  """Transpiles, bundles, and minifies the parser into output_file."""
  with tempfile.TemporaryDirectory() as tmp_dir:
    parser_js = transpile_typescript(parser_ts, gen_proto_ts, tmp_dir)

    bundle_js = os.path.join(tmp_dir, 'bundle.js')
    bundle_with_rollup(parser_js, gen_proto_ts, bundle_js, tmp_dir)

    minified_js = os.path.join(tmp_dir, 'bundle.min.js')
    minify_with_terser(bundle_js, minified_js)

    with open(minified_js, 'r') as f:
      minified_content = f.read().strip()

    os.makedirs(os.path.dirname(output_file), exist_ok=True)
    with open(output_file, 'w') as f:
      f.write(OUTPUT_TEMPLATE % minified_content)


def main() -> None:
  parser = argparse.ArgumentParser()
  parser.add_argument(
      '-t', '--target', default='Default',
      help='the target build subdirectory under src/out/ when run manually')
  parser.add_argument('--parser-ts', help='path to the parser TS file')
  parser.add_argument('--proto-ts', help='path to the generated proto TS file')
  parser.add_argument('--out-js', help='path to the output JS file')
  args = parser.parse_args()

  # When invoked by GN action("client_variations_js"), all three paths are given.
  if args.parser_ts and args.proto_ts and args.out_js:
    generate_bundle(os.path.abspath(args.parser_ts),
                    os.path.abspath(args.proto_ts),
                    os.path.abspath(args.out_js))
    return

  # Otherwise, run autoninja to build the generated JS and copy it to the
  # checked-in client_variations.js file synced into DevTools.
  build_dir = os.path.abspath(os.path.join(ROOT, 'out', args.target))
  subprocess.check_call([
      'autoninja', '-C', build_dir,
      'components/variations/proto/devtools:client_variations_js'
  ])

  gen_file = os.path.join(
      build_dir, 'gen', 'components', 'variations', 'proto', 'devtools',
      'client_variations.js')
  checked_in_file = os.path.abspath(os.path.join(HERE_DIR, 'client_variations.js'))
  shutil.copyfile(gen_file, checked_in_file)


if __name__ == '__main__':
  main()
