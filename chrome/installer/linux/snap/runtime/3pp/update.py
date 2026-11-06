#!/usr/bin/env vpython3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Regenerates runtime.lock.json for the snap runtime (developer tool).

Resolves the packages in config.json with apt-get at a snapshot.ubuntu.com
timestamp, the way snapcraft resolves stage-packages on a core26 build host:
- apt runs in a private root (APT_CONFIG and Dir point into a temporary
  directory), so the host's apt configuration and dpkg database are not used
  or modified. No root access is needed.
- The Ubuntu archive signature is checked against ubuntu-archive-keyring.gpg
  (Signed-By), and apt checks every downloaded index and .deb against it.
- The packages of the core26 base snap are marked as installed in the private
  dpkg status file, so their dependencies are not bundled, and like
  craft-parts, packages that the base snap lists are dropped from the result.
- Recommends and Suggests are not installed.

apt downloads the selected .debs, and this script records their full URL, size
and SHA256 in runtime.lock.json, together with the files that the gnome
extension's cleanup parts would remove. build.py only reads the lock file.

Needs apt-get, apt-cache and unsquashfs on the host.
"""

import argparse
import datetime
import fnmatch
import json
import os
import pathlib
import re
import subprocess
import sys
import tempfile
import urllib.request

import build

SCRIPT_DIR = pathlib.Path(__file__).resolve().parent
CONFIG_FILE = SCRIPT_DIR / 'config.json'
KEYRING_FILE = SCRIPT_DIR / 'ubuntu-archive-keyring.gpg'

SNAPSHOT_URL = 'https://snapshot.ubuntu.com/ubuntu/{snapshot}'
SNAP_STORE_INFO_URL = 'https://api.snapcraft.io/v2/snaps/info/{snap}'
ARCH = 'amd64'

# "Inst libnss3 (2:3.117-1ubuntu1 Ubuntu:26.04/resolute [amd64])", optionally
# with the installed version in brackets after the name.
INST_RE = re.compile(r'^Inst (\S+) (?:\[[^\]]*\] )?\((\S+) .*\[([^\]]+)\]\)')


def parse_control(text):
    """Parses deb822 stanzas into a list of dicts."""
    stanzas = []
    current = {}
    key = None
    for line in text.splitlines():
        if not line.strip():
            if current:
                stanzas.append(current)
            current, key = {}, None
        elif line[0] in ' \t':
            if key:
                current[key] += '\n' + line.strip()
        else:
            key, _, value = line.partition(':')
            current[key] = value.strip()
    if current:
        stanzas.append(current)
    return stanzas


def parse_base_manifest(path):
    """Returns {name: version} from a base snap's dpkg.yaml."""
    packages = {}
    for line in pathlib.Path(path).read_text(encoding='utf-8').splitlines():
        if line.startswith('- '):
            name, _, version = line[2:].partition('=')
            packages[name.split(':')[0]] = version.strip()
    return packages


class Apt:
    """Runs apt-get and apt-cache in a private root under `root`."""

    def __init__(self, root, base_url, suites, components):
        self.root = pathlib.Path(root)
        self.archives = self.root / 'var/cache/apt/archives'
        self.status = self.root / 'var/lib/dpkg/status'
        for d in (
            'etc/apt/apt.conf.d',
            'etc/apt/preferences.d',
            'etc/apt/sources.list.d',
            'etc/apt/trusted.gpg.d',
            'var/lib/apt/lists/partial',
            'var/lib/dpkg',
            'log',
        ):
            (self.root / d).mkdir(parents=True, exist_ok=True)
        (self.archives / 'partial').mkdir(parents=True, exist_ok=True)
        self.status.write_text('')
        (self.root / 'etc/apt/sources.list.d/ubuntu.sources').write_text(
            f'Types: deb\n'
            f'URIs: {base_url}\n'
            f'Suites: {" ".join(suites)}\n'
            f'Components: {" ".join(components)}\n'
            f'Architectures: {ARCH}\n'
            f'Signed-By: {KEYRING_FILE}\n'
        )
        self.config = self.root / 'apt.conf'
        self.config.write_text(f'''Dir "{self.root}/";
Dir::State "var/lib/apt/";
Dir::State::status "{self.status}";
Dir::Cache "var/cache/apt/";
Dir::Etc "etc/apt/";
Dir::Log "log/";
APT::Architecture "{ARCH}";
APT::Architectures {{ "{ARCH}"; }};
APT::Install-Recommends "false";
APT::Install-Suggests "false";
APT::Sandbox::User "";
Acquire::Languages "none";
Debug::NoLocking "true";
''')

    def run(self, tool, *args):
        env = dict(os.environ, APT_CONFIG=str(self.config), LC_ALL='C')
        return subprocess.run(
            [tool, '-q', *args],
            env=env,
            check=True,
            capture_output=True,
            text=True,
        ).stdout

    def update(self):
        self.run('apt-get', 'update')

    def show(self, specs):
        """Returns the index stanzas of `specs` (names or name=version)."""
        return parse_control(
            self.run('apt-cache', 'show', '--no-all-versions', *specs)
        )

    def mark_installed(self, packages):
        """Writes a dpkg status file in which `packages` are installed.

        `packages` maps names to versions. The other fields come from the
        archive's stanza; dependencies are left out, since the base snap is
        complete.
        """
        available = set(self.run('apt-cache', 'pkgnames').split())
        names = sorted(n for n in packages if n in available)
        stanzas = []
        for s in self.show(names):
            fields = [
                f'Package: {s["Package"]}',
                'Status: install ok installed',
                f'Architecture: {s["Architecture"]}',
                f'Version: {packages[s["Package"]]}',
            ]
            for key in ('Multi-Arch', 'Provides', 'Essential'):
                if key in s:
                    fields.append(f'{key}: {s[key]}')
            stanzas.append('\n'.join(fields) + '\n')
        self.status.write_text('\n'.join(stanzas))

    def resolve(self, roots):
        """Returns [(name, version, architecture)] that apt would install."""
        out = self.run('apt-get', 'install', '--simulate', *roots)
        return [m.groups() for m in map(INST_RE.match, out.splitlines()) if m]

    def download(self, roots):
        """Downloads the .debs for `roots` into the archives directory."""
        self.run('apt-get', 'install', '--download-only', '--yes', *roots)

    def archive_path(self, name, version, arch):
        """Returns the path of a .deb that download() stored."""
        return (
            self.archives / f'{name}_{version.replace(":", "%3a")}_{arch}.deb'
        )


def download_snap(snap, workdir):
    """Downloads latest/stable of `snap` from the store.

    Returns (path, revision).
    """
    req = urllib.request.Request(
        SNAP_STORE_INFO_URL.format(snap=snap) + '?fields=download,revision',
        headers={'Snap-Device-Series': '16'},
    )
    with urllib.request.urlopen(req, timeout=60) as r:
        info = json.load(r)
    entry = next(
        c
        for c in info['channel-map']
        if c['channel']['architecture'] == ARCH
        and c['channel']['track'] == 'latest'
        and c['channel']['risk'] == 'stable'
    )
    snap_file = pathlib.Path(workdir, f'{snap}_{entry["revision"]}.snap')
    if not snap_file.exists():
        build.download(entry['download']['url'], snap_file)
    return snap_file, entry['revision']


def list_snap_files(snap_file):
    """Returns the paths of all non-directories in a snap."""
    out = subprocess.run(
        ['unsquashfs', '-lc', '-d', '', str(snap_file)],
        check=True,
        capture_output=True,
        text=True,
    ).stdout
    return {line.lstrip('/') for line in out.splitlines() if line.strip()}


def fetch_base_manifest(snap, workdir):
    """Returns ({name: version}, revision) of the base snap from the store."""
    snap_file, revision = download_snap(snap, workdir)
    out = pathlib.Path(workdir, 'base')
    subprocess.run(
        [
            'unsquashfs',
            '-q',
            '-n',
            '-f',
            '-d',
            str(out),
            str(snap_file),
            'usr/share/snappy/dpkg.yaml',
        ],
        check=True,
    )
    return parse_base_manifest(out / 'usr/share/snappy/dpkg.yaml'), revision


def list_deb_files(deb_path):
    """Returns the paths of all non-directories in a .deb's data.tar."""
    with build.open_data_tar(deb_path) as tar:
        return {
            m.name[2:] if m.name.startswith('./') else m.name
            for m in tar.getmembers()
            if not m.isdir()
        }


def matches_glob(rel_path, pattern):
    """Matches `pattern` like a shell glob ('*' does not match '/').

    A match on a parent directory matches everything below it, as with rm -rf.
    """
    parts = rel_path.split('/')
    pattern_parts = pattern.split('/')
    if len(parts) < len(pattern_parts):
        return False
    return all(fnmatch.fnmatchcase(a, b) for a, b in zip(parts, pattern_parts))


def is_shared_library(rel_path):
    name = rel_path.rsplit('/', 1)[-1]
    return name.endswith('.so') or '.so.' in name


def compute_cleanup(config, debs, cache_dir):
    """Lists the files that the snapcraft gnome extension would remove.

    The extension's cleanup parts delete the libraries that the content snaps
    already provide: every *.so* in gnome-core26-sdk and gtk-common-themes,
    and the files in gpu-snap's list for the mesa content snap. `debs` are
    the paths of the bundled .debs.
    """
    cleanup = config['cleanup']
    so_files = set()
    revisions = {}
    for snap in cleanup['library_snaps']:
        snap_file, revisions[snap] = download_snap(snap, cache_dir)
        so_files |= {
            f for f in list_snap_files(snap_file) if is_shared_library(f)
        }

    gpu = cleanup['gpu_list']
    with urllib.request.urlopen(
        f'https://api.github.com/repos/{gpu["repo"]}/commits/{gpu["ref"]}',
        timeout=60,
    ) as r:
        gpu_commit = json.load(r)['sha']
    with urllib.request.urlopen(
        f'https://raw.githubusercontent.com/{gpu["repo"]}/{gpu_commit}/'
        f'{gpu["path"]}',
        timeout=60,
    ) as r:
        gpu_globs = [
            line.strip()
            for line in r.read().decode().splitlines()
            if line.strip() and not line.startswith('#')
        ]

    remove = set()
    for deb in debs:
        for f in list_deb_files(deb):
            if (is_shared_library(f) and f in so_files) or any(
                matches_glob(f, g) for g in gpu_globs
            ):
                remove.add(f)
    return {
        'library_snaps': revisions,
        'gpu_snap_commit': gpu_commit,
        'remove': sorted(remove),
    }


def next_version(snapshot, old_lock):
    date = snapshot[:8]
    counter = 1
    if old_lock and old_lock['version'].split('.')[0] == date:
        counter = int(old_lock['version'].split('.')[1]) + 1
    return f'{date}.{counter}'


def main():
    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawTextHelpFormatter
    )
    parser.add_argument(
        '--snapshot',
        help='snapshot.ubuntu.com timestamp, e.g. '
        '20261103T000000Z. Defaults to now.',
    )
    parser.add_argument(
        '--base-manifest',
        help='Path to the base snap\'s '
        'usr/share/snappy/dpkg.yaml. Downloaded from the snap '
        'store by default.',
    )
    parser.add_argument(
        '--cache-dir',
        help='Directory for the apt root and downloaded snaps '
        'and .debs. Defaults to a new temporary directory.',
    )
    args = parser.parse_args()

    config = build.load_json(CONFIG_FILE)
    old_lock = (
        build.load_json(build.LOCK_FILE) if build.LOCK_FILE.exists() else None
    )
    snapshot = args.snapshot or datetime.datetime.now(
        datetime.timezone.utc
    ).strftime('%Y%m%dT%H%M%SZ')
    base_url = SNAPSHOT_URL.format(snapshot=snapshot)
    cache_dir = pathlib.Path(args.cache_dir or tempfile.mkdtemp())
    cache_dir.mkdir(parents=True, exist_ok=True)

    if args.base_manifest:
        base_packages = parse_base_manifest(args.base_manifest)
        base_revision = None
    else:
        base_packages, base_revision = fetch_base_manifest(
            config['base_snap'], cache_dir
        )

    apt = Apt(
        cache_dir / f'apt-{snapshot}',
        base_url,
        config['suites'],
        config['components'],
    )
    build.log(f'Updating the apt indices at {base_url}')
    apt.update()
    apt.mark_installed(base_packages)

    groups_of = {}
    for group, group_config in config['groups'].items():
        roots = group_config['packages']
        for name, version, arch in apt.resolve(roots):
            # apt may also reinstall or upgrade base packages. Like
            # craft-parts, drop every package that the base snap lists.
            if name in base_packages and name not in roots:
                continue
            groups_of.setdefault((name, version, arch), []).append(group)
        apt.download(roots)

    selected = sorted(groups_of)
    stanzas = apt.show([f'{name}={version}' for name, version, _ in selected])
    if len(stanzas) != len(selected):
        raise RuntimeError(
            'apt-cache show returned an unexpected number of packages'
        )
    packages = []
    debs = []
    for (name, version, arch), stanza in zip(selected, stanzas):
        if (stanza['Package'], stanza['Version']) != (name, version):
            raise RuntimeError(f'Unexpected stanza for {name} {version}')
        path = apt.archive_path(name, version, arch)
        sha256 = build.sha256_file(path)
        if sha256 != stanza['SHA256']:
            raise RuntimeError(f'{path}: SHA256 mismatch')
        debs.append(path)
        packages.append(
            {
                'name': name,
                'version': version,
                'architecture': arch,
                'url': f'{base_url}/{stanza["Filename"]}',
                'sha256': sha256,
                'size': int(stanza['Size']),
                'groups': groups_of[(name, version, arch)],
            }
        )

    lock = {
        '_comment': 'Generated by update.py. Do not edit by hand.',
        'version': next_version(snapshot, old_lock),
        'snapshot': snapshot,
        'base': {
            'snap': config['base_snap'],
            'revision': base_revision,
        },
        'groups': {
            group: {
                'packages': group_config['packages'],
                'prime': group_config['prime'],
            }
            for group, group_config in config['groups'].items()
        },
        'cleanup': compute_cleanup(config, debs, cache_dir),
        'packages': packages,
    }
    build.write_json(build.LOCK_FILE, lock)
    total = sum(p['size'] for p in packages)
    build.log(
        f'Wrote {build.LOCK_FILE.name}: version {lock["version"]}, '
        f'{len(packages)} packages, {total / 1e6:.1f} MB of .debs'
    )
    return 0


if __name__ == '__main__':
    sys.exit(main())
