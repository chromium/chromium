# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Performance test suite for video casting on a Fuchsia device.

This script uses Selenium and Chromedriver to automate performance tests for
video playback and casting. It sets up an SSH tunnel to a remote machine,
records the casted video using ffmpeg, and analyzes the output for metrics
like dropped frames and smoothness.
"""

import logging
import multiprocessing
import os
import shutil
import socket
import subprocess
import sys
import time

from contextlib import AbstractContextManager

from selenium import webdriver
from selenium.webdriver.common.by import By
from selenium.webdriver.chrome.options import Options as ChromeOptions
from selenium.webdriver.support.ui import WebDriverWait
from selenium.webdriver.support import expected_conditions as ec

# pylint: disable=import-error, wrong-import-position
REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..',
                                         '..', '..'))

MEASURES_ROOT = os.path.join(REPO_ROOT, 'build', 'util', 'lib', 'proto')
sys.path.append(MEASURES_ROOT)
import measures

CHROME_FUCHSIA_ROOT = os.path.join(REPO_ROOT, 'fuchsia_web', 'av_testing')
sys.path.append(CHROME_FUCHSIA_ROOT)
import server
import video_analyzer

TEST_SCRIPTS_ROOT = os.path.join(REPO_ROOT, 'build', 'fuchsia', 'test')
sys.path.append(TEST_SCRIPTS_ROOT)
from repeating_log import RepeatingLog
# pylint: enable=import-error, wrong-import-position

CHROMEDRIVER_PORT = int(os.environ.get('CHROMEDRIVER_PORT', '49573'))
SENDER = os.environ.get('SENDER')
USERNAME = os.environ.get('USERNAME')
PASSWORD = os.environ.get('PASSWORD')
RECEIVER = os.environ.get('RECEIVER')
SERVER_PORT = int(os.environ.get('SERVER_PORT', '8000'))

CAST_BTN_XPATH = "//button[text()='Launch app']"
CAST_URL = "https://storage.googleapis.com/castapi/CastHelloVideo/index.html"
RECORDINGS_DIR = os.path.join(os.environ.get('TMPDIR', '/tmp'), 'recordings')
REMOTE_URL = f'http://127.0.0.1:{CHROMEDRIVER_PORT}'

# This code is used as the default failure value for recordings in the case that
# `results.get()` throws an unexpected error. -128 is chosen as a clear fail
# case (large negative) that won't overly distort tracking graphs.
FAIL_CODE = -128

CHROME_OPTIONS = [
    # Redirects logging output to stderr to better catch automation issues.
    "--enable-logging=stderr",
    # Allows casting to devices with public IPs, which is necessary for our
    # current lab setup.
    "--media-router-cast-allow-all-ips",
    # Sets the default verbose logging level to 1.
    "--v=1",
    # Sets specific verbose logging levels for specific components relevant to
    # media routing and casting.
    "--vmodule=media_router*=3,discovery_mdns*=3,cast*=3,webrtc_logging*=3"
    # Uses a basic password store for consistent automated authentication.
    # "--password-store=basic"
]

METRICS = [
    'smoothness',
    'freezing',
    'dropped_frame_count',
    'total_frame_count',
    'dropped_frame_percentage'
]

VIDEOS = [
    {
        'name': '1080p30fpsAV1_foodmarket_sync.mp4',
        'fps': 30
    },
    {
        'name': '1080p30fpsH264_foodmarket_yt_sync.mp4',
        'fps': 30
    },
    {
        'name': '1080p60fpsHEVC_boat_sync.mp4',
        'fps': 60
    },
    {
        'name': '1080p60fpsVP9_boat_yt_sync.webm',
        'fps': 60
    }
]

HOST_TUNNEL_CMD = [
    'ssh',
    '-i',
    '~/.ssh/id_ed25519',
    '-L',
    f'{CHROMEDRIVER_PORT}:127.0.0.1:{CHROMEDRIVER_PORT}',
    f'{USERNAME}@{SENDER}',
    '-N'
]

SENDER_CHROMEDRIVER_CMD = (
    f'nohup /opt/homebrew/bin/chromedriver --port={CHROMEDRIVER_PORT} '
    f'--allowed-origins=\"*\" '
    f'--verbose '
    f'--log-path=/tmp/chromedriver_verbose.log '
    f'--enable-chrome-logs '
    f'> /dev/null 2>&1 &'
)

SENDER_CHROMEDRIVER_CHECK_CMD = (
    'ps aux | grep chromedriver | grep -v grep'
)

SENDER_STATUS_CMD = (
    f'curl '
    f'-s '
    f'-o /dev/null '
    f'-w "%{{http_code}}" '
    f'http://127.0.0.1:{CHROMEDRIVER_PORT}/status'
)

SENDER_TERMINATE_DRIVER_CMD = (
    'killall chromedriver'
)

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

def send_ssh_command(hostname, username, password, command, blocking=False):
    """
    Sends a command to a remote host via SSH using a password.

    Args:
        hostname (str): The remote host to connect to.
        username (str): The username for the SSH connection.
        password (str): The password for the SSH connection.
        command (str): The command to execute on the remote host.
        blocking (bool): If True, waits for the command to complete.
                         If False, runs the command in a non-blocking way.

    Returns:
        subprocess.CompletedProcess or subprocess.Popen: The process object.
    """
    ssh_command = [
        'ssh',
        '-i',
        '~/.ssh/id_ed25519',
        f'{username}@{hostname}',
        command
    ]

    if blocking:
        process = subprocess.run(
            ssh_command,
            capture_output=True,
            text=True,
            timeout=60,
            check=False
        )
    else:
        process = subprocess.Popen( # pylint: disable=consider-using-with
            ssh_command,
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True
        )

    return process

def terminate_old_chromedriver():
    """Tries to terminate any existing chromedriver processes."""
    logging.info("Attempting to terminate old chromedriver processes...")
    send_ssh_command(SENDER, USERNAME, PASSWORD, SENDER_TERMINATE_DRIVER_CMD)

    for _ in range(5):
        result = send_ssh_command(SENDER, USERNAME, PASSWORD,
                                  SENDER_CHROMEDRIVER_CHECK_CMD, blocking=True)
        if not result.stdout.strip():
            logging.info("Old chromedriver processes confirmed gone.")
            return
        logging.info("Old chromedriver processes still present, waiting...")
        time.sleep(1)
    raise RuntimeError("Chromedriver processes lingered after kill attempts.")

def start_new_chromedriver():
    """Starts a new chromedriver process on the remote machine."""
    send_ssh_command(SENDER, USERNAME, PASSWORD, SENDER_CHROMEDRIVER_CMD)
    logging.info("Started new chromedriver.")

def wait_for_chromedriver():
    """Waits for the new chromedriver to be ready by checking its status URL."""
    logging.info("Starting Chromedriver status check...")
    for _ in range(5):
        try:
            result = send_ssh_command(SENDER, USERNAME, PASSWORD,
                                      SENDER_STATUS_CMD, blocking=True)
            stdout = result.stdout.strip()
            if result.returncode == 0 and stdout == '200':
                logging.info("Chromedriver is ready.")
                return
            logging.info("Connection refused (curl code 7). Not ready yet...")
        except subprocess.TimeoutExpired:
            logging.warning("Status check timed out. Retrying...")
        except Exception as e: # pylint: disable=broad-exception-caught
            logging.warning("A script-level error occurred: %s. Retrying...", e)
        time.sleep(2)
    raise RuntimeError("Chromedriver still not ready after multiple attempts.")

def start_ssh_tunnel():
    # pylint: disable=consider-using-with
    """Starts the SSH tunnel process."""
    tunnel_proc = subprocess.Popen(HOST_TUNNEL_CMD)
    logging.info("Started tunnel.")
    return tunnel_proc

def connect_to_remote_driver(chrome_options):
    """Attempts to connect to the remote chromedriver via the tunnel."""
    logging.info("Attempting connection to %s.", REMOTE_URL)

    for _ in range(20):
        try:
            driver = webdriver.Remote(
                command_executor=REMOTE_URL,
                options=chrome_options
            )
            logging.info("Successfully connected!")
            return driver
        except Exception: #pylint: disable=broad-exception-caught
            logging.info("Tunnel not yet up. Sleeping ...")
            time.sleep(2)
    raise RuntimeError("Could not connect to the remote chromedriver.")

def setup_test_environment():
    """
    Sets up the remote chromedriver and SSH tunnel for testing.

    This function terminates any old Chromedriver processes, starts a new one,
    waits for it to be ready, and then establishes an SSH tunnel to it. It then
    connects a WebDriver instance to the tunnel and enables Cast discovery.

    Returns:
        tuple: A tuple containing the WebDriver and the tunnel process.
    """
    terminate_old_chromedriver()
    start_new_chromedriver()
    wait_for_chromedriver()
    tunnel_proc = start_ssh_tunnel()

    chrome_options = ChromeOptions()
    for option in CHROME_OPTIONS:
        chrome_options.add_argument(option)
    driver = connect_to_remote_driver(chrome_options)

    enable_tab_mirroring(driver)

    return driver, tunnel_proc

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
            logging.warning("WARNING: Recording process timed out after 20 \
                            seconds. Terminating it now.")
            rec_proc.terminate()
            rec_proc.wait()
            raise RuntimeError("Recording process timed out and was forcefully \
                                terminated.") from e

def teardown_test_environment(driver, tunnel_proc):
    """
    Tears down the test environment, ensuring the driver and tunnel are safely
    terminated.

    This function safely terminates the the Selenium WebDriver, and the SSH
    tunnel. It handles timeouts gracefully and ensures resources are released
    properly.

    Args:
        driver (webdriver.Remote): The Selenium WebDriver instance.
        tunnel_proc (subprocess.Popen): The SSH tunnel process.
    """
    if driver:
        driver.quit()
        logging.info("Terminated chromedriver.")

    if tunnel_proc and tunnel_proc.poll() is None:
        tunnel_proc.terminate()
        logging.info("Terminated tunnel.")

def enable_tab_mirroring(driver):
    """
    Navigates to the CastHelloVideo page and enables Cast discovery.

    This function first loads a specific URL, waits for the 'Launch app' button
    to be clickable, clicks it, and then sends a CDP command to enable
    Cast discovery in the browser.

    Args:
        driver: The Selenium WebDriver instance.
    """
    driver.get(CAST_URL)

    wait = WebDriverWait(driver, 30)  # Wait up to 30 seconds for the element
    button = wait.until(ec.element_to_be_clickable((By.XPATH, CAST_BTN_XPATH)))
    button.click()

    logging.info("Enabling Cast discovery via CDP...")
    driver.execute_cdp_cmd("Cast.enable", {"presentationUrl": ""})


def start_tab_mirroring(driver, receiver):
    """
    Starts tab mirroring to a specified Cast receiver with retries.

    This function attempts to start tab mirroring to a receiver using CDP. It
    includes a retry loop to handle potential race conditions, and it will fail
    with a RuntimeError if the command does not succeed after multiple attempts.

    Args:
        driver: The Selenium WebDriver instance.
        receiver: The name of the Cast receiver to mirror to.

    Returns:
        bool: True if tab mirroring was successfully initiated.

    Raises:
        RuntimeError: If tab mirroring fails after all retries.
    """
    max_retries = 5
    for attempt in range(1, max_retries + 1):
        try:
            driver.execute_cdp_cmd("Cast.startTabMirroring",
                                   {"sinkName": receiver})
            logging.info("'Cast.startTabMirroring' command sent to %s.",
                         receiver)
            return True
        except Exception as e: # pylint: disable=broad-exception-caught
            logging.warning(
                "Attempt %d failed to start mirroring: %s. Retrying...",
                attempt, e, exc_info=False)
            time.sleep(1)

    raise RuntimeError("Failed to start tab mirroring.")

# pylint: disable=too-many-locals
def run_performance_test(video_file: str, framerate: int, driver: webdriver):
    """
    Runs a single video performance test by casting and recording the video.

    This function navigates to the video player page, enables Cast discovery,
    starts an ffmpeg recording process, initiates tab mirroring, plays a video,
    and then analyzes the recorded output for performance metrics.

    Args:
        video_file (str): The name of the video file to be tested.
        framerate (int): The framerate of the video.
        driver (webdriver.Remote): The Selenium WebDriver instance.

    Returns:
        subprocess.Popen: The Popen object for the ffmpeg recording process.
    """
    # force video output to mp4
    output_file = os.path.join(RECORDINGS_DIR,
                               video_file.replace('.webm', '.mp4'))
    host_recording_cmd = [
        'ffmpeg',
        '-y',
        '-f', 'video4linux2',
        '-framerate', str(framerate),
        '-video_size', '3840x2160',
        '-input_format', 'yuyv422',
        '-i', '/dev/video0',
        '-thread_queue_size', '1024',
        '-c:v', 'libx264',
        '-preset', 'ultrafast',
        '-crf', '28',
        '-pix_fmt', 'yuv420p',
        '-g', '60',
        '-t', '35',
        output_file
    ]

    wait = WebDriverWait(driver, 30)
    driver.get(f'http://{socket.gethostbyname(socket.gethostname())}:'
               f'{SERVER_PORT}/video.html?file={video_file}')
    wait.until(ec.presence_of_element_located((By.ID, "video")))

    try:
        # pylint: disable=consider-using-with
        rec_proc_local = subprocess.Popen(
            host_recording_cmd,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True)

        logging.info("ffmpeg recording process started. Waiting for 'Stream "
                     "mapping:' confirmation...")

        while True:
            line = rec_proc_local.stderr.readline() # Use local variable
            if line:
                line = line.strip()
                logging.info("FFMPEG STARTUP: %s", line)
                if "Stream mapping:" in line:
                    logging.info("Started recording.")
                    break

        casting = start_tab_mirroring(driver, RECEIVER)

        def _wait_js_condition(driver, element, condition: str) -> bool:
            """Waits a condition on the element once a second for at most 30
            seconds, returns True if the condition met."""
            start = time.time()
            while not driver.execute_script(f'return arguments[0].{condition};',
                                            element):
                if time.time() - start >= 30:
                    return False
                time.sleep(1)
            return True

        video = driver.find_element(By.ID, 'video')

        with measures.time_consumption(video_file, 'video_perf', 'playback',
                                       'loading'), \
             RepeatingLog(f'Waiting for video {video_file} to be loaded.'):
            if not _wait_js_condition(driver, video, 'readyState >= 2'):
                logging.warning(
                    '%s may never be loaded, still go ahead to play it.',
                    video_file)
                measures.average(video_file, 'video_perf', 'playback',
                                 'failed_to_load').record(1)

        video.click()
        logging.info("Started playing video.")

        logging.info("Casting for 30 seconds (script will then stop casting "
                     "and quit)...")
        time.sleep(30)

        rec_proc_local.communicate()
        logging.info("recording finished.")

        results = video_analyzer.from_original_video(
            output_file, f"/usr/local/cipd/videostack_videos_30s/{video_file}")

        if not results:
            raise RuntimeError("Missing video analyzer results. See log for "
                               "further details.")

        def record(key: str) -> None:
            # If the video_analyzer does not generate any result, treat it as an
            # error and use the default value to filter them out instead of
            # failing the tests.
            measures.average(video_file, 'video_perf', key).record(
                results.get(key, FAIL_CODE))

        for metric in METRICS:
            record(metric)

        logging.warning('Video analysis result of %s: %s', video_file, results)

    except Exception as e:
        raise RuntimeError(f"Error during CDP Cast command: {e}\nCheck "
                           "the chromedriver log on the remote laptop for "
                           "more details.") from e
    finally:
        if driver:
            if casting and RECEIVER:
                logging.info('Attempting to stop casting to "%s"...', RECEIVER)
                try:
                    driver.execute_cdp_cmd("Cast.stopCasting",
                                           {"sinkName": RECEIVER})
                    logging.info("'Cast.stopCasting' command sent.")
                    casting = False
                except Exception as e:
                    raise RuntimeError(f"Error stopping cast: "
                                       f"{e}") from e
    return rec_proc_local

def main():
    """
    Runs the performance testing suite for all videos.

    This function sets up a single remote test environment (Chromedriver and SSH
    tunnel) and then iterates through a list of videos. For each video, it runs
    a performance test, logs any errors, and cleans up the video-specific
    resources (like the recording process). Finally, it tears down the shared
    test environment (Chromedriver and SSH tunnel).

    Returns:
        int: The exit code for the script, typically 0 for success.
    """
    logging.basicConfig(level=logging.INFO)

    if os.path.exists(RECORDINGS_DIR):
        shutil.rmtree(RECORDINGS_DIR)
    os.makedirs(RECORDINGS_DIR)

    driver = None
    tunnel_proc = None

    try:
        driver, tunnel_proc = setup_test_environment()
        for video in VIDEOS:
            logging.info("Starting test for video: %s", video['name'])
            rec_proc = None
            try:
                rec_proc = run_performance_test(video['name'],
                                                video['fps'],
                                                driver)
            except Exception: # pylint: disable=broad-exception-caught
                logging.exception("Error during video %s test", video['name'])
            finally:
                teardown_recording_process(rec_proc)
    finally:
        teardown_test_environment(driver, tunnel_proc)

if __name__ == '__main__':
    with StartProcess(server.start, [SERVER_PORT], True):
        sys.exit(main())
