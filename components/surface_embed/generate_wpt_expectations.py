#!/usr/bin/env vpython3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Generates FlagExpectations/surface-embed using ResultDB x wpt.fyi.

`surface_embed_chrome_wpt_tests` automatically inherits standard Chromium WPT
CI filtering at runtime via flags in `//infra/config/targets/tests.star`
(`--no-virtual-tests`, `--skip-failing-tests`, `--skip-timeouts`, and
`--inverted-test-launcher-filter-file=.../TestLists/content_shell.filter`),
along with subtest baselines (`*-expected.txt`).

For tests that still report non-PASS on `ci/linux-surface-embed-rel`, this
script combines `rdb` results with default-Chrome results from `wpt.fyi`:
  1. Tests that actually pass all subtests on `linux-surface-embed-rel` and
     only failed in historical ResultDB runs due to a now-removed
     `headless_shell` `[FAIL]` `*-expected.txt` baseline (or that should be
     moved to `TestLists/chrome.filter`) are excluded from skip sections.
  2. Tests that PASS in default Chrome on `wpt.fyi` yet fail on
     `linux-surface-embed-rel` represent candidate `SurfaceEmbed` / `Webium`
     issues (Sections 1 & 2). Existing entries are preserved across refreshes;
     newly introduced `SurfaceEmbed`-only failures are reported as warnings and
     are NOT added unless `--include-new-surface-embed-failures` is passed.
  3. Tests that ALSO fail or skip in default Chrome on `wpt.fyi` are written
     to Sections 4 & 5 (`Default Chrome discrepancies`, `[ Skip ]`).
"""

import argparse
from concurrent.futures import ThreadPoolExecutor
import datetime
import fnmatch
import gzip
import json
import os
import subprocess
import sys
import urllib.parse

BUILDER = "chromium/ci/linux-surface-embed-rel"
WPT_TEST_ID_REGEX = ".*chrome_wpt_tests.*"
WPT_FYI_RUNS_URL = (
    "https://wpt.fyi/api/runs?product=chrome&label=master&max-count=1"
)
GLOB_SKIPS = (
    # `headless_shell` mocks built-in AI APIs (`EchoAIManagerImpl`), whereas
    # Chrome uses `OptimizationGuide` without a downloaded model on CI bots.
    "external/wpt/ai/*",
)

SEC_CHROME_CONSISTENT = "chrome_consistent"
SEC_CHROME_FLAKY = "chrome_flaky"
SEC_SE_CONSISTENT = "se_consistent"
SEC_SE_FLAKY = "se_flaky"


def parse_resultdb_test_id(test_id: str) -> str:
    marker = "!webtest::"
    idx = test_id.find(marker)
    rest = test_id[idx + len(marker) :] if idx != -1 else test_id.lstrip(":")
    return rest.replace("#", "/", 1)


def fetch_wpt_fyi_non_passing() -> tuple[set[str], list[str]]:
    import requests

    print("Fetching latest default Chrome run from wpt.fyi...")
    runs = requests.get(
        WPT_FYI_RUNS_URL, headers={"User-Agent": "Mozilla/5.0"}, timeout=60
    ).json()
    if not runs:
        raise RuntimeError("No runs returned by wpt.fyi")

    results_url = runs[0]["results_url"]
    print(f"Downloading wpt.fyi summary: {results_url}")
    body = requests.get(
        results_url, headers={"User-Agent": "Mozilla/5.0"}, timeout=120
    ).content
    raw = gzip.decompress(body) if body.startswith(b"\x1f\x8b") else body
    summary = json.loads(raw.decode("utf-8"))

    exact: set[str] = set()
    globs: list[str] = []
    for test_path, result in summary.items():
        status = result.get("s")
        counts = result.get("c")
        non_pass = status in (
            "F",
            "C",
            "T",
            "S",
            "E",
            "FAIL",
            "CRASH",
            "TIMEOUT",
            "SKIP",
            "ERROR",
        ) or (counts is not None and counts[0] < counts[1])
        if not non_pass:
            continue
        norm = f"external/wpt{test_path}"
        if "*" in norm:
            globs.append(norm[: norm.find("*") + 1])
        else:
            exact.add(norm)
    return exact, globs


def fetch_build_results(build_id: str) -> dict[str, list[str]]:
    proc = subprocess.run(
        [
            "rdb",
            "query",
            "-n",
            "0",
            "-json",
            "-tr-fields",
            "testId,status,expected",
            "-test",
            WPT_TEST_ID_REGEX,
            f"build-{build_id}",
        ],
        capture_output=True,
        text=True,
        check=True,
    )
    per_test: dict[str, list[str]] = {}
    for line in proc.stdout.splitlines():
        tr = json.loads(line).get("testResult")
        if not tr:
            continue
        test_name = parse_resultdb_test_id(tr["testId"])
        per_test.setdefault(test_name, []).append(tr.get("status", ""))
    return per_test


def fetch_unexpected_non_passing(build_ids: list[str]) -> set[str]:
    proc = subprocess.run(
        [
            "rdb",
            "query",
            "-u",
            "-n",
            "0",
            "-json",
            "-tr-fields",
            "testId,status,expected",
            "-test",
            WPT_TEST_ID_REGEX,
            *[f"build-{bid}" for bid in build_ids],
        ],
        capture_output=True,
        text=True,
        check=True,
    )
    unexpected: set[str] = set()
    for line in proc.stdout.splitlines():
        tr = json.loads(line).get("testResult")
        if tr and tr.get("status") not in ("PASS", "SKIP"):
            unexpected.add(parse_resultdb_test_id(tr["testId"]))
    return unexpected


def fetch_actual_text_artifacts(
    build_id: str, candidate_tests: set[str]
) -> dict[str, list[str]]:
    import requests
    from requests.adapters import HTTPAdapter
    from urllib3.util.retry import Retry

    artifacts = []
    page_token = ""
    while True:
        payload: dict[str, object] = {
            "invocations": [f"invocations/build-{build_id}"],
            "predicate": {
                "testResultPredicate": {"testIdRegexp": WPT_TEST_ID_REGEX},
                "artifactIdRegexp": "actual_text",
            },
            "pageSize": 1000,
        }
        if page_token:
            payload["pageToken"] = page_token
        proc = subprocess.run(
            ["rdb", "rpc", "luci.resultdb.v1.ResultDB", "QueryArtifacts"],
            input=json.dumps(payload),
            capture_output=True,
            text=True,
            check=True,
        )
        resp = json.loads(proc.stdout)
        for art in resp.get("artifacts", []):
            raw_id = urllib.parse.unquote(art["name"].split("/results/", 1)[0])
            test_name = parse_resultdb_test_id(raw_id)
            if test_name in candidate_tests:
                artifacts.append((test_name, art["fetchUrl"]))
        page_token = resp.get("nextPageToken", "")
        if not page_token:
            break

    session = requests.Session()
    adapter = HTTPAdapter(
        pool_connections=16,
        pool_maxsize=16,
        max_retries=Retry(total=3, backoff_factor=0.3),
    )
    session.mount("https://", adapter)

    def download(item: tuple[str, str]) -> tuple[str, str]:
        test_name, url = item
        resp = session.get(url, timeout=30)
        resp.raise_for_status()
        return test_name, resp.content.decode("utf-8", errors="replace")

    per_test: dict[str, list[str]] = {}
    with ThreadPoolExecutor(max_workers=12) as pool:
        for test_name, text in pool.map(download, artifacts):
            per_test.setdefault(test_name, []).append(text)
    return per_test


def read_existing_sections(output_path: str) -> dict[str, set[str]]:
    sections: dict[str, set[str]] = {
        SEC_CHROME_CONSISTENT: set(),
        SEC_CHROME_FLAKY: set(),
        SEC_SE_CONSISTENT: set(),
        SEC_SE_FLAKY: set(),
    }
    if not os.path.exists(output_path):
        return sections
    current = SEC_SE_FLAKY
    with open(output_path, "r", encoding="utf-8") as f:
        for raw_line in f:
            line = raw_line.strip()
            if not line:
                continue
            if "Known SurfaceEmbed consistent failures" in line:
                current = SEC_SE_CONSISTENT
            elif "Known SurfaceEmbed / multi-process" in line:
                current = SEC_SE_FLAKY
            elif "Default Chrome consistent failures" in line:
                current = SEC_CHROME_CONSISTENT
            elif "Default Chrome flaky failures" in line:
                current = SEC_CHROME_FLAKY
            elif not line.startswith("#") and "[ Skip ]" in line:
                path = line.split("[", 1)[0].strip()
                if not path.endswith("*"):
                    sections[current].add(path)
    return sections


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--full-builds",
        type=int,
        default=5,
        help="Number of recent completed builds to query in full.",
    )
    parser.add_argument(
        "--unexpected-builds",
        type=int,
        default=20,
        help="Number of recent completed builds to scan for unexpected flakes.",
    )
    parser.add_argument(
        "--include-new-surface-embed-failures",
        action="store_true",
        help=(
            "Also skip newly detected failures that pass in default Chrome "
            "on wpt.fyi (otherwise only report them as warnings)."
        ),
    )
    args = parser.parse_args()

    script_dir = os.path.dirname(os.path.abspath(__file__))
    repo_root = os.path.abspath(os.path.join(script_dir, "..", ".."))
    sys.path.insert(0, os.path.join(repo_root, "third_party", "blink", "tools"))

    from blinkpy.common.host import Host
    from blinkpy.web_tests.models import testharness_results
    from blinkpy.web_tests.models.test_expectations import TestExpectations
    from blinkpy.web_tests.models.typ_types import ResultType

    output_path = os.path.join(
        repo_root,
        "third_party",
        "blink",
        "web_tests",
        "FlagExpectations",
        "surface-embed",
    )
    content_shell_filter_path = os.path.join(
        repo_root,
        "third_party",
        "blink",
        "web_tests",
        "TestLists",
        "content_shell.filter",
    )

    host = Host()
    std_port = host.port_factory.get("linux")
    std_port.set_option_default("manifest_update", False)
    std_exp = TestExpectations(std_port)

    with open(content_shell_filter_path, "r", encoding="utf-8") as f:
        content_shell_patterns = [
            line.strip()
            for line in f
            if line.strip() and not line.strip().startswith("#")
        ]

    wpt_exact, wpt_globs = fetch_wpt_fyi_non_passing()

    def fails_on_default_chrome(test_name: str) -> bool:
        return test_name in wpt_exact or any(
            fnmatch.fnmatchcase(test_name, g) for g in wpt_globs
        )

    max_builds = max(args.full_builds, args.unexpected_builds)
    bb_out = subprocess.check_output(
        [
            "bb",
            "ls",
            BUILDER,
            "-status",
            "ended",
            "-n",
            str(max_builds),
            "-json",
        ],
        text=True,
    )
    builds = [json.loads(line) for line in bb_out.strip().splitlines()]
    full_builds = builds[: args.full_builds]
    print(
        f"Querying {len(full_builds)} full builds from {BUILDER}: "
        f"{[b['number'] for b in full_builds]}"
    )

    with ThreadPoolExecutor(max_workers=len(full_builds)) as pool:
        per_build_results = list(
            pool.map(lambda b: fetch_build_results(b["id"]), full_builds)
        )

    unexpected_set = fetch_unexpected_non_passing(
        [b["id"] for b in builds[: args.unexpected_builds]]
    )

    statuses_across_builds: dict[str, list[str]] = {}
    for build_map in per_build_results:
        for test_name, statuses in build_map.items():
            non_skip = [s for s in statuses if s != "SKIP"]
            if non_skip:
                statuses_across_builds.setdefault(test_name, []).extend(
                    non_skip
                )

    consistent_failures = {
        test_name
        for test_name, statuses in statuses_across_builds.items()
        if "PASS" not in statuses
    }
    intermittent_failures = (
        {
            test_name
            for test_name, statuses in statuses_across_builds.items()
            if any(s != "PASS" for s in statuses)
        }
        | unexpected_set
    ) - consistent_failures

    def should_include(test_name: str) -> bool:
        if test_name.startswith("virtual/"):
            return False
        if any(fnmatch.fnmatchcase(test_name, g) for g in GLOB_SKIPS):
            return False
        if any(
            fnmatch.fnmatchcase(test_name, pat)
            for pat in content_shell_patterns
        ):
            return False
        if not std_port.test_exists(test_name):
            return False
        std_results = std_exp.get_expectations(test_name).results
        if std_results & {
            ResultType.Skip,
            ResultType.Failure,
            ResultType.Timeout,
        }:
            return False
        return True

    need_text_by_build: dict[str, set[str]] = {}
    for test_name in consistent_failures:
        if not should_include(test_name):
            continue
        for build, build_map in zip(full_builds, per_build_results):
            if any(s != "SKIP" for s in build_map.get(test_name, [])):
                need_text_by_build.setdefault(build["id"], set()).add(
                    test_name
                )
                break

    latest_texts: dict[str, list[str]] = {}
    for bid, candidates in need_text_by_build.items():
        latest_texts.update(fetch_actual_text_artifacts(bid, candidates))

    # Exclude tests that pass 100% of subtests on linux-surface-embed-rel
    # and only failed in historical runs due to a headless_shell [FAIL]
    # *-expected.txt baseline (they belong in TestLists/chrome.filter with
    # their obsolete *-expected.txt removed).
    all_pass_tests: set[str] = set()
    for test_name in sorted(consistent_failures):
        if not should_include(test_name):
            continue
        texts = latest_texts.get(test_name)
        if texts and all(
            testharness_results.is_all_pass_test_result(t) for t in texts
        ):
            all_pass_tests.add(test_name)

    existing = read_existing_sections(output_path)
    existing_all = set().union(*existing.values())

    consistent_candidates = {
        t
        for t in (
            consistent_failures
            | existing[SEC_CHROME_CONSISTENT]
            | existing[SEC_SE_CONSISTENT]
        )
        if should_include(t) and t not in all_pass_tests
    }
    chrome_consistent_out = sorted(
        t for t in consistent_candidates if fails_on_default_chrome(t)
    )
    raw_se_consistent = {
        t for t in consistent_candidates if not fails_on_default_chrome(t)
    }
    new_se_consistent = sorted(raw_se_consistent - existing_all)
    if args.include_new_surface_embed_failures:
        se_consistent_out = sorted(raw_se_consistent)
    else:
        se_consistent_out = sorted(raw_se_consistent & existing_all)

    already_consistent = set(chrome_consistent_out) | set(se_consistent_out)
    flaky_candidates = {
        t
        for t in (
            intermittent_failures
            | existing[SEC_CHROME_FLAKY]
            | existing[SEC_SE_FLAKY]
        )
        - already_consistent
        if should_include(t) and t not in all_pass_tests
    }
    chrome_flaky_out = sorted(
        t for t in flaky_candidates if fails_on_default_chrome(t)
    )
    raw_se_flaky = {
        t for t in flaky_candidates if not fails_on_default_chrome(t)
    }
    new_se_flaky = sorted(raw_se_flaky - existing_all)
    if args.include_new_surface_embed_failures:
        se_flaky_out = sorted(raw_se_flaky)
    else:
        se_flaky_out = sorted(raw_se_flaky & existing_all)

    today = datetime.date.today().isoformat()
    sep = "# " + "=" * 76
    subsep = "# " + "-" * 76
    lines = [
        "# tags: [ Linux ]",
        "# tags: [ Release Debug ]",
        "# results: [ Skip ]",
        "",
        sep,
        "# Expectations for `surface_embed_chrome_wpt_tests`",
        "# (`--product=chrome --flag-specific=surface-embed`).",
        "#",
        "# Standard CI expectations (`TestExpectations`, `NeverFixTests`,",
        "# `TestLists/content_shell.filter`, and `*-expected.txt` baselines)",
        "# are inherited automatically at runtime via flags in",
        "# `//infra/config/targets/tests.star`.",
        "#",
        "# Always use `[ Skip ]` here (never `[ Failure Pass ]`) so entries",
        "# never override `[ Skip ]` or `[ Timeout ]` rules in standard",
        "# `TestExpectations` / `NeverFixTests`.",
        "#",
        f"# Last refreshed: {today}",
        "# Refresh helper:",
        "#   vpython3 components/surface_embed/generate_wpt_expectations.py",
        sep,
        "",
        subsep,
        "# 1. Known SurfaceEmbed consistent failures (PASS in default Chrome",
        "#    on wpt.fyi, FAIL consistently on linux-surface-embed-rel)",
        "#    TODO(https://crbug.com/558819229): Fix and unskip these tests.",
        "#    [Not auto-updated: existing entries are preserved; new failures",
        "#    are only warned unless --include-new-surface-embed-failures]",
        subsep,
        *[f"{t} [ Skip ]" for t in se_consistent_out],
        "",
        subsep,
        "# 2. Known SurfaceEmbed / multi-process Chrome intermittent flakes",
        "#    (PASS on wpt.fyi, flaky on linux-surface-embed-rel)",
        "#    TODO(https://crbug.com/558819229): Triage and deflake these",
        "#    tests (see also https://crbug.com/575925048).",
        "#    [Not auto-updated: existing entries are preserved; new flakes",
        "#    are only warned unless --include-new-surface-embed-failures]",
        subsep,
        *[f"{t} [ Skip ]" for t in se_flaky_out],
        "",
        subsep,
        "# 3. Chrome vs. headless_shell embedder feature differences",
        "#    [Static globs maintained in GLOB_SKIPS; written on every run]",
        subsep,
        "# Built-in AI WPTs use a mock `EchoAIManagerImpl` in",
        "# `headless_shell`, whereas Chrome uses `OptimizationGuide` without",
        "# a downloaded model on CI bots.",
        *[f"{g} [ Skip ]" for g in GLOB_SKIPS],
        "",
        subsep,
        "# 4. Default Chrome consistent failures / skips (also non-passing on",
        "#    wpt.fyi default Chrome — not caused by SurfaceEmbed)",
        "#    [Auto-updated on every run of generate_wpt_expectations.py]",
        subsep,
        *[f"{t} [ Skip ]" for t in chrome_consistent_out],
        "",
        subsep,
        "# 5. Default Chrome flaky failures (also non-passing on wpt.fyi",
        "#    default Chrome — not caused by SurfaceEmbed)",
        "#    [Auto-updated on every run of generate_wpt_expectations.py]",
        subsep,
        *[f"{t} [ Skip ]" for t in chrome_flaky_out],
        "",
    ]

    with open(output_path, "w", encoding="utf-8") as f:
        f.write("\n".join(lines))

    print(
        f"Excluded {len(all_pass_tests)} all-PASS tests, and wrote "
        f"{len(GLOB_SKIPS)} globs, "
        f"{len(chrome_consistent_out)} default-Chrome consistent skips, "
        f"{len(chrome_flaky_out)} default-Chrome flaky skips, "
        f"{len(se_consistent_out)} SurfaceEmbed consistent skips, and "
        f"{len(se_flaky_out)} SurfaceEmbed flaky skips."
    )
    if not args.include_new_surface_embed_failures and (
        new_se_consistent or new_se_flaky
    ):
        print(
            f"\nWARNING: {len(new_se_consistent)} new consistent and "
            f"{len(new_se_flaky)} new flaky SurfaceEmbed-only failures were "
            "NOT added (pass --include-new-surface-embed-failures to include):"
        )
        for t in new_se_consistent:
            print(f"  [CONSISTENT] {t}")
        for t in new_se_flaky:
            print(f"  [FLAKY]      {t}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
