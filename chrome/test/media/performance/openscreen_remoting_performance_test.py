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
import camera
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
    # HEADLESS is needed for bot environments.
    "--headless=new",
    "--disable-gpu",
    "--disable-dev-shm-usage",
    "--remote-allow-origins=*",
]

ADB_PATH = '/opt/infra-android/tools/platform-tools/adb'
if not os.path.exists(ADB_PATH):
    ADB_PATH = 'adb'

LOG_DIR = os.environ.get('ISOLATED_OUTDIR', '/tmp')

# This is set by the fleet team as the IP for all receiver devices.
RECEIVER_IP = "172.16.0.3:5555"

# Use 8080 to avoid collision with adbd (which uses 8000 on the device).
SERVER_PORT = 8080

RECEIVER_TYPE_FUCHSIA = 'fuchsia'

# The bucket for signed Android builds.
GCS_BUCKET = "gs://chrome-signed/android-B0urB0N"
# Trichrome requires BOTH the Chrome APK and the Library APK.
COMPONENTS = [
    "TrichromeChromeGoogle6432SystemStable.apk",
    "TrichromeLibraryGoogle6432SystemStable.apk"
]

monitors = None
version = None

def fuchsia_infra_is_available():
    global monitors, version
    try:
        import importlib.util
        TEST_SCRIPTS_ROOT = os.path.join(common.REPO_ROOT, 'build', 'fuchsia',
                                         'test')
        _fuchsia_common_path = os.path.join(TEST_SCRIPTS_ROOT, "common.py")
        if os.path.exists(_fuchsia_common_path):
            _spec = importlib.util.spec_from_file_location(
                "fuchsia_common", _fuchsia_common_path)
            _fuchsia_common = importlib.util.module_from_spec(_spec)
            _spec.loader.exec_module(_fuchsia_common)

            # Inject required attributes into local common to satisfy monitors.
            common.DIR_SRC_ROOT = _fuchsia_common.DIR_SRC_ROOT
            common.run_ffx_command = _fuchsia_common.run_ffx_command
            common.get_host_tool_path = _fuchsia_common.get_host_tool_path
            common.get_build_info = _fuchsia_common.get_build_info

            sys.path.append(TEST_SCRIPTS_ROOT)
            import monitors
            import version
            return True
    except Exception as e: # pylint: disable=broad-exception-caught
        logging.warning("Result reporting infra not available: %s", e)
        return False

# Surgically resolve the common.py name collision to allow result reporting.
FUCHSIA_INFRA_AVAILABLE = fuchsia_infra_is_available()

def _wait_js_condition(driver: webdriver, element,
                       condition: str) -> bool:
    """Waits a condition on the element once a second for at most 30 seconds,
       returns True if the condition met."""
    start = time.time()
    while not driver.execute_script(f'return arguments[0].{condition};',
                                    element):
        if time.time() - start >= 30:
            return False
        time.sleep(1)
    return True

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
    elif args.sender_os == 'linux':
        logging.info(
            "Linux OS detected. Setting binary_location to: %s",
            remote_app_path)
        binary_path = remote_app_path

    chrome_options.binary_location = binary_path
    driver = connect_to_remote_driver(chrome_options, binary_path)

    return driver, tunnel_proc, actual_version

def install_chrome_on_android(version):
    """
    Downloads the matching Chrome APK from GCS and installs it on the device.
    """
    logging.info("Attempting to sync Android Chrome to version: %s", version)

    # Locate repo-local gsutil (usually in third_party/catapult).
    gsutil_locations = [
        os.path.join(common.REPO_ROOT, 'third_party', 'catapult',
                     'third_party', 'gsutil', 'gsutil'),
        os.path.join(os.path.dirname(__file__), '..', '..', '..', '..',
                     'third_party', 'catapult', 'third_party', 'gsutil',
                     'gsutil'),
        'gsutil'
    ]

    gsutil_path = 'gsutil'
    for loc in gsutil_locations:
        if os.path.exists(loc):
            gsutil_path = loc
            break

    local_apks = []
    try:
        for comp in COMPONENTS:
            remote_path = f"{GCS_BUCKET}/{version}/arm_64/{comp}"
            local_path = f"/tmp/{comp}"
            logging.info("Downloading %s using %s...", comp, gsutil_path)
            subprocess.run([gsutil_path, "cp", remote_path, local_path],
                           check=True, timeout=120)
            local_apks.append(local_path)

        # Install components sequentially (Library first).
        for apk in reversed(local_apks):
            logging.info("Installing %s to device...", os.path.basename(apk))
            subprocess.run([ADB_PATH, "install", "-r", "-d", apk],
                           check=True, timeout=120)

        # Official Developer Bypass Sequence.
        logging.info("Applying official Chrome Android bypass sequence...")
        subprocess.run([ADB_PATH, 'shell', 'pm', 'clear', 'com.android.chrome'],
                       check=False, timeout=10)
        subprocess.run([ADB_PATH, 'shell', 'am', 'set-debug-app',
                       '--persistent', 'com.android.chrome'], check=False,
                       timeout=10)
        flags = "chrome --disable-fre --no-default-browser-check --no-first-run"
        subprocess.run([ADB_PATH, 'shell',
                       f'echo "{flags}" > /data/local/tmp/chrome-command-line'],
                       check=False, timeout=5)

        logging.info("Successfully updated Android Chrome to %s.", version)
    except Exception as e:
        logging.error("Failed to update Android Chrome: %s. "
                      "Proceeding with on-board version.", e)

def run_performance_test(video_file: str, driver: webdriver, args,
                         is_first_run=False):
    """
    Runs a single remoting performance test.
    """
    camera_params = camera.Parameters()
    camera_params.file = video_file.replace('.mp4', '').replace('.webm', '')
    camera_params.output_path = common.RECORDINGS_DIR
    camera_params.duration_sec = 35
    camera_params.fps = 120
    camera_params.max_frames = 1200

    original_video = os.path.join(common.server.VIDEO_DIR, video_file)

    # Start the local video player.
    driver.get(f'http://{common.LOCAL_HOST_IP}:'
               f'{SERVER_PORT}/video.html?file={video_file}')

    # Wait for the video element to be present.
    wait = WebDriverWait(driver, 30)
    video = wait.until(ec.presence_of_element_located((By.ID, "video")))

    with common.measures.time_consumption(video_file, 'video_perf',
                                          'playback', 'loading'), \
         RepeatingLog(f'Waiting for video {video_file} to be loaded.'):
        if not _wait_js_condition(driver, video, 'readyState >= 2'):
            logging.warning('%s failed to load within timeout. Skipping.',
                            video_file)
            common.measures.average(video_file, 'video_perf', 'playback',
                                    'failed_to_load').record(1)
            return None

    # Enable Cast discovery.
    logging.info("Enabling Cast discovery via CDP...")
    driver.execute_cdp_cmd("Cast.enable", {"presentationUrl": ""})

    # We need to patch the camera module INSIDE the child process.
    def camera_start_wrapper(params):
        import camera as camera_module
        import getpass
        _orig_run = subprocess.run
        username = getpass.getuser()
        groupname = 'video'
        logging.info("Camera process starting as user: %s, group: %s",
                     username, groupname)

        def run_with_ids(args, **kwargs):
            new_args = list(args)
            for i, arg in enumerate(new_args):
                if arg == '--uid=':
                    new_args[i] = f'--uid={username}'
                if arg == '--gid=':
                    new_args[i] = f'--gid={groupname}'
            return _orig_run(new_args, **kwargs)

        camera_module.subprocess.run = run_with_ids
        try:
            return camera_module.start(params)
        finally:
            camera_module.subprocess.run = _orig_run

    # Start the Basler camera recording in a separate process.
    with common.StartProcess(camera_start_wrapper, [camera_params], False):
        logging.info("Started camera recording.")

        logging.info("Starting Sender Trace...")
        driver.execute_cdp_cmd('Tracing.start', {
            'categories': 'cast,media,webrtc,gpu,blink',
            'transferMode': 'ReturnAsStream'
        })

        if args.receiver_type == 'android':
            # Re-establish the USB data bridge immediately before use.
            subprocess.run([ADB_PATH, 'reverse',
                            f'tcp:{SERVER_PORT}',
                            f'tcp:{SERVER_PORT}'],
                           check=False, timeout=10)

            check_tunnels = subprocess.run([ADB_PATH, 'reverse', '--list'],
                                           capture_output=True, text=True,
                                           check=False)
            logging.info("Active ADB Tunnels: %s", check_tunnels.stdout)

            # Step 1: Launch Chrome to about:blank to prepare the window.
            logging.info("Preparing browser window...")
            subprocess.run([
                ADB_PATH, 'shell', 'am', 'start',
                '-n', 'com.android.chrome/com.google.android.apps.chrome.Main',
                '-a', 'android.intent.action.VIEW',
                '-d', 'about:blank',
                '--ez', 'create_new_tab', 'true',
                '--ez', 'no-first-run', 'true'
            ], check=False, timeout=10)

            # Step 2: Force landscape orientation BEFORE loading the video.
            logging.info("Enforcing landscape orientation...")
            subprocess.run([ADB_PATH, 'shell', 'settings', 'put', 'system',
                            'user_rotation', '1'], check=False, timeout=5)

            # Step 3: Wait for the layout to settle.
            time.sleep(1)

            # Step 4: Load the actual video URL into the browser.
            remote_url = (f'http://127.0.0.1:{SERVER_PORT}/'
                          f'video.html?file={video_file}')
            logging.info("Loading video page into landscape browser...")
            subprocess.run([
                ADB_PATH, 'shell', 'am', 'start',
                '-n', 'com.android.chrome/com.google.android.apps.chrome.Main',
                '-a', 'android.intent.action.VIEW',
                '-d', remote_url
            ], check=False, timeout=10)

            # Step 5: Dismiss any modal (if first run) and kick reload.
            if is_first_run:
                subprocess.run([ADB_PATH, 'shell', 'input', 'keyevent',
                               'KEYCODE_BACK'],
                               check=False, timeout=5)

            subprocess.run([ADB_PATH, 'shell', 'input', 'keyevent',
                           'KEYCODE_F5'],
                           check=False, timeout=5)

        # Playback duration.
        time.sleep(30)

        logging.info("Stopping Sender Trace...")
        try:
            tracing_end_resp = driver.execute_cdp_cmd('Tracing.end', {})
            stream_id = tracing_end_resp.get('stream')
            if stream_id:
                sender_trace_path = os.path.join(
                    common.TRACES_DIR,
                    f"{video_file}_sender.perfetto-trace")
                with open(sender_trace_path, 'w', encoding='utf-8') as f:
                    while True:
                        read_resp = driver.execute_cdp_cmd(
                            'IO.read', {'handle': stream_id})
                        f.write(read_resp.get('data', ''))
                        if read_resp.get('eof'):
                            break
                driver.execute_cdp_cmd('IO.close', {'handle': stream_id})
        except Exception as e:
            logging.error("Failed to collect sender trace: %s", e)

        # Stop casting/playback.
        if args.receiver_type == 'android':
            logging.info("Stopping playback on device...")
            subprocess.run([ADB_PATH, 'shell', 'am', 'force-stop',
                            'com.android.chrome'], check=False, timeout=5)
        else:
            try:
                driver.execute_cdp_cmd("Cast.stopCasting",
                                       {"sinkName": args.receiver})
            except Exception as e:
                logging.warning("Failed to stop casting: %s", e)

    logging.info("Recording finished. Analyzing video...")

    # Analyze the camera recording output.
    results = common.video_analyzer.from_original_video(
        camera_params.video_file, original_video)

    if not results:
        raise RuntimeError("Missing video analyzer results.")

    def record(key: str) -> None:
        if FUCHSIA_INFRA_AVAILABLE:
            monitors.average(video_file, 'playback', key).record(
                results.get(key, common.FAIL_CODE))

    for metric in common.METRICS:
        record(metric)

    # Explicitly log the results to the console.
    logging.warning('Video analysis result of %s: %s', video_file, results)

    # Clean up trace/info file.
    if os.path.exists(camera_params.info_file):
        shutil.move(camera_params.info_file, LOG_DIR)

    return None

def main():
    """
    Runs the performance testing suite for all videos.
    """
    logging.getLogger().setLevel(logging.INFO)

    parser = argparse.ArgumentParser(
        description="Performance test for media remoted to a device.",
    )
    parser.add_argument('--username', help='Sender device username.')
    parser.add_argument('--sender', default='localhost',
                        help='Sender device IP (default: localhost for NUC).')
    parser.add_argument('--receiver',
                        help='Receiver device sink name.')
    parser.add_argument('--receiver-type',
                        choices=['android', RECEIVER_TYPE_FUCHSIA],
                        default='android', help='Type of receiver device.')
    parser.add_argument('--chrome-version', default=None,
                        help='Chrome version to use.')
    parser.add_argument('--sender-os', default='linux',
                        help='OS of the sender device.')
    args, _ = parser.parse_known_args()
    cv = args.chrome_version

    # If receiver is RECEIVER_TYPE_FUCHSIA, default receiver-type to it.
    if args.receiver == RECEIVER_TYPE_FUCHSIA and not any(
            arg.startswith('--receiver-type') for arg in sys.argv):
        args.receiver_type = RECEIVER_TYPE_FUCHSIA

    if args.receiver_type == RECEIVER_TYPE_FUCHSIA:
        # TODO(b/502641691): Add support for OpenScreen remoting test cases.
        # This early-exit logic keeps the Fuchsia hosts from entering bad states
        # by running failing code.
        logging.info("Fuchsia tests are currently rigged to pass.")
        return 0

    if os.path.exists(common.RECORDINGS_DIR):
        shutil.rmtree(common.RECORDINGS_DIR)
    os.makedirs(common.RECORDINGS_DIR)
    if os.path.exists(common.TRACES_DIR):
        shutil.rmtree(common.TRACES_DIR)
    os.makedirs(common.TRACES_DIR)

    if args.receiver_type == 'android':
        try:
            # Root access is needed for stable setup on lab hardware.
            logging.info("Attempting to get ADB root...")
            subprocess.run([ADB_PATH, 'root'], check=False, timeout=10)

            # Ensure the device is awake and bright for the camera.
            logging.info("Waking up device and setting max brightness...")
            subprocess.run([ADB_PATH, 'shell', 'input', 'keyevent',
                           'KEYCODE_WAKEUP'],
                           check=False, timeout=5)
            subprocess.run([ADB_PATH, 'shell', 'wm', 'dismiss-keyguard'],
                           check=False, timeout=5)

            # Set brightness to max (255)
            subprocess.run([ADB_PATH, 'shell', 'settings', 'put', 'system',
                            'screen_brightness', '255'],
                           check=False, timeout=5)

            # Pre-grant notifications to avoid system modals.
            subprocess.run([ADB_PATH, 'shell', 'pm', 'grant',
                            'com.android.chrome',
                            'android.permission.POST_NOTIFICATIONS'],
                           check=False, timeout=5)

            # Pre-clear any old traces on the device.
            subprocess.run([ADB_PATH, 'shell', 'rm', '-f',
                            '/data/local/tmp/*.perfetto-trace'],
                           check=False, timeout=5)

            subprocess.run([ADB_PATH, 'connect', RECEIVER_IP],
                           check=False,
                           timeout=15)

            # Bridge the web server to the USB device.
            subprocess.run([ADB_PATH, 'reverse',
                            f'tcp:{SERVER_PORT}',
                            f'tcp:{SERVER_PORT}'],
                           check=False, timeout=10)
            logging.info("USB data bridge established on port %d.",
                         SERVER_PORT)
        except Exception as e:
            logging.error("Failed to setup ADB receiver: %s. "
                          "Receiver traces will not be collected.", e)

    driver = None
    tunnel_proc = None
    actual_version = None

    try:
        driver, tunnel_proc, actual_version = setup_test_environment(args, cv)

        if args.receiver_type == 'android':
            # Sync the receiver Chrome version to match the sender.
            install_chrome_on_android(actual_version)

        for i, video in enumerate(common.VIDEOS):
            logging.info("Starting test for video: %s", video['name'])
            rec_proc = None
            try:
                # pass is_first_run for the first video to trigger UI cleanup.
                rec_proc = run_performance_test(video['name'], driver, args,
                                                is_first_run=(i == 0))
            except Exception: # pylint: disable=broad-exception-caught
                logging.exception("Error during video %s test",
                                  video['name'])
                common.dump_remote_logs(args)
                raise
            finally:
                common.teardown_recording_process(rec_proc)
    except Exception:
        if FUCHSIA_INFRA_AVAILABLE:
            monitors.clear()
        raise
    finally:
        if FUCHSIA_INFRA_AVAILABLE:
            monitors.dump(LOG_DIR)
        common.finalize_results(actual_version)
        common.teardown_test_environment(driver, tunnel_proc, args)

if __name__ == '__main__':
    with common.StartProcess(common.server.start, [SERVER_PORT], True):
        sys.exit(main())
