#!/usr/bin/env vpython3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Builds the Ubuntu runtime bundled into the snap package from the lock file.

The snap package (//chrome/installer/linux/snap) is based on core26 and ships
a few libraries from the Ubuntu 26.04 archive that are not part of the base
snap (NSS, libsecret, opensc-pkcs11, Qt 6, ...). This script produces those
files without snapcraft. Its only input is runtime.lock.json, which update.py
generates with apt.

Subcommands (run by the 3pp builder):
  latest              Prints the package version (source stage).
  checkout <dir>      Downloads the .debs from the URLs in runtime.lock.json
                      into <dir> and checks their sizes and SHA256s
                      (source stage).
  install <prefix>    Extracts the .debs, applies the prime filters and the
                      gnome extension's cleanup recorded in runtime.lock.json,
                      and writes the result to <prefix> (build stage, run from
                      the checkout directory).

Output layout (the CIPD package):
  prime/              Files to copy into the root of the snap ($SNAP).
  copyright/<pkg>     debian/copyright of every bundled package.
  manifest.json       Snapshot timestamp and the bundled package versions.
"""

import argparse
import fnmatch
import gzip
import hashlib
import io
import json
import lzma
import os
import pathlib
import shutil
import sys
import tarfile
import tempfile
import time
import urllib.request

SCRIPT_DIR = pathlib.Path(__file__).resolve().parent
LOCK_FILE = SCRIPT_DIR / 'runtime.lock.json'

# Directory with the downloaded .debs inside the 3pp checkout.
DEBS_DIR = 'debs'


def log(msg):
    print(msg, file=sys.stderr, flush=True)


def load_json(path):
    with open(path, encoding='utf-8') as f:
        return json.load(f)


def write_json(path, data):
    with open(path, 'w', encoding='utf-8') as f:
        json.dump(data, f, indent=2, sort_keys=False)
        f.write('\n')


def sha256_file(path):
    h = hashlib.sha256()
    with open(path, 'rb') as f:
        for chunk in iter(lambda: f.read(1 << 20), b''):
            h.update(chunk)
    return h.hexdigest()


def download(url, dest, retries=5):
    dest = pathlib.Path(dest)
    dest.parent.mkdir(parents=True, exist_ok=True)
    for attempt in range(retries):
        try:
            log(f'Downloading {url}')
            tmp = dest.with_name(dest.name + '.partial')
            with (
                urllib.request.urlopen(url, timeout=120) as r,
                open(tmp, 'wb') as f,
            ):
                shutil.copyfileobj(r, f, 1 << 20)
            tmp.rename(dest)
            return
        except OSError as e:
            if attempt == retries - 1:
                raise
            log(f'  failed ({e}), retrying')
            time.sleep(2**attempt)


# .deb extraction.


def iter_ar_members(path):
    """Yields (name, bytes) for every member of an ar archive."""
    with open(path, 'rb') as f:
        if f.read(8) != b'!<arch>\n':
            raise RuntimeError(f'{path}: not an ar archive')
        while True:
            header = f.read(60)
            if not header:
                return
            if len(header) != 60 or header[58:60] != b'`\n':
                raise RuntimeError(f'{path}: bad ar header')
            name = header[0:16].decode().strip().rstrip('/')
            size = int(header[48:58].decode().strip())
            data = f.read(size)
            if size % 2:
                f.read(1)
            yield name, data


def open_data_tar(deb_path):
    for name, data in iter_ar_members(deb_path):
        if not name.startswith('data.tar'):
            continue
        if name.endswith('.zst'):
            import zstandard  # pylint: disable=import-outside-toplevel

            data = zstandard.ZstdDecompressor().decompressobj().decompress(data)
        elif name.endswith('.xz'):
            data = lzma.decompress(data)
        elif name.endswith('.gz'):
            data = gzip.decompress(data)
        elif name != 'data.tar':
            raise RuntimeError(f'{deb_path}: unsupported member {name}')
        return tarfile.open(fileobj=io.BytesIO(data), mode='r:')
    raise RuntimeError(f'{deb_path}: no data.tar member')


def extract_deb(deb_path, dest):
    with open_data_tar(deb_path) as tar:
        # The 'tar' filter rejects paths outside of `dest` and clears the
        # setuid/setgid bits, like craft-parts does for stage-packages.
        tar.extractall(dest, filter='tar')


def is_excluded(rel_path, excludes):
    parts = rel_path.split('/')
    for i in range(1, len(parts) + 1):
        prefix = '/'.join(parts[:i])
        if any(fnmatch.fnmatchcase(prefix, p) for p in excludes):
            return True
    return False


def normalize_and_filter(tree, prime):
    """Applies snapcraft-style normalization and prime filters to `tree`.

    `prime` is a list of '-path' exclusions, as in snapcraft.yaml.
    """
    excludes = []
    for entry in prime:
        if not entry.startswith('-'):
            raise RuntimeError(
                f'Only exclusions are supported in prime: {entry}'
            )
        excludes.append(entry[1:].strip('/'))

    for root, dirs, files in os.walk(tree, topdown=True):
        for name in list(dirs) + files:
            path = pathlib.Path(root, name)
            rel = path.relative_to(tree).as_posix()
            if is_excluded(rel, excludes):
                if path.is_dir() and not path.is_symlink():
                    shutil.rmtree(path)
                    dirs.remove(name)
                else:
                    path.unlink()
                continue
            if path.is_symlink():
                target = os.readlink(path)
                # Make absolute symlinks relative if the target is in the tree.
                if target.startswith('/') and os.path.lexists(
                    tree / target[1:]
                ):
                    path.unlink()
                    path.symlink_to(
                        os.path.relpath(tree / target[1:], start=root)
                    )
                    if name in dirs:
                        dirs.remove(name)


def merge_tree(src, dest):
    for root, dirs, files in os.walk(src):
        rel_root = pathlib.Path(root).relative_to(src)
        for name in dirs + files:
            s = pathlib.Path(root, name)
            d = dest / rel_root / name
            if s.is_dir() and not s.is_symlink():
                d.mkdir(parents=True, exist_ok=True)
                continue
            if os.path.lexists(d):
                same = (
                    (os.readlink(s) == os.readlink(d))
                    if s.is_symlink()
                    else (
                        not d.is_symlink() and sha256_file(s) == sha256_file(d)
                    )
                )
                if not same:
                    raise RuntimeError(f'Conflicting file {rel_root / name}')
                continue
            d.parent.mkdir(parents=True, exist_ok=True)
            if s.is_symlink():
                d.symlink_to(os.readlink(s))
            else:
                shutil.copy2(s, d)
        # Symlinks to directories are handled above; don't descend into them.
        dirs[:] = [n for n in dirs if not pathlib.Path(root, n).is_symlink()]


def apply_cleanup(prime_dir, remove):
    for rel in remove:
        path = prime_dir / rel
        if path.is_dir() and not path.is_symlink():
            shutil.rmtree(path)
        elif os.path.lexists(path):
            path.unlink()
    # Drop symlinks left dangling by the removal (relative ones only; absolute
    # ones point into the base or content snaps at runtime), then empty dirs.
    changed = True
    while changed:
        changed = False
        for root, dirs, files in os.walk(prime_dir):
            for name in dirs + files:
                path = pathlib.Path(root, name)
                if (
                    path.is_symlink()
                    and not os.readlink(path).startswith('/')
                    and not path.exists()
                ):
                    path.unlink()
                    changed = True
    for root, dirs, files in os.walk(prime_dir, topdown=False):
        if root != str(prime_dir) and not os.listdir(root):
            os.rmdir(root)


# Subcommands.


def deb_path(root, pkg):
    return root / DEBS_DIR / pkg['url'].rsplit('/', 1)[-1]


def check_deb(path, pkg):
    if path.stat().st_size != pkg['size'] or sha256_file(path) != pkg['sha256']:
        raise RuntimeError(
            f'{path}: size or SHA256 does not match {LOCK_FILE.name}'
        )


def cmd_latest(_args):
    print(load_json(LOCK_FILE)['version'])


def cmd_checkout(args):
    lock = load_json(LOCK_FILE)
    requested = os.environ.get('_3PP_VERSION')
    if requested and requested != lock['version']:
        raise RuntimeError(
            f'Requested version {requested} does not match '
            f'{LOCK_FILE.name} version {lock["version"]}'
        )
    root = pathlib.Path(args.checkout_dir)
    root.mkdir(parents=True, exist_ok=True)
    for pkg in lock['packages']:
        dest = deb_path(root, pkg)
        download(pkg['url'], dest)
        check_deb(dest, pkg)
    shutil.copy2(LOCK_FILE, root / LOCK_FILE.name)


def cmd_install(args):
    root = pathlib.Path(args.checkout_dir or os.getcwd())
    prefix = pathlib.Path(args.prefix)
    lock = load_json(LOCK_FILE)
    for pkg in lock['packages']:
        check_deb(deb_path(root, pkg), pkg)

    prime_dir = prefix / 'prime'
    copyright_dir = prefix / 'copyright'
    prime_dir.mkdir(parents=True, exist_ok=True)
    copyright_dir.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(dir=root) as tmp:
        for group, group_lock in lock['groups'].items():
            group_dir = pathlib.Path(tmp, group)
            group_dir.mkdir()
            for pkg in lock['packages']:
                if group not in pkg['groups']:
                    continue
                extract_deb(deb_path(root, pkg), group_dir)
                copyright_file = (
                    group_dir / 'usr/share/doc' / pkg['name'] / 'copyright'
                )
                if copyright_file.is_file() and not copyright_file.is_symlink():
                    shutil.copy2(copyright_file, copyright_dir / pkg['name'])
            normalize_and_filter(group_dir, group_lock['prime'])
            merge_tree(group_dir, prime_dir)
    apply_cleanup(prime_dir, lock['cleanup']['remove'])

    missing = [
        p['name']
        for p in lock['packages']
        if not (copyright_dir / p['name']).exists()
    ]
    if missing:
        log(f'Note: no copyright file shipped in: {", ".join(missing)}')
    write_json(
        prefix / 'manifest.json',
        {
            'version': lock['version'],
            'snapshot': lock['snapshot'],
            'base': lock['base'],
            'packages': {p['name']: p['version'] for p in lock['packages']},
        },
    )


def main():
    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawTextHelpFormatter
    )
    sub = parser.add_subparsers(dest='command', required=True)
    sub.add_parser('latest').set_defaults(func=cmd_latest)
    checkout = sub.add_parser('checkout')
    checkout.add_argument('checkout_dir')
    checkout.set_defaults(func=cmd_checkout)
    install = sub.add_parser('install')
    install.add_argument('prefix')
    install.add_argument('deps_prefix', nargs='?')
    install.add_argument(
        '--checkout-dir', help='Defaults to the current directory.'
    )
    install.set_defaults(func=cmd_install)
    args = parser.parse_args()
    args.func(args)
    return 0


if __name__ == '__main__':
    sys.exit(main())
