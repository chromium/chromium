#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Syncs WebUI toolbar flags from `enabled.sh` across `about_flags.cc` and
`tools/perf/`.
"""

import argparse
from collections.abc import Callable, Iterable
from pathlib import Path
import re
import subprocess
import sys

_SHARD_NAMES = (
  'linux-r350-perf_map.json',
  'mac-m1-pro-perf_map.json',
  'win-10-perf_map.json',
)
_SHARD_PAT = (
  r'("browser_startup\.crossbench": \{\s*'
  r'"crossbench_name": "browser-startup",\s*'
  r'"arguments": \[\s*"--enable-features=)[^"]*(")'
)

_Target = tuple[str, list[tuple[str, Callable[[re.Match[str]], str]]]]


def format_block(entries: list[str], cpp_end: bool = False) -> str:
  chunks = []
  for i, entry in enumerate(entries):
    full = entry + ('' if i == len(entries) - 1 else ',')
    pieces = [full] if len(full) <= 74 else re.findall(r'[^/]+/[^/]+/?', full)
    if ''.join(pieces) != full:
      sys.exit(f'Cannot wrap malformed feature entry: {entry}')
    for piece in pieces:
      if len(piece) <= 74:
        chunks.append(piece)
      else:
        head, tail = piece.split('/', 1)
        chunks.append(f'{head}/')
        while len(tail) > 74:
          chunks.append(tail[:72])
          tail = tail[72:]
        chunks.append(tail)
  lines = [f'    "{c}"' for c in chunks]
  if cpp_end:
    lines[-1] += ';'
  return '\n'.join(lines)


def ci_sorted(items: Iterable[str]) -> list[str]:
  return sorted(items, key=lambda s: (s.lower(), s))


def read_enabled_features(script_dir: Path) -> list[str]:
  """Runs `enabled.sh` and returns the parsed feature list."""
  enabled_output = subprocess.check_output(
    [str(script_dir / 'enabled.sh')], text=True
  )
  return [f for f in enabled_output.strip().split(',') if f]


def build_targets(
  raw_features: list[str], update_crossbench: bool
) -> list[_Target]:
  """Builds per-file replacement rules from the enabled feature list."""
  about_enable = format_block(ci_sorted(raw_features) + ['WebUIToolbar'], True)
  disable_set = {f.split(':', 1)[0] for f in raw_features} | {
    'WebUILocationBar'
  }
  about_disable = format_block(ci_sorted(disable_set) + ['WebUIToolbar'], True)
  perf_entries = sorted(raw_features)
  perf_block = format_block(perf_entries)
  perf_joined = ','.join(perf_entries)

  targets: list[_Target] = [
    (
      'chrome/browser/about_flags.cc',
      [
        (
          r'(constexpr char kWebUIToolbarEnableFeatures\[\] =\n)'
          r'(?:    "[^"]*";?\n)+',
          lambda m: f'{m.group(1)}{about_enable}\n',
        ),
        (
          r'(constexpr char kWebUIToolbarDisableFeatures\[\] =\n)'
          r'(?:    "[^"]*";?\n)+',
          lambda m: f'{m.group(1)}{about_disable}\n',
        ),
      ],
    ),
    (
      'tools/perf/core/bot_platforms.py',
      [
        (
          r'(  INITIAL_WEBUI_FEATURES = \(\n)(?:    "[^"]*"\n)+(\s*\))',
          lambda m: f'{m.group(1)}{perf_block}\n{m.group(2)}',
        )
      ],
    ),
    *[
      (
        f'tools/perf/core/shard_maps/{name}',
        [(_SHARD_PAT, lambda m: f'{m.group(1)}{perf_joined}{m.group(2)}')],
      )
      for name in _SHARD_NAMES
    ],
  ]
  if update_crossbench:
    targets.append((
      'third_party/crossbench/config/benchmark/'
      'browser_startup/browser.config.hjson',
      [
        (
          r'("flags": "--enable-features=)[^"]*(")',
          lambda m: f'{m.group(1)}{perf_joined}{m.group(2)}',
        )
      ],
    ))
  return targets


def sync_targets(
  repo_root: Path, targets: list[_Target], check_only: bool
) -> list[str]:
  """Applies replacement rules to each target file and returns changed paths."""
  changed = []
  for rel_path, rules in targets:
    path = repo_root / rel_path
    if not path.exists():
      sys.exit(f'Expected file not found: {path}')
    old_text = path.read_text(encoding='utf-8')
    new_text = old_text
    for pattern, repl in rules:
      new_text, count = re.subn(pattern, repl, new_text, count=1)
      if count != 1:
        sys.exit(f'Failed to match {rel_path} (matched {count} times).')
    if new_text != old_text:
      changed.append(rel_path)
      if not check_only:
        path.write_text(new_text, encoding='utf-8', newline='\n')
  return changed


def parse_args(argv: list[str] | None = None) -> argparse.Namespace:
  """Parses command-line arguments."""
  parser = argparse.ArgumentParser()
  parser.add_argument('--check', action='store_true')
  parser.add_argument('--update-crossbench', action='store_true')
  return parser.parse_args(argv)


def main() -> int:
  args = parse_args()
  script_dir = Path(__file__).resolve().parent
  repo_root = script_dir.parents[5]
  raw_features = read_enabled_features(script_dir)
  targets = build_targets(raw_features, args.update_crossbench)
  changed = sync_targets(repo_root, targets, args.check)

  if args.check:
    if changed:
      script = (
        'chrome/browser/ui/webui/webui_toolbar/scripts/'
        'generate_chrome_flags.py'
      )
      msg = 'Out of sync with enabled.sh:\n  ' + '\n  '.join(changed)
      print(f'{msg}\nRun: {script}', file=sys.stderr)
      return 1
  elif changed:
    print('Updated:\n  ' + '\n  '.join(changed))
  else:
    print('All target files are already in sync with enabled.sh.')
  return 0


if __name__ == '__main__':
  sys.exit(main())
