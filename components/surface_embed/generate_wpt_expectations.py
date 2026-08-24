# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Generates FlagExpectations/surface-embed from wpt.fyi.

This script generates the third_party/blink/web_tests/FlagExpectations/surface-embed
file, which filters out discrepancies between content_shell and real Chrome on
the linux-surface-embed-rel test bot:

1. Virtual test suites: Skipped entirely because they are not run on wpt.live / wpt.fyi.
2. Tests that fail on default Chrome (from wpt.fyi): These are pre-existing
   Chrome failures not caused by surface-embed.
3. Tests that are skipped on default Chrome (from wpt.fyi): Tests disabled via
   WPT metadata (.ini) or unsupported on Chrome.
4. Tests that pass on default Chrome (from wpt.fyi) but have content_shell
   *-expected.txt failure baselines checked into Chromium: Because
   linux-surface-embed-rel runs real Chrome, these tests pass; adding them to
   FlagExpectations allows them to pass without conflicting with the
   content_shell baseline.
"""

import datetime
import gzip
import json
import os
import re
import urllib.request


def is_failing_on_wpt(result):
    """Returns a set of failure statuses if the wpt.fyi result represents a failure/crash/timeout."""
    statuses = set()
    status = result.get("s")
    if status in ("F", "C", "T", "TIMEOUT", "FAIL", "CRASH", "ERROR", "E"):
        statuses.add(status)
    if "c" in result:
        passes, total = result["c"]
        if passes < total:
            statuses.add("FAIL")
    return statuses


def is_skipped_on_wpt(result):
    """Returns True if the test was skipped on wpt.fyi."""
    status = result.get("s")
    return status in ("S", "SKIP")


def is_passing_on_wpt(result):
    """Returns True if the test ran and passed completely on wpt.fyi."""
    return not is_skipped_on_wpt(result) and not is_failing_on_wpt(result)


def get_candidate_baseline_paths(wpt_root, test_path):
    """Returns possible *-expected.txt filepaths in Blink for a given WPT test path."""
    base = test_path.lstrip("/")
    candidates = []

    idx = base.find("?")
    if idx != -1:
        clean_path = base[:idx]
        query_part = base[idx:]
        sanitized_query = re.sub(r"[^A-Za-z0-9_.-]", "_", query_part)
    else:
        clean_path = base
        sanitized_query = None

    stem, _ = os.path.splitext(clean_path)
    stems = [stem]
    if ".any." in clean_path:
        any_stem = clean_path[: clean_path.find(".any.") + 4]
        if any_stem not in stems:
            stems.append(any_stem)

    for s in stems:
        candidates.append(os.path.join(wpt_root, f"{s}-expected.txt"))
        if sanitized_query:
            candidates.append(
                os.path.join(wpt_root, f"{s}{sanitized_query}-expected.txt")
            )

    if not sanitized_query:
        candidates.append(os.path.join(wpt_root, f"{stem}.html-expected.txt"))

    return candidates


def has_failure_baseline(wpt_root, test_path):
    """Returns True if a checked-in *-expected.txt exists and expects failure."""
    for path in get_candidate_baseline_paths(wpt_root, test_path):
        if os.path.exists(path):
            try:
                with open(path, "r", encoding="utf-8", errors="replace") as f:
                    content = f.read()
                if any(
                    kw in content
                    for kw in ("FAIL", "TIMEOUT", "NOTRUN", "Harness Error")
                ):
                    return True
            except OSError:
                pass
    return False


def main():
    script_dir = os.path.dirname(os.path.abspath(__file__))
    repo_root = os.path.abspath(os.path.join(script_dir, "..", ".."))
    wpt_root = os.path.join(
        repo_root, "third_party", "blink", "web_tests", "external", "wpt"
    )
    output_path = os.path.join(
        repo_root,
        "third_party",
        "blink",
        "web_tests",
        "FlagExpectations",
        "surface-embed",
    )

    print("Fetching run details from wpt.fyi...")
    runs_url = "https://wpt.fyi/api/runs?product=chrome&label=master&max-count=1"
    req = urllib.request.Request(
        runs_url, headers={"User-Agent": "Mozilla/5.0"}
    )
    with urllib.request.urlopen(req) as response:
        runs_data = json.loads(response.read().decode("utf-8"))

    if not runs_data:
        print("No runs found")
        return

    run = runs_data[0]
    results_url = run["results_url"]
    print(f"Latest Chrome run results URL: {results_url}")

    print("Downloading results summary...")
    req_summary = urllib.request.Request(
        results_url, headers={"User-Agent": "Mozilla/5.0"}
    )
    with urllib.request.urlopen(req_summary) as response_summary:
        body = response_summary.read()
        if body.startswith(b"\x1f\x8b"):
            raw_data = gzip.decompress(body)
        else:
            raw_data = body
        summary = json.loads(raw_data.decode("utf-8"))

    print(f"Total test files in summary: {len(summary)}")

    chrome_failures = {}
    chrome_skips = set()
    content_shell_baseline_overrides = set()

    for test_path, result in summary.items():
        normalized_path = f"external/wpt{test_path}"
        if "*" in normalized_path:
            idx = normalized_path.find("*")
            normalized_path = normalized_path[: idx + 1]

        failing_statuses = is_failing_on_wpt(result)
        if is_skipped_on_wpt(result):
            chrome_skips.add(normalized_path)
        elif failing_statuses:
            chrome_failures.setdefault(normalized_path, set()).update(
                failing_statuses
            )
        elif has_failure_baseline(wpt_root, test_path):
            content_shell_baseline_overrides.add(normalized_path)

    print(
        f"Found {len(chrome_failures)} failing test files in default Chrome"
        " (Section 1)."
    )
    print(
        f"Found {len(chrome_skips)} skipped test files in default Chrome"
        " (Section 2)."
    )
    print(
        f"Found {len(content_shell_baseline_overrides)} tests passing in Chrome"
        " with content_shell baselines (Section 3)."
    )

    today = datetime.datetime.now().strftime("%Y-%m-%d")
    header = f"""# tags: [ Linux ]
# tags: [ Release Debug ]
# results: [ Timeout Crash Pass Failure Slow Skip ]

# ==============================================================================
# Automatically generated expectations for linux-surface-embed-rel.
#
# Generated by components/surface_embed/generate_wpt_expectations.py.
#
# This file filters out discrepancies caused by using real Chrome instead of
# content_shell / headless_shell:
#
# 1. Virtual test suites:
#    Virtual test suites are not run on wpt.live / wpt.fyi and are skipped here.
#
# 2. Section 1 (Pre-existing failures on default Chrome from wpt.fyi):
#    Tests that fail in default Chrome (on wpt.fyi / wpt.live). We allow
#    [ Failure Pass ] (or Crash/Timeout) so pre-existing Chrome bugs do not
#    fail the linux-surface-embed-rel bot.
#
# 3. Section 2 (Tests skipped on default Chrome from wpt.fyi):
#    Tests that are skipped on default Chrome on wpt.fyi / wpt.live (e.g. disabled
#    via WPT metadata .ini files or unsupported features). We skip them here too.
#
# 4. Section 3 (Tests passing in default Chrome with content_shell baselines):
#    Tests that fail in content_shell have *-expected.txt baselines checked in
#    recording FAIL subtests. However, these tests pass in real Chrome and on
#    wpt.live (e.g. cross-origin/iframe focus tests). When real Chrome runs on
#    linux-surface-embed-rel, all subtests pass, which mismatches the
#    content_shell baseline and is treated as an unexpected failure. We add
#    [ Failure Pass ] to allow them to pass without conflicting with the
#    baseline.
#
# Last update: {today}
#
# To update, run:
#   vpython3 components/surface_embed/generate_wpt_expectations.py
# ==============================================================================

# Skip all virtual test suites because they are not run on wpt.live / wpt.fyi.
virtual/* [ Skip ]

# ------------------------------------------------------------------------------
# Section 1: Pre-existing failures on default Chrome (from wpt.fyi)
# ------------------------------------------------------------------------------
"""

    section2_header = """
# ------------------------------------------------------------------------------
# Section 2: Tests skipped on default Chrome (from wpt.fyi)
# ------------------------------------------------------------------------------
"""

    section3_header = """
# ------------------------------------------------------------------------------
# Section 3: Tests passing in default Chrome with content_shell baselines
#            (overriding *-expected.txt)
# ------------------------------------------------------------------------------
"""

    with open(output_path, "w", encoding="utf-8") as f:
        f.write(header)
        for path, statuses in sorted(chrome_failures.items()):
            exps = set()
            for status in statuses:
                if status in ("C", "CRASH"):
                    exps.add("Crash")
                elif status in ("T", "TIMEOUT"):
                    exps.add("Timeout")
                else:
                    exps.add("Failure")
            # Add "Pass" expectations so that flaky tests won't show up as "pass
            # unexpectedly" in the test results.
            exps.add("Pass")
            exp_str = " ".join(sorted(list(exps)))
            f.write(f"{path} [ {exp_str} ]\n")

        f.write(section2_header)
        for path in sorted(chrome_skips):
            f.write(f"{path} [ Skip ]\n")

        f.write(section3_header)
        for path in sorted(content_shell_baseline_overrides):
            f.write(f"{path} [ Failure Pass ]\n")

    print(f"Written expectations to {output_path}")


if __name__ == "__main__":
    main()
