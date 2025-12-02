# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Performance test suite for video casting on a Chromecast device.

This script uses Selenium and Chromedriver to automate performance tests for
video playback and casting. It sets up an SSH tunnel to a remote machine,
records the casted video using ffmpeg, and analyzes the output for metrics
like dropped frames and smoothness.
"""

import argparse
import logging
import multiprocessing
import os
import shutil
import subprocess
import sys
import time

from selenium import webdriver
from selenium.webdriver.common.by import By
from selenium.webdriver.chrome.options import Options as ChromeOptions
from selenium.webdriver.support.ui import WebDriverWait
from selenium.webdriver.support import expected_conditions as ec

import common

# pylint: disable=import-error, wrong-import-position
from repeating_log import RepeatingLog
# pylint: enable=import-error, wrong-import-position

CAST_BTN_XPATH = "//button[text()='Launch app']"
CAST_URL = "https://storage.googleapis.com/castapi/CastHelloVideo/index.html"

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
    "--vmodule=media_router*=3,discovery_mdns*=3,cast*=3,"
    "webrtc_logging*=3",
    # Skips the first-run experience modal.
    "--no-first-run",
    # Prevents the "Set as default browser" prompt from appearing.
    "--no-default-browser-check",
    # Launches Chrome in fullscreen mode to prevent scrollbar clipping.
    "--start-fullscreen",
]


def connect_to_remote_driver(chrome_options, binary_location):
    """Attempts to connect to the remote chromedriver via the tunnel."""
    logging.info("Attempting connection to %s.", common.REMOTE_URL)

    # Set the binary location directly on the options object.
    if binary_location:
        chrome_options.binary_location = binary_location

    for _ in range(20):
        try:
            driver = webdriver.Remote(
                command_executor=common.REMOTE_URL,
                options=chrome_options
            )
            logging.info("Successfully connected!")
            return driver
        except Exception as e: #pylint: disable=broad-exception-caught
            logging.info("Tunnel not yet up. Sleeping ... Error: %s", e)
            time.sleep(2)
    raise RuntimeError("Could not connect to the remote chromedriver.")

def setup_test_environment(args, chrome_version):
    """
    Sets up the remote chromedriver and SSH tunnel for testing.

    This function terminates any old Chromedriver processes, starts a new one,
    waits for it to be ready, and then establishes an SSH tunnel to it. It then
    connects a WebDriver instance to the tunnel and enables Cast discovery.

    Returns:
        tuple: A tuple containing the WebDriver and the tunnel process.
    """
    common.terminate_old_chromedriver(args)
    remote_app_path = common.install_and_setup_chrome(args, chrome_version)
    common.wait_for_chromedriver(args)
    tunnel_proc = common.start_ssh_tunnel(args)

    chrome_options = ChromeOptions()
    for option in CHROME_OPTIONS:
        chrome_options.add_argument(option)

    binary_path = None
    if args.sender_os == 'mac':
        binary_path = (f'{remote_app_path}/Contents/MacOS/Google Chrome for '
                       'Testing')
        logging.info(
            "Mac OS detected. Setting binary_location to: %s",
            binary_path)
    elif args.sender_os == 'win':
        logging.info(
            "Windows OS detected. Setting binary_location to: %s",
            remote_app_path)
        binary_path = remote_app_path

    chrome_options.binary_location = binary_path
    driver = connect_to_remote_driver(chrome_options, binary_path)
    enable_tab_mirroring(driver)

    return driver, tunnel_proc


def enable_tab_mirroring(driver):
    """
    Navigates to the CastHelloVideo page and enables Cast discovery.

    This function first loads a specific URL, waits for the 'Launch app' button
    to be clickable, clicks it, sends a CDP command to enable Cast discovery,
    and then waits for a receiver to be found.

    Args:
        driver: The Selenium WebDriver instance.
    """
    driver.get(CAST_URL)

    wait = WebDriverWait(driver, 300)
    button = wait.until(
        ec.element_to_be_clickable((By.XPATH, CAST_BTN_XPATH)))
    button.click()

    logging.info("Enabling Cast discovery via CDP...")
    driver.execute_cdp_cmd("Cast.enable", {"presentationUrl": ""})

    # Wait for the debug message to indicate a receiver has been found.
    logging.info("Waiting for receiver to be found...")
    wait.until(
        ec.text_to_be_present_in_element((By.ID, "debugmessage"),
                                         "receiver found"))
    logging.info("Receiver found in debug message.")

def start_tab_mirroring(driver, args):
    """
    Starts tab mirroring to a specified Cast receiver with retries.

    This function attempts to start tab mirroring to a receiver using CDP. It
    includes a retry loop to handle potential race conditions, and it will fail
    with a RuntimeError if the command does not succeed after multiple attempts.

    Args:
        driver: The Selenium WebDriver instance.
        args: The parsed command-line arguments.

    Returns:
        bool: True if tab mirroring was successfully initiated.

    Raises:
        RuntimeError: If tab mirroring fails after all retries.
    """
    max_retries = 5
    for attempt in range(1, max_retries + 1):
        try:
            driver.execute_cdp_cmd("Cast.startTabMirroring",
                                   {"sinkName": args.receiver})
            logging.info("'Cast.startTabMirroring' command sent to %s.",
                         args.receiver)
            return True
        except Exception as e: # pylint: disable=broad-exception-caught
            logging.warning(
                "Attempt %d failed to start mirroring: %s. Retrying...",
                attempt, e, exc_info=False)
            time.sleep(1)

    raise RuntimeError("Failed to start tab mirroring.")

# pylint: disable=too-many-locals
def run_performance_test(video_file: str, framerate: int,
                         driver: webdriver, args):
    """
    Runs a single video performance test by casting and recording the video.

    This function navigates to the video player page, enables Cast discovery,
    starts an ffmpeg recording process, initiates tab mirroring, plays a video,
    and then analyzes the recorded output for performance metrics.

    Args:
        video_file (str): The name of the video file to be tested.
        framerate (int): The framerate of the video.
        driver (webdriver.Remote): The Selenium WebDriver instance.
        args: The parsed command-line arguments.

    Returns:
        subprocess.Popen: The Popen object for the ffmpeg recording process.
    """
    # force video output to mp4
    output_file = os.path.join(common.RECORDINGS_DIR,
                               video_file.replace('.webm', '.mp4'))

    width, height, fps = common._query_v4l2_device('/dev/video0')

    host_recording_cmd = [
        'ffmpeg',
        '-y',
        '-f', 'video4linux2',
        '-framerate', str(fps),
        '-video_size', f'{width}x{height}',
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
    driver.get(f'http://{common.LOCAL_HOST_IP}:'
               f'{common.SERVER_PORT}/video.html?file={video_file}')
    wait.until(ec.presence_of_element_located((By.ID, "video")))

    casting = False
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
            line = rec_proc_local.stderr.readline()
            if not line:
                raise RuntimeError("ffmpeg process exited before starting.")
            line = line.strip()
            logging.info("FFMPEG STARTUP: %s", line)
            if "Stream mapping:" in line:
                logging.info("Started recording.")
                break

        casting = start_tab_mirroring(driver, args)

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

        with common.measures.time_consumption(video_file,
                                              'video_perf',
                                              'playback',
                                              'loading'), \
             RepeatingLog(f'Waiting for video {video_file} to be loaded.'):
            if not _wait_js_condition(driver, video, 'readyState >= 2'):
                logging.warning(
                    '%s may never be loaded, still go ahead to play it.',
                    video_file)
                common.measures.average(video_file, 'video_perf', 'playback',
                                 'failed_to_load').record(1)

        video.click()
        logging.info("Started playing video.")

        logging.info("Casting for 30 seconds (script will then stop casting "
                     "and quit)...")
        time.sleep(30)

        rec_proc_local.communicate()
        logging.info("recording finished.")

        results = common.video_analyzer.from_original_video(
            output_file, f"/usr/local/cipd/videostack_videos_30s/{video_file}")
        if not results:
            raise RuntimeError("Missing video analyzer results. See log for "
                               "further details.")

        def record(key: str) -> None:
            # If the video_analyzer does not generate any result, treat it as an
            # error and use the default value to filter them out instead of
            # failing the tests.
            common.measures.average(video_file, 'video_perf', key).record(
                results.get(key, common.FAIL_CODE))

        for metric in common.METRICS:
            record(metric)

        logging.warning('Video analysis result of %s: %s', video_file, results)
    finally:
        if driver:
            if casting and args.receiver:
                logging.info('Attempting to stop casting to "%s"...',
                             args.receiver)
                try:
                    driver.execute_cdp_cmd("Cast.stopCasting",
                                           {"sinkName": args.receiver})
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
    logging.getLogger().setLevel(logging.INFO)

    parser = argparse.ArgumentParser(
        description="Performance test for media played via Chromecast.",
    )
    parser.add_argument('--username', help='Sender device username.')
    parser.add_argument('--sender', help='Sender device IP.')
    parser.add_argument('--receiver', help='Receiver device sink name.')
    parser.add_argument(
        '--chrome-version',
        default=None,
        help='Chrome for Testing version to use. Defaults to the latest '
    'known good version.')
    parser.add_argument('--sender-os', help='OS of the sender device.')
    args, _ = parser.parse_known_args()
    cv = args.chrome_version

    if os.path.exists(common.RECORDINGS_DIR):
        shutil.rmtree(common.RECORDINGS_DIR)
    os.makedirs(common.RECORDINGS_DIR)

    driver = None
    tunnel_proc = None

    try:
        driver, tunnel_proc = setup_test_environment(args, cv)
        for video in common.VIDEOS:
            logging.info("Starting test for video: %s", video['name'])
            rec_proc = None
            try:
                rec_proc = run_performance_test(video['name'],
                                                video['fps'],
                                                driver,
                                                args)
            except Exception: # pylint: disable=broad-exception-caught
                logging.exception("Error during video %s test", video['name'])
                raise
            finally:
                common.teardown_recording_process(rec_proc)
    finally:
        common.teardown_test_environment(driver, tunnel_proc, args)

if __name__ == '__main__':
    with common.StartProcess(common.server.start, [common.SERVER_PORT], True):
        sys.exit(main())

