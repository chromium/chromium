# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Reports per-case results of the media perf scripts to the bots.

Without this, a failed shard only shows "shard #0 (failed)" and the cause is
buried in the swarming log. TestReporter makes each case (e.g.
"H264/1080p60fpsH264_boat_yt_sync.mp4") a test result that carries a one-line
failure reason:

  - Each result is posted to ResultDB through ResultSink as soon as it is
    known, so it survives a later crash or a killed task.
  - On exit, all results are written to the --isolated-script-test-output
    file in the JSON Test Results format, so the step summary lists the
    failed tests instead of "invalid results".

Usage:

    reporter = test_results.TestReporter.from_argv()
    with reporter:
        reporter.plan(case_names)
        with reporter.case('setup'):
            ...
        for name in case_names:
            with reporter.case(name):
                ...

An exception raised inside reporter.case() is recorded as that case's failure
and re-raised. An exception that reaches the `with reporter:` block without
having been recorded by a case is recorded as a failure of HARNESS_CASE.
Planned cases that never ran are reported as skips that name the failure that
stopped the run. They are unexpected in the JSON file. ResultSink cannot mark
a skip unexpected, so in ResultDB they carry a failure_kind tag instead, which
deliberate skips do not have.
"""

import argparse
import contextlib
import dataclasses
import enum
import html
import json
import logging
import pathlib
import sys
import time
from collections.abc import Callable, Iterable, Iterator

import requests

# Imported for its side effect: puts build/util (home of lib/) on sys.path.
import perf_config  # noqa: F401  # pylint: disable=unused-import
from lib.results import result_sink
from lib.results import result_types
from remote_transport import RemoteDeviceError

SETUP_CASE = 'setup'
HARNESS_CASE = 'harness'

# ResultDB truncates primaryErrorMessage to 1024 bytes. Keep the summary
# short enough to read in the step text and the test results tab.
_MAX_REASON_CHARS = 1000


class _StrEnum(str, enum.Enum):
    """A string enum whose members print as their value, like enum.StrEnum.

    enum.StrEnum is only available in Python 3.11 and later.
    """

    def __str__(self) -> str:
        return self.value


class Status(_StrEnum):
    """A case outcome, spelled as in the JSON Test Results format."""

    PASS = 'PASS'
    FAIL = 'FAIL'
    SKIP = 'SKIP'


class FailureKind(_StrEnum):
    """Who should look at a failure: the lab (infra) or the code (test)."""

    INFRA = 'infra'
    TEST = 'test'


_RESULT_SINK_STATUS = {
    Status.PASS: result_types.PASS,
    Status.FAIL: result_types.FAIL,
    Status.SKIP: result_types.SKIP,
}


def _exception_chain(exc: BaseException) -> Iterator[BaseException]:
    """Yields `exc`, then each exception it was raised from or during."""
    seen = set()
    while exc is not None and exc not in seen:
        seen.add(exc)
        yield exc
        exc = exc.__cause__ or (
            None if exc.__suppress_context__ else exc.__context__
        )


def failure_kind(exc: BaseException) -> FailureKind:
    """Returns whether `exc` is an infra failure or a test failure.

    A RemoteDeviceError anywhere in the chain is an infra failure, as is an
    interruption (a BaseException that is not an Exception, e.g.
    KeyboardInterrupt).
    """
    if not isinstance(exc, Exception) or any(
        isinstance(e, RemoteDeviceError) for e in _exception_chain(exc)
    ):
        return FailureKind.INFRA
    return FailureKind.TEST


def _summarize(exc: BaseException) -> str:
    message = str(exc).strip() or repr(exc)
    return f'{type(exc).__name__}: {message}'


def describe_failure(exc: BaseException) -> str:
    """Returns a short, human-readable root cause for `exc`.

    If `exc` was raised from another exception, the innermost one is named
    too, since that is usually the actual cause.
    """
    reason = _summarize(exc)
    *_, root = _exception_chain(exc)
    if root is not exc:
        reason += f' (caused by {_summarize(root)})'
    if len(reason) > _MAX_REASON_CHARS:
        reason = reason[: _MAX_REASON_CHARS - 3] + '...'
    return reason


@dataclasses.dataclass(frozen=True)
class CaseResult:
    """The outcome of one test case.

    `kind` is set for failures, and for skips caused by a failure. A skip
    without a kind is a deliberate one, e.g. an unsupported codec.
    """

    name: str
    status: Status
    duration_secs: float = 0.0
    reason: str | None = None
    kind: FailureKind | None = None

    @property
    def expected(self) -> bool:
        return self.status == Status.PASS or (
            self.status == Status.SKIP and self.kind is None
        )


@dataclasses.dataclass
class Case:
    """Handle yielded by TestReporter.case()."""

    skip_reason: str | None = None

    def skip(self, reason: str) -> None:
        """Reports the case as deliberately skipped instead of passed."""
        self.skip_reason = reason


class TestReporter(contextlib.AbstractContextManager):
    """Collects case results and reports them to ResultSink and JSON."""

    def __init__(
        self,
        output_path: str | None = None,
        sink_client: result_sink.ResultSinkClient | None = None,
        clock: Callable[[], float] = time.time,
    ):
        """
        Args:
            output_path: Where to write the JSON Test Results, or None.
            sink_client: Where to post results, or None outside LUCI.
            clock: Returns the current time in seconds.
        """
        self._output_path = output_path
        self._sink = sink_client
        self._clock = clock
        self._start_time = clock()
        # dicts rather than sets, to keep the reporting order.
        self._planned: dict[str, None] = {}
        self._results: dict[str, CaseResult] = {}
        self._first_failure: CaseResult | None = None
        # Exceptions already attributed to a case, so the harness does not
        # report them again when they propagate out of main().
        self._recorded_errors: set[BaseException] = set()

    @classmethod
    def from_argv(cls, argv: list[str] | None = None) -> 'TestReporter':
        """Builds a reporter from the isolated script command line."""
        parser = argparse.ArgumentParser(add_help=False)
        parser.add_argument('--isolated-script-test-output')
        args, _ = parser.parse_known_args(
            sys.argv[1:] if argv is None else argv
        )
        return cls(
            output_path=args.isolated_script_test_output,
            sink_client=result_sink.TryInitClient(),
        )

    @property
    def results(self) -> list[CaseResult]:
        """The recorded results, in the order they were reported."""
        return list(self._results.values())

    @property
    def first_failure(self) -> CaseResult | None:
        """The first failed CaseResult, or None."""
        return self._first_failure

    def plan(self, names: Iterable[str]) -> None:
        """Declares cases that should run, so missing ones are reported."""
        self._planned.update(dict.fromkeys(names))

    @contextlib.contextmanager
    def case(self, name: str) -> Iterator[Case]:
        """Runs one case, recording PASS, SKIP, or FAIL with the raised error.

        Yields a Case whose skip() marks the case as deliberately skipped.
        Anything raised, including KeyboardInterrupt, fails the case and is
        re-raised.

        Raises:
            ValueError: `name` was already reported or conflicts with another
                case name.
        """
        self._check_new_name(name)
        start = self._clock()
        handle = Case()
        try:
            yield handle
        except BaseException as e:
            self.fail(name, e, self._clock() - start)
            raise
        if handle.skip_reason is not None:
            self.skip(name, handle.skip_reason)
        else:
            self._record(CaseResult(name, Status.PASS, self._clock() - start))

    def fail(
        self, name: str, exc: BaseException, duration_secs: float = 0.0
    ) -> None:
        """Records `exc` as the failure of case `name`."""
        self._recorded_errors.add(exc)
        self._record(
            CaseResult(
                name,
                Status.FAIL,
                duration_secs,
                reason=describe_failure(exc),
                kind=failure_kind(exc),
            )
        )

    def skip(self, name: str, reason: str) -> None:
        """Records a deliberate, expected skip of case `name`."""
        self._record(CaseResult(name, Status.SKIP, reason=reason))

    def __exit__(self, exc_type, exc_value, traceback):
        if exc_value is not None and exc_value not in self._recorded_errors:
            self.fail(HARNESS_CASE, exc_value)
        self._report_not_run()
        self._write_json(interrupted=exc_value is not None)
        return False

    def _report_not_run(self) -> None:
        if failure := self._first_failure:
            reason = f'Not run: {failure.name} failed: {failure.reason}'
            kind = failure.kind
        else:
            reason = 'Not run.'
            kind = FailureKind.TEST
        not_run = [name for name in self._planned if name not in self._results]
        for name in not_run:
            self._record(
                CaseResult(name, Status.SKIP, reason=reason, kind=kind)
            )

    def _check_new_name(self, name: str) -> None:
        """Raises ValueError unless `name` can be added to the results.

        Names are '/'-separated paths in the JSON results trie, so a name
        cannot be reported twice or be a path prefix of another name.
        """
        if name in self._results:
            raise ValueError(f'Case {name!r} was already reported.')
        if '' in name.split('/'):
            raise ValueError(f'Case name {name!r} has an empty path component.')
        for other in self._results:
            if other.startswith(f'{name}/') or name.startswith(f'{other}/'):
                raise ValueError(
                    f'Case names {name!r} and {other!r} conflict: one is a '
                    'path prefix of the other.'
                )

    def _record(self, result: CaseResult) -> None:
        self._check_new_name(result.name)
        self._results[result.name] = result
        if result.status == Status.FAIL:
            if self._first_failure is None:
                self._first_failure = result
            logging.error(
                'FAILED %s [%s]: %s', result.name, result.kind, result.reason
            )
        else:
            logging.info('%s %s', result.status, result.name)
        self._post(result)

    def _post(self, result: CaseResult) -> None:
        if not self._sink:
            return
        tags = [('failure_kind', result.kind)] if result.kind else []
        is_failure = result.status == Status.FAIL
        # Logged rather than raised: losing a result must not mask the real
        # failure or skip the remaining cases and teardown.
        try:
            self._sink.Post(
                result.name,
                _RESULT_SINK_STATUS[result.status],
                int(result.duration_secs * 1000),
                None,  # test_log
                None,  # test_file
                test_id_structured={
                    'coarseName': None,
                    'fineName': None,
                    'caseNameComponents': result.name.split('/'),
                },
                failure_reason=result.reason if is_failure else None,
                html_artifact=(
                    html.escape(result.reason)
                    if result.reason and not is_failure
                    else None
                ),
                tags=tags,
            )
        except (requests.RequestException, OSError):
            logging.exception('Failed to report %s to ResultSink.', result.name)

    def to_json(self, interrupted: bool = False) -> dict:
        """Returns the results in the JSON Test Results format (version 3)."""
        tests = {}
        counts = dict.fromkeys(Status, 0)
        for result in self._results.values():
            counts[result.status] += 1
            *parents, leaf = result.name.split('/')
            node = tests
            for part in parents:
                node = node.setdefault(part, {})
            entry = {
                'expected': result.status if result.expected else Status.PASS,
                'actual': result.status,
                'time': round(result.duration_secs, 3),
            }
            if not result.expected:
                entry['is_unexpected'] = True
            if result.reason:
                is_failure = result.status == Status.FAIL
                key = 'failure_reason' if is_failure else 'skip_reason'
                entry[key] = result.reason
            if result.kind:
                entry['failure_kind'] = result.kind
            node[leaf] = entry
        return {
            'version': 3,
            'interrupted': interrupted,
            'path_delimiter': '/',
            'seconds_since_epoch': self._start_time,
            'num_failures_by_type': counts,
            'tests': tests,
        }

    def _write_json(self, interrupted: bool) -> None:
        if not self._output_path:
            return
        path = pathlib.Path(self._output_path)
        try:
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(
                json.dumps(self.to_json(interrupted), indent=2),
                encoding='utf-8',
            )
            logging.info('Wrote test results to %s', path)
        except OSError:
            logging.exception('Failed to write test results to %s', path)
