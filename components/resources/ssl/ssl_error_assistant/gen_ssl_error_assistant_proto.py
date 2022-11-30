#!/usr/bin/env python3
# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
 Convert the ASCII ssl_error_assistant.asciipb proto into a binary resource.
"""

import base64
import os
import sys

# Subdirectory to be copied to Google Cloud Storage. Contains a copy of the
# generated proto under a versioned directory.
GS_COPY_DIR = "gs_copy"

# Import the binary proto generator. Walks up to the root of the source tree
# which is five directories above, and finds the protobufs directory from there.
proto_generator_path = os.path.normpath(os.path.join(os.path.abspath(__file__),
    *[os.path.pardir] * 5 + ['components/resources/protobufs']))
sys.path.insert(0, proto_generator_path)
from binary_proto_generator import BinaryProtoGenerator

def MakeSubDirs(outfile):
  """ Make the subdirectories needed to create file |outfile| """
  dirname = os.path.dirname(outfile)
  if not os.path.exists(dirname):
    os.makedirs(dirname)

class SSLErrorAssistantProtoGenerator(BinaryProtoGenerator):
  def ImportProtoModule(self):
    import ssl_error_assistant_pb2
    globals()['ssl_error_assistant_pb2'] = ssl_error_assistant_pb2

  def EmptyProtoInstance(self):
    return ssl_error_assistant_pb2.SSLErrorAssistantConfig()

  def ValidatePb(self, opts, pb):
    assert pb.version_id > 0
    assert len(pb.captive_portal_cert) > 0
    for cert in pb.captive_portal_cert:
      assert(cert.sha256_hash.startswith("sha256/"))
      decoded_hash = base64.b64decode(cert.sha256_hash[len("sha256/"):])
      assert(len(decoded_hash) == 32)

  def ProcessPb(self, opts, pb):
    binary_pb_str = pb.SerializeToString()
    outfile = os.path.join(opts.outdir, opts.outbasename)

    # Write two copies of the proto:
    # 1. Under the root of the gen directory for .grd files to refer to
    #    (./ssl_error_assistant/ssl_error_assistant.pb)
    # 2. Under a versioned directory for the proto pusher to refer to
    #    (./ssl_error_assistant/gs_copy/<version>/all/ssl_error_assistant.pb)
    outfile = os.path.join(opts.outdir, opts.outbasename)
    with open(outfile, 'wb') as f:
      f.write(binary_pb_str)

    outfile_copy = os.path.join(opts.outdir, GS_COPY_DIR, str(pb.version_id),
                                "all", opts.outbasename)
    MakeSubDirs(outfile_copy)
    with open(outfile_copy, 'wb') as f:
      f.write(binary_pb_str)


def main():
  return SSLErrorAssistantProtoGenerator().Run()

if __name__ == '__main__':
  sys.exit(main())
