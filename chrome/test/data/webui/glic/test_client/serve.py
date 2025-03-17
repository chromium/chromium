#!/usr/bin/env python3
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""
Builds and serves the glic test client.
"""

import argparse
import http.server
import os
import shutil
import socketserver
import subprocess
import sys


def build(outdir: str):
    subprocess.run([
        shutil.which('autoninja'), '-C', outdir,
        'chrome/test/data/webui/glic:generate_test_files'
    ],
                   stdout=sys.stdout,
                   stderr=sys.stderr,
                   check=True)



class RequestHandler(http.server.SimpleHTTPRequestHandler):
    directory = None

    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=RequestHandler.directory, **kwargs)

    def end_headers(self):
        self.send_header("Cache-Control", "no-cache, no-store")
        self.send_header("Expires", "0")
        http.server.SimpleHTTPRequestHandler.end_headers(self)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('-C',
                        '--outdir',
                        help='Build output directory',
                        required=True)
    parser.add_argument('-p',
                        '--port',
                        help='HTTP Port',
                        type=int,
                        default=8000)
    parser.add_argument('-n',
                        '--nobuild',
                        help='Skips the build step',
                        action='store_true')
    args = parser.parse_args()

    RequestHandler.directory = f'{args.outdir}/gen/chrome/test/data/webui/glic'
    RequestHandler.extensions_map['.js'] = 'text/javascript'

    if not args.nobuild:
        try:
            build(args.outdir)
            print("Test client build done.")
        except subprocess.CalledProcessError as e:
            print("Test client build error; check build output above.")
            sys.exit(1)

    if not os.path.isdir(RequestHandler.directory):
        print(f'Directory does not exist: {RequestHandler.directory}',
              file=sys.stderr)
        sys.exit(1)

    with socketserver.ThreadingTCPServer(("", args.port),
                                         RequestHandler) as httpd:
        print("Server started at localhost:" + str(args.port))
        httpd.serve_forever()


if __name__ == '__main__':
    main()
