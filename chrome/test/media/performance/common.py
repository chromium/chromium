# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""A common module for media performance tests.

The implementation lives in smaller modules:
  perf_config       Paths, ports, and output directories.
  remote_transport  Running commands on the sender (SSH or local) and the
                    sender connectivity preflight.
  senders           Per-OS sender behavior; use make_sender(args).
  cros_setup        Starting Chrome on ChromeOS through Crossbench.
  media_metrics     PSNR/SSIM, resource monitoring metrics, result upload.

This module re-exports what the test scripts use and keeps thin
args-based wrappers for the teardown and monitoring helpers.
"""

import logging
import multiprocessing
import subprocess

from contextlib import AbstractContextManager

# pylint: disable=import-error, wrong-import-position, unused-import
# Imported first: puts build/util, fuchsia_web/av_testing, and
# build/fuchsia/test on sys.path for the imports below.
import perf_config  # noqa: F401
from lib.proto import measures  # noqa: F401
import server  # noqa: F401
import video_analyzer  # noqa: F401
import camera  # noqa: F401
from repeating_log import RepeatingLog  # noqa: F401

from cros_setup import setup_cros_environment  # noqa: F401
from media_metrics import (
    calculate_psnr_ssim,  # noqa: F401
    finalize_results,  # noqa: F401
    parse_glances_csv_and_record,  # noqa: F401
)
from perf_config import (
    BUILD_UTIL_ROOT,  # noqa: F401
    CHROME_FUCHSIA_ROOT,  # noqa: F401
    CFT_JSON_URL,  # noqa: F401
    CHROMEDRIVER_PORT,  # noqa: F401
    LOCAL_HOST_IP,  # noqa: F401
    RECORDINGS_DIR,  # noqa: F401
    REMOTE_URL,  # noqa: F401
    REPO_ROOT,  # noqa: F401
    SERVER_PORT,
    TEST_SCRIPTS_ROOT,  # noqa: F401
    TRACES_DIR,  # noqa: F401
)
from remote_transport import (
    RemoteDeviceError,  # noqa: F401
    SenderNotFoundError,  # noqa: F401
    SenderSshError,  # noqa: F401
    SenderUnreachableError,  # noqa: F401
)
from senders import WIN_REMOTE_TMP_DIR, make_sender  # noqa: F401
# pylint: enable=import-error, wrong-import-position, unused-import

# This code is used as the default failure value for recordings in the case that
# `results.get()` throws an unexpected error. -128 is chosen as a clear fail
# case (large negative) that won't overly distort tracking graphs.
FAIL_CODE = -128

METRICS = [
    'smoothness',
    'freezing',
    'dropped_frame_count',
    'total_frame_count',
    'dropped_frame_percentage',
]

# Framerate is now legacy data, but until our results are standardized we'll
# maintain the data in case it's necessary to pass later.
VIDEOS = [
    {'name': '1080p30fpsAV1_foodmarket_sync.mp4', 'fps': 30},
    {'name': '1080p30fpsH264_foodmarket_yt_sync.mp4', 'fps': 30},
    {'name': '1080p60fpsHEVC_boat_yt_sync.mp4', 'fps': 60},
    {'name': '1080p60fpsVP9_boat_yt_sync.webm', 'fps': 60},
]


class StartProcess(AbstractContextManager):
    """Starts a multiprocessing.Process."""

    def __init__(self, target, args, terminate: bool):
        self._proc = multiprocessing.Process(target=target, args=args)
        self._terminate = terminate

    def __enter__(self):
        self._proc.start()

    def __exit__(self, exc_type, exc_value, traceback):
        if self._terminate:
            self._proc.terminate()
        self._proc.join()
        if not self._terminate:
            assert self._proc.exitcode == 0


def _sender_for_cleanup(args):
    """Returns the Sender for `args`, or None if there is none to clean up."""
    try:
        return make_sender(args)
    except NotImplementedError as e:
        logging.warning('Skipping remote cleanup: %s', e)
        return None


def dump_remote_logs(args, chrome_version=None, codec_name=None):
    """Logs the sender's chromedriver console output.

    `chrome_version` and `codec_name` are unused and kept for existing callers;
    every console log in the sender's temp directory is dumped.
    """
    del chrome_version, codec_name
    make_sender(args).dump_console_logs()


def teardown_recording_process(rec_proc):
    """
    Tears down the recording process.

    This function safely tears down the ffmpeg recording process via either
    a graceful wait or a forceful terminate.

    Args:
        rec_proc (subprocess.Popen): The video recording process.
    """
    if rec_proc is not None:
        logging.info("Waiting for recording to finish...")
        try:
            rec_proc.communicate(timeout=20)
            logging.info("Recording finished.")
        except subprocess.TimeoutExpired as e:
            logging.warning(
                "WARNING: Recording process timed out after 20 "
                "seconds. Terminating it now."
            )
            rec_proc.terminate()
            rec_proc.wait()
            raise RuntimeError(
                "Recording process timed out and was forcefully terminated."
            ) from e


def teardown_test_environment(driver, tunnel_proc, args):
    """
    Tears down the test environment, ensuring the driver and tunnel are safely
    terminated.

    This function safely terminates the Selenium WebDriver, and the SSH
    tunnel, then removes downloaded zips from the sender. The sender cleanup
    never raises.

    Args:
        driver (webdriver.Remote): The Selenium WebDriver instance.
        tunnel_proc: The SSH tunnel process, or the Crossbench platform on
            ChromeOS.
        args: The parsed command-line arguments.
    """
    if driver:
        driver.quit()
        logging.info("Terminated chromedriver.")

    if tunnel_proc:
        if hasattr(tunnel_proc, 'poll'):
            if tunnel_proc.poll() is None:
                tunnel_proc.terminate()
                logging.info("Terminated tunnel.")
        elif hasattr(tunnel_proc, 'ports'):
            # Handle Crossbench platform objects.
            try:
                tunnel_proc.ports.stop_reverse_forward(SERVER_PORT)
                logging.info("Stopped Crossbench port forwarding.")
            except Exception as e:  # pylint: disable=broad-exception-caught
                logging.warning("Failed to stop Crossbench forwarding: %s", e)

    sender = _sender_for_cleanup(args)
    if sender:
        sender.cleanup_downloads()


def cleanup_binaries(args, chrome_version=None):
    """Removes Chrome/chromedriver from the sender. Never raises."""
    del chrome_version
    sender = _sender_for_cleanup(args)
    if sender:
        sender.cleanup_binaries()


def start_glances_monitoring(args, csv_remote_path):
    """Starts glances (or ChromeOS power logging) on the sender."""
    return make_sender(args).start_monitoring(csv_remote_path)


def stop_glances_monitoring(
    args, glances_proc, csv_remote_path, csv_local_path
):
    """Stops monitoring, copies its output here, and cleans up the sender."""
    make_sender(args).stop_monitoring(
        glances_proc, csv_remote_path, csv_local_path
    )
