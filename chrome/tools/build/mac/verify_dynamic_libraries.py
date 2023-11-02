#!/usr/bin/env python
# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


"""
Verifies that a Mach-O image is linked against only specific dynamic libraries.

Usage:
    verify_image_libraries.py --image Foo.app/Contents/MacOS/Foo \
        --allow /usr/lib/libSystem.B.dylib
"""

from __future__ import print_function

import argparse
import subprocess
import sys


def verify_image_libraries(image_path, allowed_libraries, binary_path):
    """Verifies that the Mach-O image residing at |image_path| is only linked
    against dynamic libraries listed in |allowed_libraries|. The image does
    not have to link against everything that is allowed, but it cannot link
    against anything that is not allowed. Returns True if the image only
    links against allowed libraries, and otherwise returns False with an error
    printed.
    """
    # If |image_path| is a dynamic library, allow the LC_DYLIB_ID implicitly.
    output = subprocess.check_output(
        [binary_path + 'llvm-objdump', '--macho', '--dylib-id', image_path])
    dylib_id = output.decode('utf8').split('\n')[1]
    if dylib_id:
        allowed_libraries.append(dylib_id)

    output = subprocess.check_output(
        [binary_path + 'llvm-objdump', '--macho', '--dylibs-used', image_path])
    output = output.decode('utf8').strip()

    disallowed_libraries = []

    # Skip the first line of output, which is image_path.
    linked_libraries = output.split('\n')[1:]
    for linked_library in linked_libraries:
        # The line starts with a tab, followed by the image path, followed by
        # a space and then version information in parentheses.
        linked_library = linked_library.lstrip('\t')
        open_paren = linked_library.rindex('(')
        linked_library = linked_library[:open_paren - 1]

        if linked_library not in allowed_libraries:
            disallowed_libraries.append(linked_library)

    if not disallowed_libraries:
        return True

    print('{} linked against the following disallowed libraries:'.format(
        image_path),
          file=sys.stderr)
    for disallowed_library in disallowed_libraries:
        print('    {}'.format(disallowed_library), file=sys.stderr)

    return False


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--stamp',
                        help='Location to write a build stamp file.')
    parser.add_argument('--image',
                        required=True,
                        help='The Mach-O image to verify.')
    parser.add_argument(
        '--allow',
        action='append',
        default=[],
        help='Allow the specified dynamic library to be linked to the image.')
    parser.add_argument('-B', help='Path to look for the objdump binary.')
    args = parser.parse_args()

    if not verify_image_libraries(args.image, args.allow, args.B):
        return 1

    if args.stamp:
        open(args.stamp, 'w').close()

    return 0


if __name__ == '__main__':
    sys.exit(main())
