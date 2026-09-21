#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""
Builds and serves the skills test client (SkillsWebViewV2).
"""

import argparse
import http.server
import os
import re
import shutil
import socketserver
import subprocess
import sys
import urllib.parse

FIRST_LINE_OF_INSTRUCTIONS = "Launch Chrome with Test Client Flags"

def build(outdir: str):
    subprocess.run(
        [
            shutil.which('autoninja'),
            '-C',
            outdir,
            'chrome/test/data/webui/skills:generate_test_files',
        ],
        stdout=sys.stdout,
        stderr=sys.stderr,
        check=True,
    )


class RequestHandler(http.server.SimpleHTTPRequestHandler):
    directory = None
    protocol_version = 'HTTP/1.0'

    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=RequestHandler.directory, **kwargs)

    def translate_path(self, path):
        parsed = urllib.parse.urlparse(path)
        clean_path = parsed.path.lstrip('/')

        # 1. Check if the path directly exists under directory.
        direct_file = os.path.join(self.directory, clean_path)
        if os.path.isfile(direct_file):
            return direct_file

        # Strip prefixes like 'chromeskills/' or 'test_client/'.
        stripped = clean_path
        if stripped.startswith('chromeskills/'):
            stripped = stripped[len('chromeskills/') :]
        elif stripped.startswith('test_client/'):
            stripped = stripped[len('test_client/') :]

        # 2. If it's a file with an extension, search in test_client and skills
        # subdirectories.
        if os.path.splitext(stripped)[1]:
            for prefix in [
                'test_client',
                os.path.join('skills', 'test_client'),
                '',
            ]:
                candidate = os.path.join(self.directory, prefix, stripped)
                if os.path.isfile(candidate):
                    return candidate
            return direct_file

        # 3. For SPA routes (e.g. /chromeskills, /chromeskills/browse,
        # /chromeskills/dialog, /chromeskills/yourSkills, /chromeskills/editor,
        # /), return index.html.
        for prefix in [
            'test_client',
            os.path.join('skills', 'test_client'),
            '',
        ]:
            index_candidate = os.path.join(self.directory, prefix, 'index.html')
            if os.path.isfile(index_candidate):
                return index_candidate

        return direct_file

    def end_headers(self):
        self.send_header('Cache-Control', 'no-cache, no-store')
        self.send_header('Expires', '0')
        self.send_header('Connection', 'close')
        super().end_headers()


def load_readme() -> str:
    readme_path = os.path.join(os.path.dirname(__file__), 'README.md')
    if os.path.isfile(readme_path):
        with open(readme_path, 'r', encoding='utf-8') as f:
            return f.read()
    return ''


def print_instructions(outdir: str, port: int):
    origin = f'http://localhost:{port}'

    banner = '=' * 80
    print(f'\n{banner}')
    print('  Skills Test Client (V2) Server Started')
    print(f'{banner}')
    print(f'Server URL:     {origin}/chromeskills/browse')
    print(f'Direct Test UI: {origin}/test_client/index.html\n')

    readme = load_readme()
    if readme:
        # Strip preamble
        readme = re.sub(
            rf'^.*{FIRST_LINE_OF_INSTRUCTIONS}\s*',
            '### Next, start Chrome as follows\n\n', readme, flags=re.DOTALL
        )
        # Replace default placeholders with runtime parameters.
        readme = readme.replace('out/Default', outdir).replace(
            'http://localhost:8000', origin
        )
        print(readme.strip())
        print(f'\n{banner}\n')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        '-C', '--outdir', help='Build output directory', required=True
    )
    parser.add_argument(
        '-p', '--port', help='HTTP Port', type=int, default=8000
    )
    parser.add_argument(
        '-n', '--nobuild', help='Skips the build step', action='store_true'
    )
    parser.add_argument(
        '--bind-all-interfaces',
        help='Serves on all interfaces (by default serves only localhost)',
        action='store_true',
    )
    args = parser.parse_args()

    RequestHandler.directory = (
        f'{args.outdir}/gen/chrome/test/data/webui/skills'
    )
    RequestHandler.extensions_map['.js'] = 'text/javascript'
    RequestHandler.extensions_map['.ts'] = 'text/plain'
    RequestHandler.extensions_map['.json'] = 'application/json'
    RequestHandler.extensions_map['.md'] = 'text/markdown'

    if not args.nobuild:
        try:
            build(args.outdir)
            print('Skills test client build done.')
        except subprocess.CalledProcessError:
            print(
                'Skills test client build error; check build output above.',
                file=sys.stderr,
            )
            sys.exit(1)

    if not os.path.isdir(RequestHandler.directory):
        print(
            f'Directory does not exist: {RequestHandler.directory}',
            file=sys.stderr,
        )
        sys.exit(1)

    server_addr = '' if args.bind_all_interfaces else '127.0.0.1'

    with socketserver.ThreadingTCPServer(
        (server_addr, args.port), RequestHandler
    ) as httpd:
        print_instructions(args.outdir, args.port)
        try:
            httpd.serve_forever()
        except KeyboardInterrupt:
            print('\nServer stopped.')


if __name__ == '__main__':
    main()
