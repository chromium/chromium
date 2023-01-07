#!/usr/bin/env python
# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Script which process RendererPerfTest output.

Run viz_perftests.exe --gtest_filter=RendererPerfTest* > stdout.txt.
Then run this script as
  python process_renderer_perftest_results.py --test-stdout="stdout.txt"

The perf data will be collected and stored in "output.csv".
"""

import argparse
import csv
import logging
import sys


def SaveResultsAsCSV(csv_data, csv_filename):
  assert len(csv_data) > 0
  with open(csv_filename, 'wb') as csv_file:
    labels = sorted(csv_data[0].keys(), reverse=True)
    writer = csv.DictWriter(csv_file, fieldnames=labels)
    writer.writeheader()
    writer.writerows(csv_data)


def FindTestEntry(csv_data, test_name):
  for entry in csv_data:
    if entry['TestName'] == test_name:
      return entry
  return None


def ProcessOutput(lines):
  csv_data = []
  for line in lines:
    line = line.strip()
    if line.startswith('[ RUN      ]'):
      test_name = line.split('.')[1]
      entry = FindTestEntry(csv_data, test_name)
      if entry is None:
        entry = {'TestName': test_name}
        csv_data.append(entry)
    elif line.startswith('Using '):
      renderer = line.split()[1][:-8]
    elif line.startswith('*RESULT '):
      fps = line.split('=')[1].strip().split()[0]
      assert renderer == 'GL' or renderer == 'Skia'
      assert entry is not None
      entry['FPS_' + renderer] = fps
    elif line.startswith('Histogram: '):
      draw_to_swap_us_mean = line.split('=')[1].strip()
      assert renderer == 'GL' or renderer == 'Skia'
      assert entry is not None
      entry['DrawToSwap_' + renderer] = draw_to_swap_us_mean
    elif line.startswith('[       OK ]'):
      end_test_name = line.split('.')[1].split()[0]
      assert test_name == end_test_name
      test_name = None
      entry = None
      renderer = None
  return csv_data


def main():
  rest_args = sys.argv[1:]
  parser = argparse.ArgumentParser(
    description='Gather RendererPerfTest results.',
    formatter_class=argparse.ArgumentDefaultsHelpFormatter)
  parser.add_argument(
    '--test-stdout',
    metavar='FILE',
    help='Test stdout filename. Input of this script.')
  parser.add_argument(
    '--csv-file',
    metavar='FILE',
    default='output.csv',
    help='CSV filename. Output of this script. '
    'Default is output.csv.')

  options = parser.parse_args(rest_args)
  input_filename = options.test_stdout
  if input_filename is None:
    logging.error('Specify test stdout filename with --test-stdout.')
    return 0

  with open(input_filename, 'r') as input_file:
    lines = input_file.readlines()

  csv_data = ProcessOutput(lines)
  SaveResultsAsCSV(csv_data, options.csv_file)
  return 0


if __name__ == '__main__':
  sys.exit(main())
