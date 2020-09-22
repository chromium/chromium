# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys

def add_typ_dir_to_sys_path():
    path_to_typ = get_typ_dir()
    if path_to_typ not in sys.path:
        sys.path.insert(0, path_to_typ)

def get_typ_dir():
    return os.path.join(get_chromium_src_dir(), 'third_party', 'catapult',
                        'third_party', 'typ')

def add_tests_to_python_path():
  path = get_integration_tests_dir()
  if path not in os.environ['PATH']:
    os.environ['PATH'] += os.pathsep + path
  if path not in os.environ['PYTHONPATH']:
    os.environ['PYTHONPATH'] = os.pathsep + path

def get_integration_tests_dir():
  return os.path.dirname(os.path.dirname(
      os.path.realpath(__file__)))

def get_chromium_src_dir():
  return os.path.dirname(os.path.dirname(get_updater_dir()))

def get_updater_dir():
  return os.path.dirname(os.path.dirname(
      os.path.dirname(os.path.dirname(os.path.realpath(__file__)))))

