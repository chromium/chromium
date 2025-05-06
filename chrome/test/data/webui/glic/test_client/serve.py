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
import time
import subprocess
import sys
import json

_HERE_PATH = os.path.dirname(__file__)
_SRC_PATH = os.path.normpath(
    os.path.join(_HERE_PATH, '..', '..', '..', '..', '..', '..'))
sys.path.insert(0, os.path.join(_SRC_PATH, 'third_party', 'protobuf',
                                'python'))

from google.protobuf.message import DecodeError
from google.protobuf import json_format

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
    flaky = False

    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=RequestHandler.directory, **kwargs)

    def do_GET(self):
        if self.flaky:
            if time.localtime().tm_min % 2 != 0:
                self.send_response(404)
                self.send_header('Content-type', 'text/html')
                self.end_headers()
                return
        super().do_GET()

    def _parse_apc(self):
        """Deserializes AnnotatedPageContent from the request payload and
           converts it to JSON (which is sent as a response)."""
        try:
            content_length = int(self.headers['Content-Length'])
            serialized_apc = self.rfile.read(content_length)
            import common_quality_data_pb2
            apc = common_quality_data_pb2.AnnotatedPageContent()
            apc.ParseFromString(serialized=serialized_apc)
            result = json_format.MessageToJson(apc)
            self.send_response(200)
            self.send_header('Content-type', 'application/json')
            self.end_headers()
            self.wfile.write(result.encode())
        except DecodeError:
            self.send_error(400, 'proto could not be parsed')

    def do_POST(self):
        if self.path == '/parse-apc':
            self._parse_apc()
        else:
            self.send_error(404, f'invalid path: ${self.path}')

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
    parser.add_argument('--flaky',
                        help="Alternates between 200 and" +
                        " 404 responses, every minute",
                        action="store_true")
    parser.add_argument('--bind-all-interfaces',
                        help='Serves on all interfaces' +
                        ' (by default serves only localhost)',
                        action='store_true')
    args = parser.parse_args()

    RequestHandler.directory = f'{args.outdir}/gen/chrome/test/data/webui/glic'
    RequestHandler.extensions_map['.js'] = 'text/javascript'
    RequestHandler.flaky = args.flaky

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

    # Allows us to import generated proto bindings for common_quality_data.proto.
    sys.path.insert(
        0,
        os.path.join(args.outdir, 'pyproto', 'components',
                     'optimization_guide', 'proto', 'features'))

    server_addr = '' if args.bind_all_interfaces else '127.0.0.1'

    with socketserver.ThreadingTCPServer((server_addr, args.port),
                                         RequestHandler) as httpd:
        print("Server started at localhost:" + str(args.port))
        httpd.serve_forever()


if __name__ == '__main__':
    main()
