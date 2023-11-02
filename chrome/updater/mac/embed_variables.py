# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import optparse
import os
import sys


def embed_version(input_file, output_file, version, product_full_name):
    fin = open(input_file, 'r')
    fout = open(output_file, 'w')

    if version:
        replace_string = 'UPDATE_VERSION=\"' + version + '\"'
        for line in fin:
            fout.write(line.replace('UPDATE_VERSION=', replace_string))

    if product_full_name:
        replace_string = 'PRODUCT_NAME=\"' + product_full_name + '\"'
        for line in fin:
            fout.write(line.replace('PRODUCT_NAME=', replace_string))

    fin.close()
    fout.close()

    os.chmod(output_file, 0o755)


def parse_options():
    parser = optparse.OptionParser()
    parser.add_option('-i', '--input_file', help='Path to the input script.')
    parser.add_option('-o',
                      '--output_file',
                      help='Path to where we should output the script')
    parser.add_option('-v',
                      '--version',
                      help='Version of the application bundle being built.')
    parser.add_option('-p',
                      '--product_full_name',
                      help='Name of the product being built.')
    options, _ = parser.parse_args()

    if not options.version and not options.product_full_name:
        parser.error('You must provide a version or a product name')

    return options


def main(options):
    embed_version(options.input_file, options.output_file, options.version,
                  options.product_full_name)
    return 0


if '__main__' == __name__:
    options = parse_options()
    sys.exit(main(options))
