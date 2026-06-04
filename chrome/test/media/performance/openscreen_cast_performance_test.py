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
CAST_URL = (f"http://{common.LOCAL_HOST_IP}:{common.SERVER_PORT}/"
            "cast_starter.html?flavor=stable")
# This is set by the fleet team as the IP for all receiver devices.
RECEIVER_IP = "172.16.0.3:5555"
RECEIVER_TRACE_DIR = "/data/misc/perfetto-traces"

ADB_PATH = '/opt/infra-android/tools/platform-tools/adb'
if not os.path.exists(ADB_PATH):
    ADB_PATH = 'adb'

CHROME_OPTIONS = [
    # Redirects logging output to file.
    "--enable-logging",
    # Allows casting to devices with public IPs, which is necessary for our
    # current lab setup.
    "--media-router-cast-allow-all-ips",
    # Sets the default verbose logging level to 1.
    "--v=1",
    # Verbose levels for routing, casting, mirroring, and openscreen.
    "--vmodule=media_router*=3,discovery_mdns*=3,*cast*=3,"
    "webrtc_logging*=3,*mirroring*=3,*openscreen*=3",
    # Force-enable stats collection loop and RTCP reporting on startup.
    "--enable-features=EnableRtcpReporting",
    # Skips the first-run experience modal.
    "--no-first-run",
    # Prevents the "Set as default browser" prompt from appearing.
    "--no-default-browser-check",
    # Launches Chrome in fullscreen mode to prevent scrollbar clipping.
    "--start-fullscreen",
    # Enables WebRTC event logging extensions.
    "--enable-webrtc-event-logging-extensions",
]

PERFETTO_CONFIG = """
buffers: {
    size_kb: 65536
    fill_policy: DISCARD
}
data_sources: {
    config {
        name: "linux.ftrace"
        ftrace_config {
            ftrace_events: "sched/sched_switch"
            ftrace_events: "sched/sched_wakeup"
            ftrace_events: "power/cpu_frequency"
            ftrace_events: "power/cpu_idle"
        }
    }
}
data_sources: {
    config {
        name: "android.track_event"
    }
}
duration_ms: 40000
"""


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
        tuple: A tuple containing the WebDriver, the tunnel process, and the
               actual chrome version used.
    """
    if args.sender_os == 'cros':
        driver, cb_platform, actual_version = common.setup_cros_environment(
            args, chrome_version, CHROME_OPTIONS)
        enable_tab_mirroring(driver)
        return driver, cb_platform, actual_version

    common.terminate_old_chromedriver(args)
    remote_app_path, actual_version = common.install_and_setup_chrome(
        args, chrome_version)
    common.wait_for_chromedriver(args)
    tunnel_proc = common.start_ssh_tunnel(args)

    chrome_options = ChromeOptions()
    for option in CHROME_OPTIONS:
        chrome_options.add_argument(option)

    # Dynamically set the --log-file path.
    log_file_path = (
        f'{common.WIN_REMOTE_TMP_DIR}/chrome_debug.log'
        if args.sender_os == 'win' else '/tmp/chrome_debug.log')
    chrome_options.add_argument(f'--log-file={log_file_path}')

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

    return driver, tunnel_proc, actual_version


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
def run_performance_test(video_file: str, driver: webdriver, args):
    """
    Runs a single video performance test by casting and recording the video.

    This function navigates to the video player page, enables Cast discovery,
    starts an ffmpeg recording process, initiates tab mirroring, plays a video,
    and then analyzes the recorded output for performance metrics.

    Args:
        video_file (str): The name of the video file to be tested.
        driver (webdriver.Remote): The Selenium WebDriver instance.
        args: The parsed command-line arguments.

    Returns:
        subprocess.Popen: The Popen object for the ffmpeg recording process.
    """
    # force video output to mp4
    output_file = os.path.join(common.RECORDINGS_DIR,
                               video_file.replace('.webm', '.mp4'))
    receiver_trace_remote_path = (
        f'{RECEIVER_TRACE_DIR}/{video_file}.perfetto-trace')

    host_recording_cmd = [
        'ffmpeg',
        # Overwrite output files without asking.
        '-y',
        # Set the input format to Video4Linux2.
        '-f', 'video4linux2',
        # Set the input pixel format.
        '-input_format', 'yuyv422',
        # Specify the input file (video device).
        '-i', '/dev/video0',
        # Set the size of the input buffer to help prevent dropped frames.
        '-thread_queue_size', '1024',
        # Set the video codec to libx264 (H.264).
        '-c:v', 'libx264',
        # Use the ultrafast preset for real-time encoding.
        '-preset', 'ultrafast',
        # Set the Constant Rate Factor for quality (lower is better).
        '-crf', '28',
        # Set the output pixel format for compatibility.
        '-pix_fmt', 'yuv420p',
        # Set the Group of Pictures (GOP) size for better seeking.
        '-g', '60',
        # Set the duration of the recording.
        '-t', '35',
        output_file
    ]

    wait = WebDriverWait(driver, 30)
    driver.get(f'http://{common.LOCAL_HOST_IP}:'
               f'{common.SERVER_PORT}/video.html?file={video_file}')
    wait.until(ec.presence_of_element_located((By.ID, "video")))

    # TODO(b/506206539): Refactor injected logging code into functions.
    # Best-effort reset of tracing state in case a previous run leaked it.
    try:
        reset_resp = driver.execute_cdp_cmd('Tracing.end', {})
        reset_stream = reset_resp.get('stream')
        if reset_stream:
            driver.execute_cdp_cmd('IO.close', {'handle': reset_stream})
    except Exception: # pylint: disable=broad-exception-caught
        pass

    casting = False
    rec_proc_local = None
    receiver_trace_proc = None
    sender_tracing_started = False
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

        logging.info("Starting Sender Perfetto trace via CDP...")
        # Attempt to start sender trace with retries to handle state collision.
        for attempt in range(3):
            try:
                driver.execute_cdp_cmd('Tracing.start', {
                    'categories': 'cast,media,webrtc,gpu,blink',
                    'transferMode': 'ReturnAsStream'
                })
                sender_tracing_started = True
                break
            except Exception as e:
                if "Tracing has already been started" in str(e) and attempt < 2:
                    logging.warning(
                        "Tracing collision detected. Attempting reset and "
                        "retry...")
                    try:
                        reset_resp = driver.execute_cdp_cmd('Tracing.end', {})
                        reset_stream = reset_resp.get('stream')
                        if reset_stream:
                            driver.execute_cdp_cmd('IO.close',
                                                   {'handle': reset_stream})
                    except Exception: # pylint: disable=broad-exception-caught
                        pass
                    # Wait significantly longer for the browser state to clear.
                    time.sleep(10)
                else:
                    raise

        logging.info("Starting Receiver Perfetto trace via ADB...")
        try:
            # pylint: disable=consider-using-with
            # Pre-clean the remote trace path to ensure fresh data.
            subprocess.run([
                ADB_PATH, '-s', RECEIVER_IP, 'shell', 'rm', '-f',
                receiver_trace_remote_path
            ],
                           check=False,
                           timeout=5)
            receiver_trace_proc = subprocess.Popen([
                ADB_PATH, '-s', RECEIVER_IP, 'shell', 'perfetto',
                '-c', '-', '--txt', '-o', receiver_trace_remote_path
            ],
                                                   stdin=subprocess.PIPE,
                                                   stdout=subprocess.PIPE,
                                                   stderr=subprocess.PIPE,
                                                   text=True)
            receiver_trace_proc.stdin.write(PERFETTO_CONFIG)
            receiver_trace_proc.stdin.close()
        except Exception as e:
            logging.error("Failed to start receiver trace: %s", e)

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
                # Capture video element state for better root-causing.
                v_state = driver.execute_script(
                    "return { "
                    "  error: arguments[0].error ? "
                    "arguments[0].error.code : 'none', "
                    "  networkState: arguments[0].networkState, "
                    "  readyState: arguments[0].readyState, "
                    "  src: arguments[0].src "
                    "}", video)
                logging.warning(
                    '%s failed to load within timeout. Skipping. State: %s',
                    video_file, v_state)
                common.measures.average(video_file, 'video_perf', 'playback',
                                 'failed_to_load').record(1)
                # Gracefully stop the recording process since we're skipping.
                rec_proc_local.terminate()
                rec_proc_local.wait()
                return None

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

        original_video = f"/usr/local/cipd/videostack_videos_30s/{video_file}"
        common.calculate_psnr_ssim(video_file, output_file, original_video)

        logging.warning('Video analysis result of %s: %s', video_file, results)
    finally:
        if driver and casting and args.receiver:
            logging.info('Attempting to stop casting to "%s"...',
                         args.receiver)
            try:
                driver.execute_cdp_cmd("Cast.stopCasting",
                                       {"sinkName": args.receiver})
                logging.info("'Cast.stopCasting' command sent.")
                casting = False
                # Wait for the sink to disconnect for stable logging.
                time.sleep(10)
            except Exception as e:
                logging.error("Error stopping cast: %s", e)

        if driver and sender_tracing_started:
            try:
                logging.info("Stopping Sender Trace...")
                tracing_end_resp = driver.execute_cdp_cmd('Tracing.end', {})
                stream_id = tracing_end_resp.get('stream')
                if stream_id:
                    sender_trace_path = os.path.join(
                        common.TRACES_DIR,
                        f"{video_file}_sender.chrome-trace")
                    try:
                        with open(
                                sender_trace_path, 'w',
                                encoding='utf-8') as f:
                            while True:
                                read_resp = driver.execute_cdp_cmd(
                                    'IO.read', {'handle': stream_id})
                                f.write(read_resp.get('data', ''))
                                if read_resp.get('eof'):
                                    break
                    finally:
                        driver.execute_cdp_cmd(
                            'IO.close', {'handle': stream_id})
                        logging.info("Sender trace closed.")
            except Exception as e:
                logging.error("Failed to collect/stop sender trace: %s", e)

        # Collect the Receiver Trace
        try:
            if receiver_trace_proc:
                logging.info(
                    "Waiting for Receiver Perfetto trace to finish...")
                try:
                    # Perfetto runs for 40 seconds. We've slept for 30s +
                    # some overhead, so it should finish within 25s.
                    receiver_trace_proc.wait(timeout=25)
                    logging.info("Receiver Perfetto trace finished.")
                    stdout_data = receiver_trace_proc.stdout.read()
                    stderr_data = receiver_trace_proc.stderr.read()
                    if receiver_trace_proc.returncode != 0:
                        logging.error(
                            "Receiver Perfetto trace failed with code %d. "
                            "Stderr: %s",
                            receiver_trace_proc.returncode, stderr_data)
                except subprocess.TimeoutExpired:
                    logging.warning(
                        "Receiver Perfetto trace did not finish within "
                        "timeout. Terminating...")
                    receiver_trace_proc.terminate()
                    receiver_trace_proc.wait()
                    stdout_data = receiver_trace_proc.stdout.read()
                    stderr_data = receiver_trace_proc.stderr.read()
                    logging.error(
                        "Receiver Perfetto trace timed out. Stderr: %s",
                        stderr_data)

                logging.info("Collecting Receiver Trace...")
                receiver_trace_local_path = os.path.join(
                    common.TRACES_DIR,
                    f"{video_file}_receiver.perfetto-trace")
                subprocess.run([
                    ADB_PATH, '-s', RECEIVER_IP, 'pull',
                    receiver_trace_remote_path, receiver_trace_local_path
                ],
                               check=False,
                               timeout=30)
        except Exception as e:
            logging.error("Failed to collect receiver trace: %s", e)

        # Collect Receiver Logcat
        try:
            logging.info("Collecting Receiver Logcat...")
            logcat_path = os.path.join(
                common.TRACES_DIR, f"{video_file}_receiver_logcat.txt")
            with open(logcat_path, 'w', encoding='utf-8') as f:
                subprocess.run(
                    [ADB_PATH, '-s', RECEIVER_IP, 'logcat', '-d'],
                    stdout=f,
                    check=False,
                    timeout=30)
            subprocess.run([ADB_PATH, '-s', RECEIVER_IP, 'logcat', '-c'],
                           check=False,
                           timeout=30)
        except Exception as e:
            logging.error("Failed to collect receiver logcat: %s", e)

        # Collect the Sender Chrome Log
        try:
            logging.info("Collecting Sender Chrome Log...")
            if args.sender_os == 'win':
                # Use standard Windows path with forward slashes for scp.
                log_file_path = (
                    f'{common.WIN_REMOTE_TMP_DIR}/chrome_debug.log'
                    )
            else:
                log_file_path = '/tmp/chrome_debug.log'
            sender_log_local_path = os.path.join(
                common.TRACES_DIR, f"{video_file}_chrome_debug.log")
            key_path = os.path.expanduser('~/.ssh/id_ed25519')
            subprocess.run([
                'scp', '-i', key_path, '-o', 'StrictHostKeyChecking=no',
                f'{args.username}@{args.sender}:{log_file_path}',
                sender_log_local_path
            ],
                           check=False,
                           timeout=30)
        except Exception as e:
            logging.error("Failed to collect sender log: %s", e)
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
    parser.add_argument('--sender-os',
                        choices=['mac', 'win', 'linux', 'cros'],
                        help='OS of the sender device.')
    args, _ = parser.parse_known_args()
    cv = args.chrome_version

    if os.path.exists(common.RECORDINGS_DIR):
        shutil.rmtree(common.RECORDINGS_DIR)
    os.makedirs(common.RECORDINGS_DIR)
    if os.path.exists(common.TRACES_DIR):
        shutil.rmtree(common.TRACES_DIR)
    os.makedirs(common.TRACES_DIR)

    driver = None
    tunnel_proc = None
    actual_version = None

    # Connect to the ADB receiver.
    try:
        subprocess.run([ADB_PATH, 'connect', RECEIVER_IP],
                       check=False,
                       timeout=15)
        # Attempt to get ADB root.
        logging.info("Attempting to get ADB root...")
        res_root = subprocess.run([ADB_PATH, '-s', RECEIVER_IP, 'root'],
                                  capture_output=True, text=True,
                                  check=False, timeout=10)
        if res_root.returncode != 0:
            logging.error("ADB root failed: %s", res_root.stderr.strip())

        # Reconnect after root.
        res_conn = subprocess.run([ADB_PATH, 'connect', RECEIVER_IP],
                                  capture_output=True, text=True,
                                  check=False, timeout=15)
        if res_conn.returncode != 0:
            logging.error("ADB reconnect failed: %s", res_conn.stderr.strip())

        # Ensure the tracing daemons are enabled and started.
        res_enable = subprocess.run([
            ADB_PATH, '-s', RECEIVER_IP, 'shell', 'setprop',
            'persist.traced.enable', '1'
        ],
                                    capture_output=True, text=True,
                                    check=False, timeout=5)
        if res_enable.returncode != 0:
            logging.error("Failed to enable tracing daemons: %s",
                          res_enable.stderr.strip())

        # Ensure the trace directory exists and is writable.
        res_mkdir = subprocess.run([
            ADB_PATH, '-s', RECEIVER_IP, 'shell', 'mkdir', '-p',
            RECEIVER_TRACE_DIR
        ],
                                   capture_output=True, text=True,
                                   check=False, timeout=5)
        if res_mkdir.returncode != 0:
            logging.error("Failed to create trace directory: %s",
                          res_mkdir.stderr.strip())

        res_chmod = subprocess.run([
            ADB_PATH, '-s', RECEIVER_IP, 'shell', 'chmod', '777',
            RECEIVER_TRACE_DIR
        ],
                                   capture_output=True, text=True,
                                   check=False, timeout=5)
        if res_chmod.returncode != 0:
            logging.error("Failed to chmod trace directory: %s",
                          res_chmod.stderr.strip())

        # Restore SELinux context on the newly created directory.
        res_restorecon = subprocess.run([
            ADB_PATH, '-s', RECEIVER_IP, 'shell', 'restorecon', '-R',
            RECEIVER_TRACE_DIR
        ],
                                        capture_output=True, text=True,
                                        check=False, timeout=5)
        if res_restorecon.returncode != 0:
            logging.error("Failed to restorecon trace directory: %s",
                          res_restorecon.stderr.strip())
    except Exception as e:
        logging.error(
            "Failed to setup ADB receiver: %s. "
            "Receiver traces will not be collected.", e)

    try:
        driver, tunnel_proc, actual_version = setup_test_environment(args, cv)
        for video in common.VIDEOS:
            # TODO(b/512198717): Enable HEVC tests on ChromeOS.
            # Currently these tests are rendering a blank white screen, so we
            # skip them to bring up the other cros tests.
            if args.sender_os == 'cros' and 'HEVC' in video['name']:
                logging.info("Skipping HEVC on ChromeOS: %s", video['name'])
                continue
            logging.info("Starting test for video: %s", video['name'])
            rec_proc = None
            try:
                rec_proc = run_performance_test(video['name'], driver, args)
                if rec_proc is None:
                    logging.warning("Video %s was skipped.", video['name'])
                    continue
            except Exception: # pylint: disable=broad-exception-caught
                logging.exception("Error during video %s test", video['name'])
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
