#!/usr/bin/env python3
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""This script provides functionality for automatically keeping a list of
histograms and user actions used by segmentation platform models up to date.

By running this script as a binary, it automatically rewrites the golden files,
but it can also be used as a Python module for presubmit scripts.
"""

import os
import re
import sys

# The script needs to be run from the chromium src directory.
if __name__ == '__main__' and not os.path.exists('components'):
    sys.exit('This script must be run from the chromium src directory.')

SRC_ROOT = os.path.abspath(
    os.path.join(os.path.dirname(__file__), '..', '..', '..'))

# Path to the directory containing segmentation platform models.
# TODO(haileywang): Also include home_modules/ folder.
MODEL_DIR = os.path.join(SRC_ROOT, 'components', 'segmentation_platform',
                         'embedder', 'default_model')

# Name of the golden files.
# TODO(haileywang): Add ukm metrics.
HISTOGRAMS_FILENAME = 'histograms.txt'
USER_ACTIONS_FILENAME = 'user_actions.txt'


def _GetComponentDirectoryPath():
    """Returns the path to the current component."""
    return os.path.join(SRC_ROOT, 'components', 'segmentation_platform')


def _GetHistogramsFilePath():
    """Returns the path to the histograms golden file."""
    component_directory = _GetComponentDirectoryPath()
    return os.path.join(component_directory, 'tools', HISTOGRAMS_FILENAME)


def _GetUserActionsFilePath():
    """Returns the path to the user actions golden file."""
    component_directory = _GetComponentDirectoryPath()
    return os.path.join(component_directory, 'tools', USER_ACTIONS_FILENAME)


def _FindMetrics(cwd, patterns):
    """Finds all metrics matching the given patterns."""
    metrics = set()
    for root, _, files in os.walk(cwd):
        for filename in files:
            if not filename.endswith('.cc') or filename.endswith(
                ('_unittest.cc', '_test.cc')):
                continue

            file_path = os.path.join(root, filename)
            with open(file_path, 'r', encoding='utf-8') as f:
                try:
                    file_contents = f.read()
                    for pattern in patterns:
                        for match in pattern.finditer(file_contents):
                            metrics.add(match.group(1))
                except Exception as e:
                    print(f"Error reading file {file_path}: {e}")
    return sorted(list(metrics))


def _FindHistograms(cwd):
    """Finds all histograms used in the segmentation platform models."""
    histogram_patterns = [
        re.compile(r'From(?:Enum|Value)Histogram\s*\(\s*"([^"]+)"',
                   re.MULTILINE),
        re.compile(r'FromLatestOrDefaultValue\s*\(\s*"([^"]+)"', re.MULTILINE),
        re.compile(
            r'MetadataWriter::UMAFeature\s*\{[^}]*\.name\s*=\s*"([^"]+)"',
            re.MULTILINE),
        re.compile(r'features::UMA\w*\s*\(\s*"([^"]+)"', re.MULTILINE),
        re.compile(r'features::LatestOrDefaultValue\s*\(\s*"([^"]+)"',
                   re.MULTILINE),
    ]
    return _FindMetrics(cwd, histogram_patterns)


def _FindUserActions(cwd):
    """Finds all user actions used in the segmentation platform models."""
    user_action_patterns = [
        re.compile(r'FromUserAction\s*\(\s*"([^"]+)"', re.MULTILINE),
        re.compile(r'features::UserAction\s*\(\s*"([^"]+)"', re.MULTILINE),
    ]
    return _FindMetrics(cwd, user_action_patterns)


def _CreateFileContent(metrics):
    """Creates the content for the golden file."""
    return '\n'.join(metrics) + '\n'


def GetActualHistogramsFileContent():
    """Reads the current content of the histograms golden file."""
    file_path = _GetHistogramsFilePath()
    if not os.path.exists(file_path):
        return ""
    with open(file_path, 'r', encoding='utf-8') as f:
        return f.read()


def GetActualHistogramNames():
    """Returns the list of histogram names in the golden file."""
    histograms_content = GetActualHistogramsFileContent()
    segmentation_histograms = set(line.strip()
                                  for line in histograms_content.splitlines())
    segmentation_histograms.discard('')
    return segmentation_histograms


def GetExpectedHistogramsFileContent():
    """Creates the expected content of the histograms golden file."""
    histograms = _FindHistograms(MODEL_DIR)
    return _CreateFileContent(histograms)


def GetActualUserActionsFileContent():
    """Reads the current content of the user actions golden file."""
    file_path = _GetUserActionsFilePath()
    if not os.path.exists(file_path):
        return ""
    with open(file_path, 'r', encoding='utf-8') as f:
        return f.read()


def GetActualActionNames():
    """Returns the list of action names in the golden file."""
    actions_content = GetActualUserActionsFileContent()
    segmentation_actions = set(line.strip()
                               for line in actions_content.splitlines())
    segmentation_actions.discard('')
    return segmentation_actions


def GetExpectedUserActionsFileContent():
    """Creates the expected content of the user actions golden file."""
    user_actions = _FindUserActions(MODEL_DIR)
    return _CreateFileContent(user_actions)


def _WriteFile(file_path, content):
    """Writes the content to the given file path."""
    os.makedirs(os.path.dirname(file_path), exist_ok=True)
    with open(file_path, 'w', encoding='utf-8') as f:
        f.write(content)


def main():
    """Main function to update the golden files."""
    histograms_file_path = _GetHistogramsFilePath()
    expected_histograms_content = GetExpectedHistogramsFileContent()
    _WriteFile(histograms_file_path, expected_histograms_content)
    print(f"Updated {histograms_file_path}")

    user_actions_file_path = _GetUserActionsFilePath()
    expected_user_actions_content = GetExpectedUserActionsFileContent()
    _WriteFile(user_actions_file_path, expected_user_actions_content)
    print(f"Updated {user_actions_file_path}")


if __name__ == '__main__':
    main()
