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
    os.path.join(os.path.dirname(__file__), '..', '..', '..')
)

# Paths to the directories containing segmentation platform models.
MODEL_DIRS = [
    os.path.join(
        SRC_ROOT,
        'components',
        'segmentation_platform',
        'embedder',
        'default_model',
    ),
    os.path.join(
        SRC_ROOT,
        'components',
        'segmentation_platform',
        'embedder',
        'home_modules',
    ),
]

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


# Matches an argument that is either a string literal (captured as group 1) or
# the name of a `const char[]` constant (captured as the `var` group).
_LITERAL_OR_CONST_ARG = r'(?:"([^"]+)"|(?P<var>\w+))'


def _FindMetrics(dirs, patterns):
    """Finds all metrics matching the given patterns.

    Each pattern captures the metric name as group 1. Patterns built with
    _LITERAL_OR_CONST_ARG may instead capture a constant name in the `var`
    group, which is resolved against the `const char[]` declarations in the
    same file.
    """
    metrics = set()
    const_decl_pattern = re.compile(
        r'(?:const|constexpr)\s+char\s+(\w+)\s*\[\]\s*=\s*"([^"]+)"',
        re.MULTILINE,
    )
    for cwd in dirs:
        for root, _, files in os.walk(cwd):
            for filename in files:
                if not filename.endswith('.cc') or filename.endswith(
                    ('_unittest.cc', '_test.cc')
                ):
                    continue

                file_path = os.path.join(root, filename)
                with open(file_path, 'r', encoding='utf-8') as f:
                    try:
                        file_contents = f.read()
                        # This is a flat per-file map that ignores namespaces
                        # and scopes, so if two constants in one file share a
                        # name, the last declaration wins. This assumes
                        # constants are file-scoped with unique names, which
                        # holds for home_modules/ today.
                        const_map = {
                            m.group(1): m.group(2)
                            for m in const_decl_pattern.finditer(file_contents)
                        }
                        for pattern in patterns:
                            for match in pattern.finditer(file_contents):
                                var_name = match.groupdict().get('var')
                                if var_name is None:
                                    metrics.add(match.group(1))
                                elif var_name in const_map:
                                    metrics.add(const_map[var_name])
                    except Exception as e:
                        print(f"Error reading file {file_path}: {e}")
    return sorted(list(metrics))


def _FindHistograms(dirs):
    """Finds all histograms used in the segmentation platform models."""
    histogram_patterns = [
        re.compile(
            r'From(?:Enum|Value)Histogram\s*\(\s*"([^"]+)"', re.MULTILINE
        ),
        re.compile(r'FromLatestOrDefaultValue\s*\(\s*"([^"]+)"', re.MULTILINE),
        re.compile(
            r'MetadataWriter::UMAFeature\s*\{[^}]*\.name\s*=\s*"([^"]+)"',
            re.MULTILINE,
        ),
        re.compile(r'features::UMA\w*\s*\(\s*"([^"]+)"', re.MULTILINE),
        re.compile(
            r'features::LatestOrDefaultValue\s*\(\s*"([^"]+)"', re.MULTILINE
        ),
        re.compile(
            r'DEFINE_UMA_FEATURE_\w+\s*\(\s*\w+\s*,\s*' + _LITERAL_OR_CONST_ARG,
            re.MULTILINE,
        ),
    ]
    return _FindMetrics(dirs, histogram_patterns)


def _FindUserActions(dirs):
    """Finds all user actions used in the segmentation platform models."""
    user_action_patterns = [
        re.compile(r'FromUserAction\s*\(\s*"([^"]+)"', re.MULTILINE),
        re.compile(r'features::UserAction\s*\(\s*"([^"]+)"', re.MULTILINE),
    ]
    return _FindMetrics(dirs, user_action_patterns)


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
    segmentation_histograms = set(
        line.strip() for line in histograms_content.splitlines()
    )
    segmentation_histograms.discard('')
    return segmentation_histograms


def GetExpectedHistogramsFileContent():
    """Creates the expected content of the histograms golden file."""
    histograms = _FindHistograms(MODEL_DIRS)
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
    segmentation_actions = set(
        line.strip() for line in actions_content.splitlines()
    )
    segmentation_actions.discard('')
    return segmentation_actions


def GetExpectedUserActionsFileContent():
    """Creates the expected content of the user actions golden file."""
    user_actions = _FindUserActions(MODEL_DIRS)
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
