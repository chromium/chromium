#!/usr/bin/env vpython3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Performance test suite for media remoting on a laptop device.

This script uses Selenium and Chromedriver to automate performance tests for
media remoting. It sets up an SSH tunnel to a remote machine, records the
output using a Basler camera, and analyzes the results for performance metrics.
"""

import argparse
import logging
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

CHROME_OPTIONS = [
    # Redirects logging output to stderr to better catch automation issues.
    "--enable-logging=stderr",
    # Sets the default verbose logging level to 1.
    "--v=1",
    # Disables the sandbox, often necessary in automated test environments.
    "--no-sandbox",
    # Disables the GPU sandbox, used to prevent issues with GPU crashes.
    "--disable-gpu-sandbox",
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

    Returns:
        tuple: A tuple containing the WebDriver, the tunnel process, and the
               actual chrome version used.
    """
    common.terminate_old_chromedriver(args)
    remote_app_path, actual_version = common.install_and_setup_chrome(
        args, chrome_version)
    common.wait_for_chromedriver(args)
    tunnel_proc = common.start_ssh_tunnel(args)

    chrome_options = ChromeOptions()
    for option in CHROME_OPTIONS:
        chrome_options.add_argument(option)

    binary_path = None
    if args.sender_os == 'mac':
        # Split long path for 80 char limit compliance.
        binary_path = (f'{remote_app_path}/Contents/MacOS/'
                       'Google Chrome for Testing')
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

    return driver, tunnel_proc, actual_version

def run_performance_test(video_file: str, driver: webdriver, args):
    """
    Runs a single remoting performance test.
    """
    # TODO: Implement Basler camera recording startup.
    logging.info("Basler camera recording not implemented. Skipping recording.")
    rec_proc_local = None

    driver.get(f'http://{common.LOCAL_HOST_IP}:'
               f'{common.SERVER_PORT}/video.html?file={video_file}')

    # Wait for the video element to be present.
    wait = WebDriverWait(driver, 30)
    wait.until(ec.presence_of_element_located((By.ID, "video")))

    # TODO: Implement remoting logic (start remoting to device).
    logging.info("Remoting to device not yet implemented.")

    # TODO: Implement Basler camera recording teardown and video analysis.
    logging.info("Video analysis for Basler camera output not implemented.")

    return rec_proc_local

def main():
    """
    Runs the performance testing suite for all videos.
    """
    logging.getLogger().setLevel(logging.INFO)

    parser = argparse.ArgumentParser(
        description="Performance test for media remoted to a device.",
    )
    parser.add_argument('--username', help='Sender device username.')
    parser.add_argument('--sender', help='Sender device IP.')
    parser.add_argument('--receiver',
                        help='Receiver device info (placeholder).')
    parser.add_argument(
        '--chrome-version',
        default=None,
        help='Chrome for Testing version to use.')
    parser.add_argument('--sender-os', help='OS of the sender device.')
    args, _ = parser.parse_known_args()
    cv = args.chrome_version

    if os.path.exists(common.RECORDINGS_DIR):
        shutil.rmtree(common.RECORDINGS_DIR)
    os.makedirs(common.RECORDINGS_DIR)

    driver = None
    tunnel_proc = None
    actual_version = None

    try:
        driver, tunnel_proc, actual_version = setup_test_environment(args, cv)
        for video in common.VIDEOS:
            logging.info("Starting test for video: %s", video['name'])
            rec_proc = None
            try:
                rec_proc = run_performance_test(video['name'], driver, args)
            except Exception: # pylint: disable=broad-exception-caught
                logging.exception("Error during video %s test",
                                  video['name'])
                common.dump_remote_logs(args)
                raise
            finally:
                common.teardown_recording_process(rec_proc)
    finally:
        common.finalize_results(actual_version)
        common.teardown_test_environment(driver, tunnel_proc, args)

if __name__ == '__main__':
    with common.StartProcess(common.server.start, [common.SERVER_PORT], True):
        sys.exit(main())
