#!/usr/bin/env python3

# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import base64
import http.server
import json
import ssl

from absl import app, flags

FLAGS = flags.FLAGS
flags.DEFINE_string('addr', '0.0.0.0:443',
                    'Address (<host>:<port>) to bind server to.')
flags.DEFINE_string('cert', None,
                    'Path to certificate to run HTTPS server with.')
flags.DEFINE_string(
    'key', None,
    'Path to private key that establishes authenticity of `--cert`.')
flags.DEFINE_string('verify_cert', None,
                    'If provided, verify clients using this certificate.')
flags.mark_flag_as_required('cert')
flags.mark_flag_as_required('key')


class Handler(http.server.BaseHTTPRequestHandler):
  protocol_version = 'HTTP/1.1'

  def do_GET(self):
    # Unfortunately, this handler can't easily get the client certificate's
    # thumbprint from either form of `getpeercert()` [0]. This machine lacks a
    # package like `cryptography` to decode the raw DER. Therefore, send back
    # the entire DER to the orchestrating machine, which has `cryptography`
    # installed via vpython.
    #
    # [0]: https://docs.python.org/3/library/ssl.html#ssl.SSLSocket.getpeercert
    payload = b''
    if isinstance(self.request, ssl.SSLSocket):
      payload = base64.b64encode(self.request.getpeercert(binary_form=True))
    self.send_response(200)
    self.send_header('Content-Type', 'text/plain')
    self.send_header('Content-Length', len(payload))
    self.end_headers()
    self.wfile.write(payload)


def serve(argv):
  host, _, port = FLAGS.addr.rpartition(':')
  server = http.server.ThreadingHTTPServer((host, int(port)), Handler)
  server.allow_reuse_address = True

  ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
  ctx.load_cert_chain(FLAGS.cert, FLAGS.key)
  if FLAGS.verify_cert:
    print(f'Using {FLAGS.verify_cert!r} to verify clients')
    ctx.verify_mode = ssl.CERT_REQUIRED
    ctx.load_verify_locations(FLAGS.verify_cert)

  with server:
    server.socket = ctx.wrap_socket(server.socket, server_side=True)
    print(f'Server started on {server.server_address}')
    server.serve_forever()


if __name__ == '__main__':
  app.run(serve)
