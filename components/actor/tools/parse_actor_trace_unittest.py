#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import shutil
import sys
import tempfile
import unittest

# Ensure the Chromium src directory is in sys.path when running standalone.
_SRC_PATH = os.path.abspath(
    os.path.join(os.path.dirname(__file__), os.pardir, os.pardir, os.pardir)
)
if _SRC_PATH not in sys.path:
  sys.path.insert(0, _SRC_PATH)

from unittest import mock

from components.actor.tools.parse_actor_trace import (
    extract_raw_payload,
    extract_trace_to_dir,
    find_or_build_protoc,
    parse_actor_trace,
)


class ParseActorTraceTest(unittest.TestCase):

  def setUp(self):
    self.temp_dir = tempfile.mkdtemp()

  def tearDown(self):
    shutil.rmtree(self.temp_dir, ignore_errors=True)

  @mock.patch('components.actor.tools.parse_actor_trace.parse_actor_trace')
  def test_extract_trace_to_dir_parented(self, mock_parse_trace):
    apc_payload_1 = b'fake_apc_proto_data_step_1'
    jpg_payload_1 = bytes([0xFF, 0xD8, 0xFF, 0xE0]) + b'fake_jpeg_step_1'
    apc_payload_2 = b'fake_apc_proto_data_step_2'
    png_payload_2 = (
        bytes([0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A])
        + b'fake_png_step_2'
    )

    mock_parse_trace.return_value = [
        {
            'parent_id': 10,
            'name': 'PageContext',
            'timestamp': 100,
            'data': apc_payload_1,
        },
        {
            'parent_id': 10,
            'name': 'Screenshot',
            'timestamp': 110,
            'data': jpg_payload_1,
        },
        {
            'parent_id': 20,
            'name': 'PageContext',
            'timestamp': 200,
            'data': apc_payload_2,
        },
        {
            'parent_id': 20,
            'name': 'Screenshot',
            'timestamp': 210,
            'data': png_payload_2,
        },
    ]

    trace_file = os.path.join(self.temp_dir, 'test_trace.pb')
    with open(trace_file, 'wb') as f:
      f.write(b'fake_trace_data')

    out_dir = os.path.join(self.temp_dir, 'extracted_parented')
    extract_trace_to_dir(trace_file, out_dir)

    # Check that files were created
    step1_apc = os.path.join(out_dir, 'step1_apc.pb')
    step1_img = os.path.join(out_dir, 'step1_screenshot.jpg')
    step2_apc = os.path.join(out_dir, 'step2_apc.pb')
    step2_img = os.path.join(out_dir, 'step2_screenshot.png')

    self.assertTrue(os.path.exists(step1_apc))
    self.assertTrue(os.path.exists(step1_img))
    self.assertTrue(os.path.exists(step2_apc))
    self.assertTrue(os.path.exists(step2_img))

    with open(step1_apc, 'rb') as f:
      self.assertEqual(f.read(), apc_payload_1)
    with open(step1_img, 'rb') as f:
      self.assertEqual(f.read(), jpg_payload_1)
    with open(step2_apc, 'rb') as f:
      self.assertEqual(f.read(), apc_payload_2)
    with open(step2_img, 'rb') as f:
      self.assertEqual(f.read(), png_payload_2)

  @mock.patch('components.actor.tools.parse_actor_trace.parse_actor_trace')
  def test_extract_trace_to_dir_unparented(self, mock_parse_trace):
    apc_payload_1 = b'fake_apc_proto_data_step_1'
    jpg_payload_1 = bytes([0xFF, 0xD8, 0xFF, 0xE0]) + b'fake_jpeg_step_1'

    mock_parse_trace.return_value = [
        {'name': 'PageContext', 'timestamp': 100, 'data': apc_payload_1},
        {'name': 'Screenshot', 'timestamp': 110, 'data': jpg_payload_1},
    ]

    trace_file = os.path.join(self.temp_dir, 'test_unparented.pb')
    with open(trace_file, 'wb') as f:
      f.write(b'fake_trace_data')

    out_dir = os.path.join(self.temp_dir, 'extracted_unparented')
    extract_trace_to_dir(trace_file, out_dir)

    step1_apc = os.path.join(out_dir, 'step1_apc.pb')
    step1_img = os.path.join(out_dir, 'step1_screenshot.jpg')
    self.assertTrue(os.path.exists(step1_apc))
    self.assertTrue(os.path.exists(step1_img))

  @mock.patch('components.actor.tools.parse_actor_trace.parse_actor_trace')
  def test_extract_trace_to_dir_mixed(self, mock_parse_trace):
    apc_payload_1 = b'fake_apc_proto_data_step_1'
    jpg_payload_1 = bytes([0xFF, 0xD8, 0xFF, 0xE0]) + b'fake_jpeg_step_1'
    apc_payload_2 = b'fake_apc_proto_data_step_2'
    jpg_payload_2 = bytes([0xFF, 0xD8, 0xFF, 0xE0]) + b'fake_jpeg_step_2'

    mock_parse_trace.return_value = [
        {
            'parent_id': 10,
            'name': 'PageContext',
            'timestamp': 100,
            'data': apc_payload_1,
        },
        {
            'parent_id': 10,
            'name': 'Screenshot',
            'timestamp': 110,
            'data': jpg_payload_1,
        },
        {
            'name': 'PageContext',
            'timestamp': 200,
            'data': apc_payload_2,
        },
        {
            'name': 'Screenshot',
            'timestamp': 210,
            'data': jpg_payload_2,
        },
    ]

    trace_file = os.path.join(self.temp_dir, 'test_mixed.pb')
    with open(trace_file, 'wb') as f:
      f.write(b'fake_trace_data')

    out_dir = os.path.join(self.temp_dir, 'extracted_mixed')
    extract_trace_to_dir(trace_file, out_dir)

    step1_apc = os.path.join(out_dir, 'step1_apc.pb')
    step1_img = os.path.join(out_dir, 'step1_screenshot.jpg')
    step2_apc = os.path.join(out_dir, 'step2_apc.pb')
    step2_img = os.path.join(out_dir, 'step2_screenshot.jpg')

    self.assertTrue(os.path.exists(step1_apc))
    self.assertTrue(os.path.exists(step1_img))
    self.assertTrue(os.path.exists(step2_apc))
    self.assertTrue(os.path.exists(step2_img))

  def test_extract_raw_payload(self):
    raw_binary = b'abcd\x00\x01\x02\x03'
    self.assertEqual(extract_raw_payload(raw_binary), raw_binary)

    b64_str = 'aGVsbG8gd29ybGQ='
    self.assertEqual(extract_raw_payload(b64_str), b'hello world')

  def test_find_or_build_protoc(self):
    # Test non-existent build directory returns None or builds
    non_existent = os.path.join(self.temp_dir, 'no_such_dir')
    self.assertIsNone(find_or_build_protoc(non_existent))


if __name__ == '__main__':
  unittest.main()
