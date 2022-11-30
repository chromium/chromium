#!/usr/bin/env python
# coding: utf-8

# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import errno
import filecmp
import os
import plistlib
import shutil
import stat
import subprocess
import sys
import time


def _stat_or_none(path, root):
    """Calls os.stat or os.lstat to obtain information about a path.

    This program traverses parallel directory trees, which may have subtle
    differences such as directory entries that are present in fewer than all
    trees. It also operates on symbolic links directly, instead of on their
    targets.

    Args:
        path: The path to call os.stat or os.lstat on.
        root: True if called on the root of a tree to be merged, False
            otherwise. See the discussion below.

    Returns:
        The return value of os.stat or os.lstat, or possibly None if the path
        does not exist.

    When root is True, indicating that path is at the root of one of these
    trees, this permissiveness is disabled, as all roots are required to be
    present. If one is absent, an exception will be raised. When root is True,
    os.stat will be used, as this is the one case when it is desirable to
    operate on a symbolic link’s target.

    When root is False, os.lstat will be used to operate on symbolic links
    directly, and a missing path will cause None to be returned.
    """
    if root:
        return os.stat(path)

    try:
        return os.lstat(path)
    except OSError as e:
        if e.errno == errno.ENOENT:
            return None
        raise


def _file_type_for_stat(st):
    """Returns a string indicating the type of directory entry in st.

    Args:
        st: The return value of os.stat or os.lstat.

    Returns:
        'symbolic link', 'file', or 'directory'.
    """
    if stat.S_ISLNK(st.st_mode):
        return 'symbolic_link'
    if stat.S_ISREG(st.st_mode):
        return 'file'
    if stat.S_ISDIR(st.st_mode):
        return 'directory'

    raise Exception('unknown file type for mode 0o%o' % mode)


def _sole_list_element(l, exception_message):
    """Assures that every element in a list is identical.

    Args:
        l: The list to consider.
        exception_message: A message used to convey failure if every element in
            l is not identical.

    Returns:
        The value of each identical element in the list.
    """
    s = set(l)
    if len(s) != 1:
        raise Exception(exception_message)

    return l[0]


def _read_plist(path):
    """Reads a macOS property list, API compatibility adapter."""
    with open(path, 'rb') as file:
        try:
            # New API, available since Python 3.4.
            return plistlib.load(file)
        except AttributeError:
            # Old API, available (but deprecated) until Python 3.9.
            return plistlib.readPlist(file)


def _write_plist(value, path):
    """Writes a macOS property list, API compatibility adapter."""
    with open(path, 'wb') as file:
        try:
            # New API, available since Python 3.4.
            plistlib.dump(value, file)
        except AttributeError:
            # Old API, available (but deprecated) until Python 3.9.
            plistlib.writePlist(value, file)


class CantMergeException(Exception):
    """Raised when differences exist between input files such that they cannot
    be merged successfully.
    """
    pass


def _merge_info_plists(input_paths, output_path):
    """Merges multiple macOS Info.plist files.

    Args:
        input_plists: A list of paths containing Info.plist files to be merged.
        output_plist: The path of the merged Info.plist to create.

    Raises:
        CantMergeException if all input_paths could not successfully be merged
        into output_path.

    A small number of differences are tolerated in the input Info.plists. If a
    key identifying the build environment (OS or toolchain) is different in any
    of the inputs, it will be removed from the output. There are valid reasons
    to produce builds for different architectures using different toolchains or
    SDKs, and there is no way to rationalize these differences into a single
    value.

    If present, the Chrome KSChannelID family of keys are rationalized by using
    “universal” to identify the architecture (compared to, for example,
    “arm64”.)
    """
    input_plists = [_read_plist(x) for x in input_paths]
    output_plist = input_plists[0]
    for index in range(1, len(input_plists)):
        input_plist = input_plists[index]
        for key in set(input_plist.keys()) | set(output_plist.keys()):
            if input_plist.get(key, None) == output_plist.get(key, None):
                continue
            if key in ('BuildMachineOSBuild', 'DTCompiler', 'DTPlatformBuild',
                       'DTPlatformName', 'DTPlatformVersion', 'DTSDKBuild',
                       'DTSDKName', 'DTXcode', 'DTXcodeBuild'):
                if key in input_plist:
                    del input_plist[key]
                if key in output_plist:
                    del output_plist[key]
            elif key == 'KSChannelID' or key.startswith('KSChannelID-'):
                # These keys are Chrome-specific, where it’s only present in the
                # outer browser .app’s Info.plist.
                #
                # Ensure that the values match the expected format as a
                # prerequisite to what follows.
                key_tail = key[len('KSChannelID'):]
                input_value = input_plist.get(key, '')
                output_value = output_plist.get(key, '')
                assert input_value.endswith(key_tail)
                assert output_value.endswith(key_tail)

                # Find the longest common trailing sequence of hyphen-separated
                # elements, and use that as the trailing sequence of the new
                # value.
                input_parts = reversed(input_value.split('-'))
                output_parts = output_value.split('-')
                output_parts.reverse()
                new_parts = []
                for input_part, output_part in zip(input_parts, output_parts):
                    if input_part == output_part:
                        new_parts.append(output_part)
                    else:
                        break

                # Prepend “universal” to the entire value if it’s not already
                # there.
                if len(new_parts) == 0 or new_parts[-1] != 'universal':
                    new_parts.append('universal')
                output_plist[key] = '-'.join(reversed(new_parts))
                assert output_plist[key] != ''
            else:
                raise CantMergeException(input_paths[index], output_path)

    _write_plist(output_plist, output_path)


def _universalize(input_paths, output_path, root):
    """Merges multiple trees into a “universal” tree.

    This function provides the recursive internal implementation for
    universalize.

    Args:
        input_paths: The input directory trees to be merged.
        output_path: The merged tree to produce.
        root: True if operating at the root of the input and output trees.
    """
    input_stats = [_stat_or_none(x, root) for x in input_paths]
    for index in range(len(input_paths) - 1, -1, -1):
        if input_stats[index] is None:
            del input_paths[index]
            del input_stats[index]

    input_types = [_file_type_for_stat(x) for x in input_stats]
    type = _sole_list_element(
        input_types,
        'varying types %r for input paths %r' % (input_types, input_paths))

    if type == 'file':
        identical = True
        for index in range(1, len(input_paths)):
            if not filecmp.cmp(input_paths[0], input_paths[index]):
                identical = False
                if (os.path.basename(output_path) == 'Info.plist' or
                        os.path.basename(output_path).endswith('-Info.plist')):
                    _merge_info_plists(input_paths, output_path)
                else:
                    command = ['lipo', '-create', '-output', output_path]

                    # Force 16kB alignment for both x86_64 and arm64 slices. The
                    # inherent alignment requirement for x86_64 (absent Rosetta
                    # x86_64-on-arm64 concerns) is 4kB, and that is what lipo
                    # traditionally aligned x86_64 slices to. Since
                    # cctools-959.0.1 (Xcode 11.4), lipo attempts to guess the
                    # desired alignment of each slice, with the sometimes
                    # comical result being a slice over-aligned for its
                    # architecture. Over-alignment is normally benign, but
                    # https://crbug.com/1281111 documents a bug caused by “slice
                    # mobility” in the the main executable across updates, when
                    # the x86_64 slice moved from its traditional offset of 4kB
                    # to 16kB as a result of over-aligning. Until a code change
                    # lifts that restriction, the main executable’s physical
                    # layout across the installed base is frozen. In order to
                    # ensure that this temporary requirement can be met,
                    # artificially inflate the x86_64 slice’s alignment
                    # requirement to 16kB to keep its location stable. The arm64
                    # slice’s alignment requirement is also frozen at 16kB,
                    # although this is the correct value for that architecture.
                    #
                    # TODO(mark): Implement “Change 3” from
                    # https://crbug.com/1281111#c33 by reducing the x86_64
                    # alignment requirement to 4kB and truncating this comment,
                    # or if appropriate, implement “Change 3A” instead, updating
                    # this comment with a revised rationale.
                    command.extend(['-segalign', 'x86_64', '0x4000'])
                    command.extend(['-segalign', 'arm64', '0x4000'])

                    command.extend(input_paths)
                    subprocess.check_call(command)

        if identical:
            shutil.copyfile(input_paths[0], output_path)
    elif type == 'directory':
        os.mkdir(output_path)

        entries = set()
        for input in input_paths:
            entries.update(os.listdir(input))

        for entry in entries:
            input_entry_paths = [os.path.join(x, entry) for x in input_paths]
            output_entry_path = os.path.join(output_path, entry)
            _universalize(input_entry_paths, output_entry_path, False)
    elif type == 'symbolic_link':
        targets = [os.readlink(x) for x in input_paths]
        target = _sole_list_element(
            targets, 'varying symbolic link targets %r for input paths %r' %
            (targets, input_paths))
        os.symlink(target, output_path)

    input_permissions = [stat.S_IMODE(x.st_mode) for x in input_stats]
    permission = _sole_list_element(
        input_permissions, 'varying permissions %r for input paths %r' %
        (['0o%o' % x for x in input_permissions], input_paths))

    os.lchmod(output_path, permission)

    if type != 'file' or identical:
        input_mtimes = [x.st_mtime for x in input_stats]
        if len(set(input_mtimes)) == 1:
            times = (time.time(), input_mtimes[0])
            try:
                # follow_symlinks is only available since Python 3.3.
                os.utime(output_path, times, follow_symlinks=False)
            except TypeError:
                # If it’s a symbolic link and this version of Python isn’t able
                # to set its timestamp, just leave it alone.
                if type != 'symbolic_link':
                    os.utime(output_path, times)
        elif type == 'directory':
            # Always touch directories, in case a directory is a bundle, as a
            # cue to LaunchServices to invalidate anything it may have cached
            # about the bundle as it was being built.
            os.utime(output_path, None)


def universalize(input_paths, output_path):
    """Merges multiple trees into a “universal” tree.

    Args:
        input_paths: The input directory trees to be merged.
        output_path: The merged tree to produce.

    input_paths are expected to be parallel directory trees. Each directory
    entry at a given subpath in the input_paths, if present, must be identical
    to all others when present, with these exceptions:
     - Mach-O files that are not identical are merged using lipo.
     - Info.plist files that are not identical are merged by _merge_info_plists.
    """
    rmtree_on_error = not os.path.exists(output_path)
    try:
        return _universalize(input_paths, output_path, True)
    except:
        if rmtree_on_error and os.path.exists(output_path):
            shutil.rmtree(output_path)
        raise


def main(args):
    parser = argparse.ArgumentParser(
        description='Merge multiple single-architecture directory trees into a '
        'single universal tree.')
    parser.add_argument(
        'inputs',
        nargs='+',
        metavar='input',
        help='An input directory tree to be merged. At least two inputs must '
        'be provided.')
    parser.add_argument('output', help='The merged directory tree to produce.')
    parsed = parser.parse_args(args)
    if len(parsed.inputs) < 2:
        raise Exception('too few inputs')

    universalize(parsed.inputs, parsed.output)


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
