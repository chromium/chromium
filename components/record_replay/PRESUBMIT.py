# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

PRESUBMIT_VERSION = '2.0.0' # Finds Check* methods and executes them.
USE_PYTHON3 = True # Mandatory: run under Python 3

def CheckSubfolderPresubmits(inp, out):
  results = []
  for root, _, files in inp.os_walk(inp.PresubmitLocalPath()):
    if root == inp.PresubmitLocalPath():
      continue

    if 'PRESUBMIT.py' in files:
      results.extend(_ExecuteExtendedRootChecks(
          inp, out, inp.os_path.join(root, 'PRESUBMIT.py')))

  return results

def _ExecuteExtendedRootChecks(inp, out, path):
  import importlib.util
  rel_path = inp.os_path.relpath(path, inp.PresubmitLocalPath())
  module_name = "ps_" + rel_path.replace(inp.os_path.sep, '_').replace('.', '_')

  spec = importlib.util.spec_from_file_location(module_name, path)
  if spec and spec.loader:
    try:
      module = importlib.util.module_from_spec(spec)
      spec.loader.exec_module(module)
      if hasattr(module, 'ExtendedRootChecks'):
        return module.ExtendedRootChecks(inp, out)
    except Exception as e:
      return [out.PresubmitError(f"Failed to load presubmit {path}: {e}")]
  return []
