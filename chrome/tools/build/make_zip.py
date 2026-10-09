#!/usr/bin/env python3
# Copyright 2015 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Archives files from a build output directory.

This creates archives for testing, like the ones the CI archive builders
upload (e.g. the chrome-linux.zip snapshots), and also works for cross builds
(e.g. Linux builds on macOS). To create installable packages, use the
//chrome/installer/linux GN targets instead (e.g. :stable_deb, :stable_rpm),
which stage files with chrome/installer/linux/common/installer.py and need a
Linux host.

The files are listed either in an archive config from //infra/archive_config
(using the archive named like the output, e.g. chrome-linux.zip), or in a
legacy FILES.cfg. The output is a .zip, or a .tar.gz if its name ends with
.tar.gz or .tgz.
"""

import glob
import json
import os
import posixpath
import sys
import tarfile
import zipfile

# Builds without the V8 context snapshot (e.g. cross builds on a non-Linux host)
# only have the plain V8 snapshot.
_ALTERNATIVES = {'v8_context_snapshot.bin': 'snapshot_blob.bin'}


def _archive_stem(path):
  name = os.path.basename(path)
  for ext in ('.tar.gz', '.tgz', '.zip'):
    if name.endswith(ext):
      return name[: -len(ext)]
  return name


def _read_files_cfg(cfg_file):
  exec_globals = {'__builtins__': None}
  with open(cfg_file) as f:
    exec(f.read(), exec_globals)
  return [spec['filename'] for spec in exec_globals['FILES']], []


def _read_archive_config(config_file, output_file):
  with open(config_file) as f:
    archive_datas = json.load(f).get('archive_datas', [])
  stem = _archive_stem(output_file)
  names = []
  for data in archive_datas:
    if not data.get('gcs_path', '').endswith('.zip'):
      continue
    names.append(_archive_stem(data['gcs_path']))
    if names[-1] == stem:
      paths = (
        data.get('files', [])
        + data.get('file_globs', [])
        + data.get('dirs', [])
      )
      return paths, data.get('rename_dirs', [])
  sys.exit(
    f'error: no archive named {stem} in {config_file} '
    f'(available: {", ".join(names)})'
  )


def _expand(build_dir, path):
  """Returns the files at |path| (a file, dir or glob) relative to build_dir."""
  files = []
  for match in glob.glob(os.path.join(build_dir, path)):
    if os.path.isfile(match):
      files.append(os.path.relpath(match, build_dir))
    elif os.path.isdir(match):
      for root, _, filenames in os.walk(match):
        files.extend(
          os.path.relpath(os.path.join(root, f), build_dir) for f in filenames
        )
  return files


def _rename(path, rename_dirs):
  """Applies the first matching |rename_dirs| entry of an archive config."""
  for rename in rename_dirs:
    from_dir = rename['from_dir'].strip('/')
    to_dir = rename['to_dir'].strip('/')
    if from_dir in ('', '.'):
      return posixpath.join(to_dir, path)
    if path == from_dir or path.startswith(from_dir + '/'):
      return to_dir + path[len(from_dir) :]
  return path


def _root_owned(tarinfo):
  tarinfo.uid = tarinfo.gid = 0
  tarinfo.uname = tarinfo.gname = 'root'
  return tarinfo


def main(args):
  if len(args) != 3:
    print('usage: make_zip.py build_dir config output')
    print('  config: an //infra/archive_config .json file, or a FILES.cfg')
    print('  output: archive to write (.zip, .tar.gz or .tgz)')
    return 1
  (build_dir, config_file, output_file) = args

  is_archive_config = config_file.endswith('.json')
  if is_archive_config:
    paths, rename_dirs = _read_archive_config(config_file, output_file)
  else:
    paths, rename_dirs = _read_files_cfg(config_file)

  files = set()
  for path in paths:
    matches = _expand(build_dir, path)
    if not matches and path in _ALTERNATIVES:
      matches = _expand(build_dir, _ALTERNATIVES[path])
    if not matches and is_archive_config:
      print(f'warning: {path} not found in {build_dir}', file=sys.stderr)
    files.update(matches)
  files.discard(os.path.relpath(output_file, build_dir))
  if not files:
    print(f'error: no files found in {build_dir}')
    return 1

  arcnames = {
    f: _rename(f.replace(os.sep, '/'), rename_dirs) for f in sorted(files)
  }
  if output_file.endswith(('.tar.gz', '.tgz')):
    with tarfile.open(output_file, 'w:gz') as output:
      for f, arcname in arcnames.items():
        output.add(os.path.join(build_dir, f), arcname, filter=_root_owned)
  else:
    with zipfile.ZipFile(
      output_file, 'w', compression=zipfile.ZIP_DEFLATED, allowZip64=True
    ) as output:
      for f, arcname in arcnames.items():
        output.write(os.path.join(build_dir, f), arcname)

  print(f'wrote {output_file} ({len(files)} files)')
  return 0


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
