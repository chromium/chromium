#!/usr/bin/env python3
# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import ast
import argparse
from distutils import dir_util
import functools
import json
import logging
import os
import re
import shlex
import shutil
import subprocess
import sys
import tempfile
import time


@functools.lru_cache(1)
def get_chromium_root():
    path = os.path.realpath('../../../../')
    assert os.path.basename(path) == 'src'
    return path


def shell_join(cmd):
    return ' '.join(shlex.quote(c) for c in cmd)


def run(args, cwd=None):
    logging.debug(f'$ {shell_join(args)}')
    subprocess.check_call(args, cwd=cwd)


def build_locale_strings():
    grit_cmd = [
        os.path.join(get_chromium_root(), 'tools/grit/grit.py'),
        '-i',
        'strings/camera_strings.grd',
        'build',
        '-E',
        'out_camera_app_dir=platform',
        '-E',
        'gen_camera_app_dir=swa',
        '-o',
        'build/strings',
    ]
    run(grit_cmd)


def build_preload_images_js(outdir):
    with open('images/images.gni') as f:
        in_app_assets = ast.literal_eval(
            re.search(r'in_app_assets\s*=\s*(\[.*\])', f.read(),
                      re.DOTALL).group(1))
    with tempfile.NamedTemporaryFile('w') as f:
        f.writelines(asset + '\n' for asset in in_app_assets)
        f.flush()
        cmd = [
            'utils/gen_preload_images_js.py',
            '--images_list_file',
            f.name,
            '--output_file',
            os.path.join(outdir, 'preload_images.js'),
        ]
        subprocess.check_call(cmd)


def build_mojom_bindings(mojom_bindings):
    pylib = os.path.join(get_chromium_root(), 'mojo/public/tools/mojom')
    sys.path.insert(0, pylib)
    # pylint: disable=import-error,import-outside-toplevel
    from mojom.parse.parser import Parse

    mojom_paths = set()

    def add_path(mojom):
        path = os.path.join(get_chromium_root(), mojom)
        if path in mojom_paths:
            return
        mojom_paths.add(path)

        with open(path) as f:
            code = f.read()
            ast = Parse(code, path)
            for imp in ast.import_list:
                add_path(imp.import_filename)

    for binding in mojom_bindings:
        mojom = re.sub('-lite.js$', '', binding)
        add_path(mojom)

    # It's needed for generating mojo_bindings_lite.js.
    add_path(
        'mojo/public/interfaces/bindings/interface_control_messages.mojom')

    if not os.path.exists('build/mojo'):
        os.makedirs('build/mojo')

    generator = os.path.join(
        get_chromium_root(),
        'mojo/public/tools/bindings/mojom_bindings_generator.py')

    precompile_cmd = [
        generator,
        '-o',
        'build/mojo',
        '--use_bundled_pylibs',
        'precompile',
    ]
    run(precompile_cmd)

    parser = os.path.join(get_chromium_root(),
                          'mojo/public/tools/mojom/mojom_parser.py')

    parse_cmd = [
        parser,
        '--output-root',
        'build/mojo',
        '--input-root',
        get_chromium_root(),
        '--mojoms',
    ] + list(mojom_paths)
    run(parse_cmd)

    generate_cmd = [
        generator,
        '-o',
        'build/mojo',
        '--use_bundled_pylibs',
        'generate',
        '-d',
        get_chromium_root(),
        '-I',
        get_chromium_root(),
        '--bytecode_path',
        'build/mojo',
        '-g',
        'javascript',
        '--js_bindings_mode',
        'new',
    ] + list(mojom_paths)
    run(generate_cmd)

    generate_binding_lite_cmd = [
        os.path.join(get_chromium_root(),
                     ('mojo/public/tools/bindings/' +
                      'concatenate_and_replace_closure_exports.py')),
        os.path.join(get_chromium_root(),
                     'mojo/public/js/mojo_internal_preamble.js.part'),
        os.path.join(get_chromium_root(), 'mojo/public/js/bindings_lite.js'),
        os.path.join(get_chromium_root(),
                     'mojo/public/js/interface_support_preamble.js.part'),
        os.path.join(get_chromium_root(),
                     'mojo/public/js/interface_support.js'),
        ('build/mojo/mojo/public/interfaces/' +
         'bindings/interface_control_messages.mojom-lite.js'),
        'build/mojo/mojo_bindings_lite.js',
    ]
    run(generate_binding_lite_cmd)


def build_cca(overlay=None, key=None):
    with open('BUILD.gn') as f:
        mojom_bindings = re.findall(r'root_gen_dir/(.*mojom-lite\.js)',
                                    f.read())
    mojo_files = ['mojo_bindings_lite.js'] + mojom_bindings

    # TODO(shik): Check mtime and rebuild them if the source is updated.
    if not os.path.exists('build/strings/platform'):
        build_locale_strings()
    if any(not os.path.exists(os.path.join('build/mojo', f))
           for f in mojo_files):
        build_mojom_bindings(mojom_bindings)

    shutil.rmtree('build/camera', ignore_errors=True)

    dir_list = [src for src in os.listdir('.') if os.path.isdir(src)]
    for d in dir_list:
        if d in ['build', 'node_modules', 'strings']:
            continue
        dir_util.copy_tree(d, os.path.join('build/camera', d))
    build_preload_images_js('build/camera/js')
    shutil.copy2('manifest.json', 'build/camera/manifest.json')

    for f in mojo_files:
        shutil.copy2(os.path.join('build/mojo', f), 'build/camera/js/mojo')
    dir_util.copy_tree('build/strings/platform', 'build/camera')

    if overlay == 'dev':
        dir_util.copy_tree('utils/dev', 'build/camera')

        git_cmd = ['git', 'rev-parse', 'HEAD']
        commit_hash = subprocess.check_output(git_cmd, text=True).strip()[:8]
        timestamp = time.strftime("%F %T")

        with open('manifest.json') as f:
            manifest = json.load(f)
        manifest['version_name'] = f'Dev {commit_hash} @ {timestamp}'
        if key is not None:
            manifest['key'] = key
        with open('build/camera/manifest.json', 'w') as f:
            json.dump(manifest, f, indent=2)


def deploy(args):
    build_cca()
    cmd = [
        'rsync',
        '--archive',
        '--checksum',
        '--chown=chronos:chronos',
        '--omit-dir-times',
        '--perms',
        '--verbose',
        'build/camera/',
        f'{args.device}:/opt/google/chrome/resources/chromeos/camera',
    ]
    run(cmd)


def deploy_swa(args):
    cca_root = os.getcwd()
    target_dir = os.path.join(get_chromium_root(), f'out_{args.board}/Release')

    build_preload_images_js(
        os.path.join(target_dir,
                     'gen/chromeos/components/camera_app_ui/resources/js'))

    build_pak_cmd = [
        'tools/grit/grit.py',
        '-i',
        os.path.join(cca_root, 'camera_app_resources.grd'),
        'build',
        '-o',
        os.path.join(target_dir, 'gen/chromeos'),
        '-f',
        os.path.join(target_dir,
                     'gen/tools/gritsettings/default_resource_ids'),
        '-E',
        f'root_gen_dir={os.path.join(target_dir, "gen")}',
    ]
    # Since there is a constraint in grit.py which will replace ${root_gen_dir}
    # in .grd file only if the script is executed in the parent directory of
    # ${root_gen_dir}, execute the script in Chromium root as a workaround.
    run(build_pak_cmd, get_chromium_root())

    with tempfile.TemporaryDirectory() as tmp_dir:
        pak_util_script = os.path.join(get_chromium_root(),
                                       'tools/grit/pak_util.py')
        extract_resources_pak_cmd = [
            pak_util_script,
            'extract',
            os.path.join(target_dir, 'resources.pak'),
            '-o',
            tmp_dir,
        ]
        run(extract_resources_pak_cmd)

        extract_camera_pak_cmd = [
            pak_util_script,
            'extract',
            os.path.join(target_dir,
                         'gen/chromeos/chromeos_camera_app_resources.pak'),
            '-o',
            tmp_dir,
        ]
        run(extract_camera_pak_cmd)

        create_new_resources_pak_cmd = [
            pak_util_script,
            'create',
            '-i',
            tmp_dir,
            os.path.join(target_dir, 'resources.pak'),
        ]
        run(create_new_resources_pak_cmd)

    deploy_new_resources_pak_cmd = [
        'rsync',
        '--inplace',
        os.path.join(target_dir, 'resources.pak'),
        f'{args.device}:/opt/google/chrome/',
    ]
    run(deploy_new_resources_pak_cmd)


def test(args):
    assert 'CCAUI' not in args.device, (
        'The first argument should be <device> instead of a test name pattern.'
    )
    tast_cmd = ['local_test_runner'] + args.pattern
    cmd = [
        'ssh',
        args.device,
        shell_join(tast_cmd),
    ]
    run(cmd)


def lint(args):
    root = get_chromium_root()
    node = os.path.join(root, 'third_party/node/linux/node-linux-x64/bin/node')
    eslint = os.path.join(
        root, 'third_party/node/node_modules/eslint/bin/eslint.js')
    subprocess.call([node, eslint, 'js'])


def parse_args(args):
    parser = argparse.ArgumentParser(description='CCA developer tools.')
    parser.add_argument('--debug', action='store_true')
    subparsers = parser.add_subparsers()

    deploy_parser = subparsers.add_parser('deploy',
                                          help='deploy to device',
                                          description='Deploy CCA to device.')
    deploy_parser.add_argument('device')
    deploy_parser.set_defaults(func=deploy)

    deploy_swa_parser = subparsers.add_parser(
        'deploy-swa',
        help='deploy CCA (SWA) to device',
        description='''Deploy CCA (SWA) to device.
            This script only works if there is no file added/deleted.
            And please build Chrome at least once before running the command.'''
    )
    deploy_swa_parser.add_argument('board')
    deploy_swa_parser.add_argument('device')
    deploy_swa_parser.set_defaults(func=deploy_swa)

    test_parser = subparsers.add_parser('test',
                                        help='run tests',
                                        description='Run CCA tests on device.')
    test_parser.add_argument('device')
    test_parser.add_argument('pattern',
                             nargs='*',
                             default=['camera.CCAUI*'],
                             help='test patterns. (default: camera.CCAUI*)')
    test_parser.set_defaults(func=test)

    lint_parser = subparsers.add_parser('lint',
                                        help='check code',
                                        description='Check coding styles.')
    lint_parser.set_defaults(func=lint)
    parser.set_defaults(func=lambda _args: parser.print_help())

    return parser.parse_args(args)


def main(args):
    cca_root = os.path.dirname(os.path.dirname(os.path.realpath(__file__)))
    assert os.path.basename(cca_root) == 'resources'
    os.chdir(cca_root)

    args = parse_args(args)

    log_level = logging.DEBUG if args.debug else logging.INFO
    log_format = '%(asctime)s - %(levelname)s - %(funcName)s: %(message)s'
    logging.basicConfig(level=log_level, format=log_format)

    logging.debug(f'args = {args}')
    return args.func(args)


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
