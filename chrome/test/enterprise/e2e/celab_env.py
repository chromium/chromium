# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Bootstraps the chrome_ent_test wheel from the CELab CIPD package."""

import glob
import importlib.util
import os
import re
import shutil
import subprocess
import sys
import tempfile

_E2E_DIR = os.path.dirname(os.path.abspath(__file__))
_CELAB_VERSION_PATTERN = re.compile(
  r'name:\s*"infra/celab/celab/windows-amd64"[^>]*version:\s*"([^"]+)"'
)
_CEL_CTL_ARG_PATTERN = re.compile(r"--cel_ctl(?:=|\s+)([^\s\"']+)")


def _find_wheel_in_dir(directory: str) -> str | None:
  matches = sorted(glob.glob(os.path.join(directory, "chrome_ent_test-*.whl")))
  return matches[0] if matches else None


def _celab_package_root(cel_ctl_path: str) -> str:
  """Returns the CELab package root for a cel_ctl binary.

  Both the infra/celab/celab CIPD package and cel.zip are laid out as
  <root>/<platform>/bin/cel_ctl[.exe] with chrome_ent_test-*.whl in <root>.
  """
  bin_dir = os.path.dirname(cel_ctl_path)
  platform_dir = os.path.dirname(bin_dir)
  return os.path.dirname(platform_dir)


def _find_wheel_from_argv(argv: list[str]) -> str | None:
  joined_args = " ".join(argv)
  match = _CEL_CTL_ARG_PATTERN.search(joined_args)
  if not match:
    return None
  cel_ctl_path = os.path.abspath(os.path.expanduser(match.group(1)))
  return _find_wheel_in_dir(_celab_package_root(cel_ctl_path))


def _read_celab_version() -> str | None:
  spec_path = os.path.join(_E2E_DIR, "vpython.toml")
  if not os.path.exists(spec_path):
    return None
  with open(spec_path, encoding="utf-8") as spec_file:
    match = _CELAB_VERSION_PATTERN.search(spec_file.read())
    return match.group(1) if match else None


def _export_wheel_from_cipd(version: str) -> str | None:
  safe_version = re.sub(r"[^A-Za-z0-9._-]", "_", version)
  cache_dir = os.path.join(tempfile.gettempdir(), f"celab_wheel_{safe_version}")
  wheel_path = _find_wheel_in_dir(cache_dir)
  if wheel_path:
    return wheel_path

  cipd_bin = shutil.which("cipd") or "cipd"
  os.makedirs(cache_dir, exist_ok=True)
  subprocess.run(
    [
      cipd_bin,
      "export",
      "-log-level",
      "warning",
      "-root",
      cache_dir,
      "-ensure-file",
      "-",
    ],
    input=f"infra/celab/celab/windows-amd64 {version}\n",
    text=True,
    check=True,
    stdout=subprocess.DEVNULL,
  )
  return _find_wheel_in_dir(cache_dir)


def _add_wheel_to_path(wheel_path: str) -> None:
  if wheel_path not in sys.path:
    sys.path.insert(0, wheel_path)
  existing_pythonpath = os.environ.get("PYTHONPATH", "")
  pythonpath_entries = (
    existing_pythonpath.split(os.pathsep) if existing_pythonpath else []
  )
  if wheel_path not in pythonpath_entries:
    os.environ["PYTHONPATH"] = (
      f"{wheel_path}{os.pathsep}{existing_pythonpath}"
      if existing_pythonpath
      else wheel_path
    )


def ensure_chrome_ent_test() -> None:
  if importlib.util.find_spec("chrome_ent_test") is not None:
    return

  wheel_path = _find_wheel_from_argv(sys.argv[1:])
  if not wheel_path:
    version = _read_celab_version()
    if version:
      wheel_path = _export_wheel_from_cipd(version)

  if wheel_path:
    _add_wheel_to_path(wheel_path)


ensure_chrome_ent_test()
