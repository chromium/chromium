#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Extracts APC (AnnotatedPageContent) and screenshots from an Actor trace.

This script parses a Perfetto trace recorded by Chrome during Actor tasks and
extracts:
  - PageContext event (contains serialized AnnotatedPageContent protobuf)
    -> step{N}_apc.textproto (or step{N}_apc.pb if protoc is unavailable)
  - Screenshot event (contains JPEG or PNG screenshot)
    -> step{N}_screenshot.jpg (or .png)

Recording Actor Traces:
  Pass the `--actor-trace-path=<path>` switch to Chrome to record traces to a
  file or directory.

  Android:
    1. Set command-line flags:
       out_android/Release/bin/chrome_apk argv set \
           --args="--actor-trace-path=/sdcard/Download/actor_trace.pb"
       (or via adb shell "echo '_ \
           --actor-trace-path=/sdcard/Download/actor_trace.pb' \
           > /data/local/tmp/chrome-command-line")
    2. Launch Chrome and run Actor tasks:
       out_android/Release/bin/chrome_apk launch
    3. Pull the trace file from device:
       adb pull /sdcard/Download/actor_trace.pb /tmp/actor_trace.pb

  Desktop (Linux / Mac / Windows):
    Run Chrome with the flag:
      chrome --actor-trace-path=/tmp/actor_trace.pb

Parsing and Extracting Steps:
  # Automatically decodes to .textproto when protoc or -b is provided:
  python3 components/actor/tools/parse_actor_trace.py \
      /tmp/actor_trace.pb -b out_linux/Release

  # Custom output directory is optional (defaults to <trace>_extracted):
  python3 components/actor/tools/parse_actor_trace.py \
      /tmp/actor_trace.pb -o /tmp/extracted_steps -b out_linux/Release

Manual Protoc Inspection (if raw .pb files were saved):
  protoc --decode=optimization_guide.proto.AnnotatedPageContent \
      components/optimization_guide/proto/features/common_quality_data.proto \
      < /tmp/actor_trace_extracted/step1_apc.pb
"""

import argparse
import base64
import binascii
import os
import pathlib
import shutil
import subprocess
import sys
from typing import Any, Dict, List, Optional, Tuple, Union

# Add third_party/perfetto/python and third_party/protobuf/python to sys.path
_SCRIPT_DIR = pathlib.Path(__file__).resolve().parent
_SRC_DIR = _SCRIPT_DIR.parents[2]
for p in [
    _SRC_DIR / "third_party" / "perfetto" / "python",
    _SRC_DIR / "third_party" / "protobuf" / "python",
]:
  if p.is_dir() and str(p) not in sys.path:
    sys.path.insert(0, str(p))

from perfetto.trace_processor import TraceProcessor  # pylint: disable=wrong-import-position

# Standard file signature magic bytes:
# - JPEG begins with the Start of Image (SOI) marker (0xFF, 0xD8) followed by
#   a marker prefix byte (0xFF).
# - PNG begins with the 4-byte signature: 0x89, 'P', 'N', 'G'
#   (0x89, 0x50, 0x4E, 0x47).
JPEG_MAGIC = bytes([0xFF, 0xD8, 0xFF])
PNG_MAGIC = bytes([0x89, 0x50, 0x4E, 0x47])


def extract_raw_payload(data: Union[bytes, str]) -> bytes:
  """Returns binary data, decoding base64 if needed."""
  if not data:
    return b"" if isinstance(data, (bytes, str)) else data
  # Only attempt base64 decoding if the input is a string (e.g. from
  # TraceProcessor query results).
  if isinstance(data, str):
    try:
      return base64.b64decode(data, validate=True)
    except (binascii.Error, ValueError):
      return data.encode("utf-8")
  return data


def parse_actor_trace(trace_path: str) -> List[Dict[str, Any]]:
  """Parses trace using Perfetto TraceProcessor."""
  trace_file = pathlib.Path(trace_path)
  if not trace_file.is_file():
    raise FileNotFoundError(f"Trace file not found: {trace_path}")

  tp = TraceProcessor(trace=str(trace_file.resolve()))
  query = """
    SELECT
      slice.parent_id AS parent_id,
      slice.name AS name,
      slice.ts AS ts,
      args.string_value AS data
    FROM slice
    JOIN args ON slice.arg_set_id = args.arg_set_id
    WHERE slice.name IN ('PageContext', 'Screenshot')
      AND args.key = 'screenshot.jpg_image'
    ORDER BY slice.ts ASC
  """
  qr = tp.query(query)
  events = []
  for row in qr:
    if row.data:
      events.append({
          "parent_id": row.parent_id,
          "timestamp": row.ts,
          "name": row.name,
          "data": extract_raw_payload(row.data),
      })
  tp.close()
  return events


def find_or_build_protoc(
    build_dir: Optional[str] = None,
) -> Optional[pathlib.Path]:
  """Locates or builds the protoc binary from the specified build directory."""
  if not build_dir:
    return None

  protoc_name = "protoc.exe" if sys.platform == "win32" else "protoc"
  build_path = pathlib.Path(build_dir).resolve()
  protoc_path = build_path / protoc_name
  if protoc_path.is_file() and os.access(protoc_path, os.X_OK):
    return protoc_path

  # Try building protoc using autoninja/ninja if not found
  print(f"protoc not found in {build_path}, attempting to build...")
  build_cmd = None
  if shutil.which("autoninja"):
    build_cmd = ["autoninja", "-C", str(build_path), "protoc"]
  elif shutil.which("ninja"):
    build_cmd = ["ninja", "-C", str(build_path), "protoc"]

  if build_cmd:
    try:
      subprocess.run(build_cmd, check=True, cwd=str(_SRC_DIR))
      if protoc_path.is_file():
        print(f"Successfully built protoc at: {protoc_path}")
        return protoc_path
    except Exception as e:
      print(f"Failed to build protoc: {e}")
  return None


def decode_apc_with_protoc(
    protoc_bin: pathlib.Path, pb_bytes: bytes
) -> Optional[str]:
  """Decodes AnnotatedPageContent proto bytes into textproto using protoc."""
  proto_file = (
      _SRC_DIR
      / "components"
      / "optimization_guide"
      / "proto"
      / "features"
      / "common_quality_data.proto"
  )
  if not proto_file.is_file():
    return None

  cmd = [
      str(protoc_bin),
      f"--proto_path={_SRC_DIR}",
      "--decode=optimization_guide.proto.AnnotatedPageContent",
      str(proto_file),
  ]
  try:
    res = subprocess.run(
        cmd,
        input=pb_bytes,
        capture_output=True,
        cwd=str(_SRC_DIR),
        timeout=10,
    )
    if res.returncode == 0:
      return res.stdout.decode("utf-8", errors="replace")
  except Exception:
    pass
  return None


def extract_trace_to_dir(trace_path: str,
                         output_dir: Optional[str] = None,
                         build_dir: Optional[str] = None) -> str:
  """Extracts step APC and Screenshot files from trace into output_dir."""
  trace_file = pathlib.Path(trace_path)
  if not trace_file.is_file():
    raise FileNotFoundError(f"Trace file not found: {trace_path}")

  if not output_dir:
    output_dir = str(trace_file.parent / f"{trace_file.stem}_extracted")

  out_path = pathlib.Path(output_dir)
  out_path.mkdir(parents=True, exist_ok=True)

  events = parse_actor_trace(str(trace_file.resolve()))
  # Sort events by timestamp
  events.sort(key=lambda x: x["timestamp"])

  protoc_bin = find_or_build_protoc(build_dir)
  if protoc_bin:
    print(f"Using protoc binary: {protoc_bin}")

  # Pair PageContext and Screenshot events that share the same parent slice
  # (e.g. RequestTabObservation). Fallback to sequential pairing if unparented.
  steps: List[Dict[str, bytes]] = []
  events_by_parent: Dict[Any, Dict[str, bytes]] = {}
  unparented_apc: List[bytes] = []
  unparented_screenshot: List[bytes] = []

  for e in events:
    pid = e.get("parent_id")
    if pid is not None:
      if pid not in events_by_parent:
        events_by_parent[pid] = {}
        steps.append(events_by_parent[pid])
      events_by_parent[pid][e["name"]] = e["data"]
    elif e["name"] == "PageContext":
      unparented_apc.append(e["data"])
    elif e["name"] == "Screenshot":
      unparented_screenshot.append(e["data"])

  if unparented_apc or unparented_screenshot:
    if steps:
      print(
          "Warning: Found unparented PageContext/Screenshot events alongside"
          " parented events. Appending them as additional steps."
      )
    total = max(len(unparented_apc), len(unparented_screenshot))
    for i in range(total):
      step: Dict[str, bytes] = {}
      if i < len(unparented_apc):
        step["PageContext"] = unparented_apc[i]
      if i < len(unparented_screenshot):
        step["Screenshot"] = unparented_screenshot[i]
      steps.append(step)

  print(f"Extracting {len(steps)} step(s) to: {out_path.resolve()}\n")

  for i, step in enumerate(steps, 1):
    step_prefix = f"step{i}"

    # Extract APC
    if "PageContext" in step:
      apc_data = step["PageContext"]
      text_content = None
      if protoc_bin:
        text_content = decode_apc_with_protoc(protoc_bin, apc_data)

      if text_content:
        apc_txt_path = out_path / f"{step_prefix}_apc.textproto"
        with open(apc_txt_path, "w", encoding="utf-8") as f:
          f.write(text_content)
        print(f"  [+] Wrote APC textproto: {apc_txt_path.name}")
      else:
        apc_pb_path = out_path / f"{step_prefix}_apc.pb"
        with open(apc_pb_path, "wb") as f:
          f.write(apc_data)
        print(
            f"  [+] Wrote APC proto:     {apc_pb_path.name} "
            f"({len(apc_data)} bytes)"
        )

    # Extract Screenshot
    if "Screenshot" in step:
      screenshot_data = step["Screenshot"]
      ext = ".png" if screenshot_data.startswith(PNG_MAGIC) else ".jpg"
      screenshot_path = out_path / f"{step_prefix}_screenshot{ext}"
      with open(screenshot_path, "wb") as f:
        f.write(screenshot_data)
      print(f"  [+] Wrote Screenshot:    {screenshot_path.name} "
            f"({len(screenshot_data)} bytes)")

  print(f"\nExtraction complete! Files saved in: {out_path.resolve()}\n")

  if not protoc_bin:
    print("=" * 70)
    print(
        "To inspect and format the AnnotatedPageContent .pb file using protoc:"
    )
    print("=" * 70)
    sample_pb = out_path / "step1_apc.pb"
    print(
        "  protoc --decode=optimization_guide.proto.AnnotatedPageContent \\\n"
        "      components/optimization_guide/proto/features/"
        "common_quality_data.proto \\\n"
        f"      < {sample_pb}"
    )
    print("=" * 70)

  return str(out_path.resolve())


def main():
  parser = argparse.ArgumentParser(description=(
      "Extract PageContext APC and Screenshots from Actor Perfetto Traces."))
  parser.add_argument("trace_path",
                      help="Path to actor trace file (.pb or .trace)")
  parser.add_argument(
      "-o",
      "--output_dir",
      default=None,
      help=("Directory to save extracted files (default:"
            " <trace_name>_extracted)"),
  )
  parser.add_argument(
      "-b",
      "--build_dir",
      default=None,
      help=(
          "Path to local OS build directory (e.g. out_linux/Release) "
          "containing or to build protoc for textproto extraction"
      ),
  )
  args = parser.parse_args()

  extract_trace_to_dir(args.trace_path, args.output_dir, args.build_dir)


if __name__ == "__main__":
  main()
