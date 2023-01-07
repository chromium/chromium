#!/usr/bin/env python3
# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Compiles a text-format RenderPass protobuf message to binary format.
"""

import os
import sys

# go up 5 parent directories to //src/
path_to_src_root = os.path.join(
    os.path.abspath(__file__), *[os.path.pardir] * 5)

# allow importing modules from  //src/components/resources/protobufs
sys.path.insert(0, os.path.normpath(
    os.path.join(path_to_src_root, 'components/resources/protobufs')))

from binary_proto_generator import BinaryProtoGenerator

class RenderPassProtoGenerator(BinaryProtoGenerator):
  def ImportProtoModule(self):
    import compositor_frame_fuzzer_pb2
    globals()['compositor_frame_fuzzer_pb2'] = compositor_frame_fuzzer_pb2

  def EmptyProtoInstance(self):
    return compositor_frame_fuzzer_pb2.CompositorRenderPass()

  def ProcessPb(self, opts, pb):
    with open(os.path.join(opts.outdir, opts.outbasename), 'wb') as out_file:
      out_file.write(pb.SerializeToString())

def main():
  return RenderPassProtoGenerator().Run()

if __name__ == '__main__':
  sys.exit(main())
