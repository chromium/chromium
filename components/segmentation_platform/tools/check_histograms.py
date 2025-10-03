#!/usr/bin/env python3
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""This script provides functionality for automatically keeping a list of
histograms used by segmentation platform models up to date.

By running this script as a binary, it automatically rewrites the golden file,
but it can also be used as a Python module for presubmit scripts.
"""

import os
import re
import sys

# The script needs to be run from the chromium src directory.
if __name__ == '__main__' and not os.path.exists('components'):
    sys.exit('This script must be run from the chromium src directory.')

# Path to the directory containing segmentation platform models.
MODEL_DIR = os.path.join('components', 'segmentation_platform', 'embedder',
                         'default_model')

# Name of the golden file.
OUTPUT_FILENAME = 'histograms.txt'


def _GetComponentDirectoryPath():
    """Returns the path to the current component."""
    return os.path.join('components', 'segmentation_platform')


def _GetHistogramsFilePath():
    """Returns the path to the golden file."""
    component_directory = _GetComponentDirectoryPath()
    return os.path.join(component_directory, 'tools', OUTPUT_FILENAME)


def _FindHistograms(cwd):
    """Finds all histograms used in the segmentation platform models."""
    # Regex to find histogram names in various macros and structs.
    # The patterns handle multi-line macro invocations.
    histogram_patterns = [
        re.compile(r'SEGMENTATION_UMA_ENUM\s*\(\s*"([^"]+)"', re.MULTILINE),
        re.compile(r'From(?:Enum|Value)Histogram\s*\(\s*"([^"]+)"',
                   re.MULTILINE),
        re.compile(r'FromUserAction\s*\(\s*"([^"]+)"', re.MULTILINE),
        # This pattern is more specific to UMAFeature structs to avoid
        # capturing names from other structs like CustomInput.
        re.compile(
            r'MetadataWriter::UMAFeature\s*\{[^}]*\.name\s*=\s*"([^"]+)"',
            re.MULTILINE),
        re.compile(r'features::UserAction\s*\(\s*"([^"]+)"', re.MULTILINE),
        re.compile(r'features::UMA\w*\s*\(\s*"([^"]+)"', re.MULTILINE),
        re.compile(r'features::LatestOrDefaultValue\s*\(\s*"([^"]+)"',
                   re.MULTILINE),
    ]

    histograms = set()
    for root, _, files in os.walk(cwd):
        for filename in files:
            if filename.endswith('.cc') and not filename.endswith(
                ('_unittest.cc', '_test.cc')):
                file_path = os.path.join(root, filename)
                with open(file_path, 'r', encoding='utf-8') as f:
                    try:
                        file_contents = f.read()
                        for pattern in histogram_patterns:
                            for match in pattern.finditer(file_contents):
                                histograms.add(match.group(1))
                    except Exception as e:
                        print(f"Error reading file {file_path}: {e}")
    return sorted(list(histograms))


def _CreateHistogramsFileContent(histograms):
    """Creates the content for the golden file."""
    return '\n'.join(histograms) + '\n'


def GetActualHistogramsFileContent():
    """Reads the current content of the golden file."""
    file_path = _GetHistogramsFilePath()
    if not os.path.exists(file_path):
        return ""
    with open(file_path, 'r', encoding='utf-8') as f:
        return f.read()


def GetExpectedHistogramsFileContent():
    """Creates the expected content of the golden file."""
    histograms = _FindHistograms(MODEL_DIR)
    return _CreateHistogramsFileContent(histograms)


def _WriteHistogramsFile(file_path, content):
    """Writes the content to the given file path."""
    os.makedirs(os.path.dirname(file_path), exist_ok=True)
    with open(file_path, 'w', encoding='utf-8') as f:
        f.write(content)


def main():
    """Main function to update the golden file."""
    output_file_path = _GetHistogramsFilePath()
    expected_content = GetExpectedHistogramsFileContent()
    _WriteHistogramsFile(output_file_path, expected_content)
    print(f"Updated {output_file_path}")


if __name__ == '__main__':
    main()
