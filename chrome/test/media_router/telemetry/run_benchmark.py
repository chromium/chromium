# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys

import media_router_config

from core import benchmark_runner


def main():
  return benchmark_runner.main(media_router_config.Config(['benchmarks']))


if __name__ == '__main__':
  sys.exit(main())
