#!/usr/bin/env python2

# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Dumps info from a ExamplePreprocessorConfig protobuf file.

Prints feature names, types, and bucket values.
"""

import os
import sys
import textwrap
from enum import Enum
from google.protobuf import text_format

class FeatureType(Enum):
  CATEGORICAL = 'categorical'
  BUCKETED = 'bucketed'
  SCALAR = 'scalar'


def ReadConfig(pb_file):
  """Parses the protobuf containing the example preprocessor config."""
  import example_preprocessor_pb2
  config = example_preprocessor_pb2.ExamplePreprocessorConfig()
  with open(pb_file) as pb:
    config.ParseFromString(pb.read())
  return config


def PrintExamplePreprocessorConfig(pb_file):
  """Prints the features listed the example preprocessor config."""
  config = ReadConfig(pb_file)

  features = set()
  for feature_index in sorted(config.feature_indices):
    # For string or string list feature types, remove the "_value" suffix to get
    # the base name.
    name_parts = feature_index.split('_')
    base_name = name_parts[0]
    # Skip additional values of the same base name.
    if base_name in features:
      continue

    features.add(base_name)
    if len(name_parts) == 1:
      feature_type = FeatureType.SCALAR
    elif base_name in config.bucketizers:
      feature_type = FeatureType.BUCKETED
    else:
      feature_type = FeatureType.CATEGORICAL
    description = '* %s (%s)' % (base_name, feature_type.value)

    if feature_type == FeatureType.BUCKETED:
      description += ':\n\t'
      boundaries = config.bucketizers[base_name].boundaries
      bucket_str = ', '.join(['%.1f' % bucket for bucket in boundaries])

      # Indent description by a tab and wrap text.
      max_len = 80 - 8  # Leave at least 8 columns for tab width.
      description += ('\n\t').join(textwrap.wrap(bucket_str, max_len))
    print description
  return 0


def Main(args):
  if len(args) != 2:
    print 'Usage: %s <out_dir> <path/to/example_preprocessor_config.pb>' % (
        __file__)
    return 1

  out_dir = args[0]
  if not os.path.isdir(out_dir):
    print 'Could not find out directory: %s' % out_dir
    return 1

  pb_file = args[1]
  if not os.path.isfile(pb_file):
    print 'Protobuf file not found: %s' % pb_file
    return 1

  proto_dir = os.path.join(out_dir, 'pyproto/components/assist_ranker/proto')
  if not os.path.isdir(proto_dir):
    print 'Proto directory not found: %s' % proto_dir
    print 'Build the "components/assist_ranker/proto" target'
    print '  (usually built with chrome)'
    return 1

  # Allow importing the ExamplePreprocessorConfig proto definition.
  sys.path.insert(0, proto_dir)
  PrintExamplePreprocessorConfig(pb_file)


if __name__ == '__main__':
  sys.exit(Main(sys.argv[1:]))
